#include "App.h"
#include "CorralWindow.h"
#include "DesktopIcons.h"
#include "DesktopMonitor.h"
#include <CommCtrl.h>
#include <cstdlib>
#include <algorithm>

App* App::instance = nullptr;

static const wchar_t* MESSAGE_WINDOW_CLASS = L"DexCorralMessageWindow";

App::App() : messageWindow(nullptr) {
    instance = this;
}

App::~App() {
    Shutdown();
    instance = nullptr;
}

void App::Initialize() {
    // Register message window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MessageWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = MESSAGE_WINDOW_CLASS;
    RegisterClassExW(&wc);

    // Create hidden message window
    messageWindow = CreateWindowExW(
        0,
        MESSAGE_WINDOW_CLASS,
        L"DexCorral Message Window",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr,
        GetModuleHandleW(nullptr), this
    );

    // Load configuration
    LoadConfig();

    // Create wallpaper manager
    wallpaperManager = std::make_unique<WallpaperManager>();
    wallpaperManager->LoadWallpaper();

    // Setup mouse hook
    mouseHook = std::make_unique<MouseHook>();
    mouseHook->SetLeftButtonDownCallback([this](POINT pt) { OnLeftButtonDown(pt); });
    mouseHook->SetLeftButtonUpCallback([this](POINT pt) { OnLeftButtonUp(pt); });
    mouseHook->SetMouseMoveCallback([this](POINT pt) { OnMouseMove(pt); });
    mouseHook->Start();

    // Create tray icon
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    trayIcon = std::make_unique<TrayIcon>(messageWindow, icon, L"DexCorral - Double-click desktop to hide icons");

    // Restore corrals
    RestoreCorrals();

    // Restore desktop icons state
    // Default to true if not specified (backward compatibility)
    DesktopIcons::SetIconsVisible(config.DesktopIconsVisible);

    if (corrals.empty()) {
        // First run: create a default catch-all corral at center of screen
        CorralConfig defaultConfig;
        defaultConfig.Left = GetSystemMetrics(SM_CXSCREEN) / 2 - 150;
        defaultConfig.Top = GetSystemMetrics(SM_CYSCREEN) / 2 - 100;
        defaultConfig.Width = 300;
        defaultConfig.Height = 200;
        defaultConfig.Title = "Desktop";
        defaultConfig.IsCatchAll = true;  // First corral is catch-all

        auto corral = std::make_unique<CorralWindow>(defaultConfig, wallpaperManager.get());
        corral->Show();
        corrals.push_back(std::move(corral));
        SaveConfig();
    }

    // Ensure exactly one catch-all corral exists
    EnsureCatchAllCorral();

    // Start desktop monitoring
    desktopMonitor = std::make_unique<DesktopMonitor>();
    desktopMonitor->SetFileAddedCallback([this](const std::wstring& fileName) { OnDesktopFileAdded(fileName); });
    desktopMonitor->SetFileRenamedCallback([this](const std::wstring& oldName, const std::wstring& newName) { OnDesktopFileRenamed(oldName, newName); });
    desktopMonitor->SetFileDeletedCallback([this](const std::wstring& fileName) { OnDesktopFileDeleted(fileName); });
    desktopMonitor->Start();
}

void App::Shutdown() {
    // Stop desktop monitor first
    if (desktopMonitor) {
        desktopMonitor->Stop();
    }

    // Save config before exit
    SaveConfig();

    // IMPORTANT: Show desktop icons before exit so user isn't stuck
    DesktopIcons::SetIconsVisible(true);

    // Clean up corrals
    corrals.clear();

    // Stop mouse hook
    if (mouseHook) {
        mouseHook->Stop();
    }
}

void App::LoadConfig() {
    config = Config::Load();
}

void App::SaveConfig() {
    // Update desktop icons state
    config.DesktopIconsVisible = DesktopIcons::AreIconsVisible();

    // Sync all corral configs from their current window states
    config.Corrals.clear();
    for (auto& corral : corrals) {
        corral->SyncConfigFromWindow();
        config.Corrals.push_back(corral->GetConfig());
    }
    Config::Save(config);
}

void App::RestoreCorrals() {
    for (auto& corralConfig : config.Corrals) {
        // Skip corrals with invalid dimensions (corrupted config)
        if (corralConfig.Width < 50 || corralConfig.Height < 50) {
            corralConfig.Width = 300;
            corralConfig.Height = 200;
        }
        if (corralConfig.Left < -1000 || corralConfig.Top < -1000) {
            corralConfig.Left = GetSystemMetrics(SM_CXSCREEN) / 2 - 150;
            corralConfig.Top = GetSystemMetrics(SM_CYSCREEN) / 2 - 100;
        }

        auto corral = std::make_unique<CorralWindow>(corralConfig, wallpaperManager.get());
        corral->Show();
        corrals.push_back(std::move(corral));
    }
}

void App::RemoveCorral(CorralConfig* configToRemove) {
    for (auto it = corrals.begin(); it != corrals.end(); ++it) {
        if (&(*it)->GetConfig() == configToRemove) {
            corrals.erase(it);
            break;
        }
    }
    SaveConfig();
}

void App::RemoveFileFromOtherCorrals(const std::wstring& fileName, CorralConfig* exceptCorral) {
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    bool changed = false;
    for (auto& corral : corrals) {
        auto& corralConfig = corral->GetConfig();
        if (&corralConfig != exceptCorral) {
            auto it = std::find(corralConfig.Files.begin(), corralConfig.Files.end(), fileNameStr);
            if (it != corralConfig.Files.end()) {
                corralConfig.Files.erase(it);
                changed = true;
            }
        }
    }

    if (changed) {
        SaveConfig();
        RefreshAllCorrals();
    }
}

void App::RefreshAllCorrals() {
    for (auto& corral : corrals) {
        corral->LoadFiles();
    }
}

void App::RefreshAllCorralBackgrounds() {
    wallpaperManager->LoadWallpaper();
    for (auto& corral : corrals) {
        corral->UpdateWallpaperBackground();
    }
}

void App::SetDefaultColorHex(const std::string& colorHex) {
    config.DefaultColorHex = colorHex;
}

void App::ApplyColorToAllCorrals(const std::string& colorHex) {
    for (auto& corral : corrals) {
        corral->GetConfig().ColorHex = colorHex;
        corral->UpdateWallpaperBackground();
    }
}

void App::ToggleDesktopIcons() {
    bool currentlyVisible = DesktopIcons::AreIconsVisible();
    DesktopIcons::SetIconsVisible(!currentlyVisible);
    SaveConfig();
}

void App::ToggleShortcutArrows() {
    bool currentlyHidden = DesktopIcons::AreShortcutArrowsHidden();
    bool newState = !currentlyHidden;

    // Confirm with user since this requires Explorer restart
    const wchar_t* message = newState
        ? L"This will hide shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?"
        : L"This will restore shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?";

    int result = MessageBoxW(nullptr, message, L"DexCorral", MB_YESNO | MB_ICONQUESTION);
    if (result != IDYES) {
        return;
    }

    if (DesktopIcons::SetShortcutArrowsHidden(newState)) {
        config.HideShortcutArrows = newState;
        SaveConfig();
        DesktopIcons::RestartExplorer();
    } else {
        MessageBoxW(nullptr, L"Failed to change shortcut arrow setting.\n\nThis may require administrator privileges.", L"DexCorral", MB_OK | MB_ICONWARNING);
    }
}

void App::OnLeftButtonDown(POINT pt) {
    // Reserved for future desktop interactions
    // Currently no action on desktop click
}

void App::OnLeftButtonUp(POINT pt) {
    // Nothing to do - drag-to-create removed
}

void App::OnMouseMove(POINT pt) {
    // Handle mouse move if needed
}

void App::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Create New Corral");

    // Icon visibility toggle
    UINT flags = DesktopIcons::AreIconsVisible() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | flags, 2, L"Show Desktop Icons");

    // Autostart toggle
    UINT autostartFlags = IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | autostartFlags, 4, L"Start with Windows");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Exit");

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(messageWindow);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, messageWindow, nullptr);
    PostMessageW(messageWindow, WM_NULL, 0, 0);

    DestroyMenu(menu);

    switch (cmd) {
    case 1: {
        int offsetX = (int)corrals.size() * 30;
        int offsetY = (int)corrals.size() * 30;
        POINT centerPt = {
            GetSystemMetrics(SM_CXSCREEN) / 2 + offsetX,
            GetSystemMetrics(SM_CYSCREEN) / 2 + offsetY
        };
        ShowCreationMenu(centerPt);
        break;
    }
    case 2:
        ToggleDesktopIcons();
        break;
    case 3:
        PostQuitMessage(0);
        break;
    case 4:
        SetAutostart(!IsAutostartEnabled());
        break;
    }
}

void App::ShowCreationMenu(POINT pt) {
    CreateCorral(pt);
}

void App::CreateCorral(POINT pt) {
    CorralConfig newConfig;

    // Create at fixed default size centered on point
    newConfig.Left = (double)pt.x - 150;
    newConfig.Top = (double)pt.y - 100;
    newConfig.Width = 300;
    newConfig.Height = 200;
    newConfig.Title = "New Corral";
    newConfig.ColorHex = config.DefaultColorHex;  // Use saved default appearance

    auto corral = std::make_unique<CorralWindow>(newConfig, wallpaperManager.get());
    corral->Show();
    corrals.push_back(std::move(corral));
    SaveConfig();
}

bool App::IsDesktopUnderMouse(POINT pt) {
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return false;

    wchar_t className[256];
    while (hwnd) {
        GetClassNameW(hwnd, className, 256);
        std::wstring classStr(className);

        if (classStr == L"Progman" || classStr == L"WorkerW") {
            return true;
        }

        HWND shellWindow = GetShellWindow();
        if (hwnd == shellWindow) {
            return true;
        }

        hwnd = GetParent(hwnd);
    }

    return false;
}

bool App::IsAutostartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        DWORD type = REG_SZ;
        LONG result = RegQueryValueExW(hKey, L"DexCorral", nullptr, &type, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    return false;
}

void App::SetAutostart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            RegSetValueExW(hKey, L"DexCorral", 0, REG_SZ, (const BYTE*)exePath, (DWORD)(wcslen(exePath) + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"DexCorral");
        }
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK App::MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    App* app = nullptr;

    if (uMsg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        app = (App*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else {
        app = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (app && uMsg == TrayIcon::WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            app->ShowTrayMenu();
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK) {
            app->ToggleDesktopIcons();
            return 0;
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int App::Run() {
    Initialize();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

void App::EnsureCatchAllCorral() {
    // Find if any corral is marked as catch-all
    bool foundCatchAll = false;
    for (auto& corral : corrals) {
        if (corral->GetConfig().IsCatchAll) {
            if (foundCatchAll) {
                // Only one catch-all allowed
                corral->GetConfig().IsCatchAll = false;
            }
            foundCatchAll = true;
        }
    }

    // If no catch-all exists, make the first corral catch-all
    if (!foundCatchAll && !corrals.empty()) {
        corrals[0]->GetConfig().IsCatchAll = true;
        corrals[0]->UpdateWallpaperBackground();  // Refresh display to show catch-all symbol
    }
}

CorralWindow* App::GetCatchAllCorral() {
    for (auto& corral : corrals) {
        if (corral->GetConfig().IsCatchAll) {
            return corral.get();
        }
    }
    // Fallback to first corral if no catch-all defined
    return corrals.empty() ? nullptr : corrals[0].get();
}

void App::OnDesktopFileAdded(const std::wstring& fileName) {
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    // Check if file is already in any corral
    for (auto& corral : corrals) {
        auto& files = corral->GetConfig().Files;
        if (std::find(files.begin(), files.end(), fileNameStr) != files.end()) {
            return;  // Already tracked
        }
    }

    // Add to catch-all corral
    CorralWindow* catchAll = GetCatchAllCorral();
    if (catchAll) {
        catchAll->AddFile(fileNameStr);
        SaveConfig();
    }
}

void App::OnDesktopFileRenamed(const std::wstring& oldName, const std::wstring& newName) {
    // Convert to UTF-8
    int oldSize = WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string oldNameStr(oldSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, &oldNameStr[0], oldSize, nullptr, nullptr);

    int newSize = WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string newNameStr(newSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, &newNameStr[0], newSize, nullptr, nullptr);

    // Update in all corrals
    bool changed = false;
    for (auto& corral : corrals) {
        auto& files = corral->GetConfig().Files;
        auto it = std::find(files.begin(), files.end(), oldNameStr);
        if (it != files.end()) {
            *it = newNameStr;
            corral->LoadFiles();
            changed = true;
        }
    }

    if (changed) {
        SaveConfig();
    }
}

void App::OnDesktopFileDeleted(const std::wstring& fileName) {
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    // Remove from all corrals
    bool changed = false;
    for (auto& corral : corrals) {
        auto& files = corral->GetConfig().Files;
        auto it = std::find(files.begin(), files.end(), fileNameStr);
        if (it != files.end()) {
            files.erase(it);
            corral->LoadFiles();
            changed = true;
        }
    }

    if (changed) {
        SaveConfig();
    }
}
