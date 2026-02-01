#include "CorralWindow.h"
#include "App.h"
#include "WallpaperManager.h"
#include "DesktopIcons.h"
#include "FolderWatcher.h"
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cmath>

#pragma comment(lib, "msimg32.lib")

static const wchar_t* CORRAL_WINDOW_CLASS = L"DexCorralWindowClass";

// Cloud file attributes (defined in Windows 10 SDK 1709+, provide fallbacks)
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif
#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED 0x00080000
#endif
#ifndef FILE_ATTRIBUTE_UNPINNED
#define FILE_ATTRIBUTE_UNPINNED 0x00100000
#endif

// ============================================================================
// Folder browser utility functions
// ============================================================================

static std::wstring BrowseForLocalFolder(HWND hwndOwner, const wchar_t* title) {
    std::wstring result;

    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        hr = pfd->GetOptions(&dwOptions);
        if (SUCCEEDED(hr)) {
            hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }

        if (SUCCEEDED(hr)) {
            pfd->SetTitle(title);
        }

        hr = pfd->Show(hwndOwner);
        if (SUCCEEDED(hr)) {
            IShellItem* psi = nullptr;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr)) {
                    result = pszPath;
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    return result;
}

static bool ValidateLocalFolder(const std::wstring& path, std::wstring& errorMsg) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        errorMsg = L"The specified folder does not exist.";
        return false;
    }

    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        errorMsg = L"The specified path is not a folder.";
        return false;
    }

    // Check for network path (starts with \\)
    if (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        errorMsg = L"Network paths are not supported. Please select a local folder.";
        return false;
    }

    // Check if it's a network drive
    if (path.length() >= 2 && path[1] == L':') {
        wchar_t rootPath[4] = { path[0], L':', L'\\', L'\0' };
        UINT driveType = GetDriveTypeW(rootPath);
        if (driveType == DRIVE_REMOTE) {
            errorMsg = L"Network drives are not supported. Please select a local folder.";
            return false;
        }
    }

    return true;
}

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

CorralWindow::CorralWindow(const CorralConfig& cfg, WallpaperManager* wallpaperMgr)
    : config(cfg), wallpaperManager(wallpaperMgr), isDragging(false), hwnd(nullptr) {

    dragStart = { 0, 0 };
    dragStartRect = { 0, 0, 0, 0 };

    // Save the full height for roll-up restore
    savedHeight = config.Height;

    // Get icon size and spacing based on view mode
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = CORRAL_WINDOW_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.style = CS_DBLCLKS;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    std::wstring wtitle = Utf8ToWide(config.Title);

    // If rolled up, create with title bar height; otherwise use full height
    int initialHeight = config.IsRolledUp ? TITLE_BAR_HEIGHT : (int)config.Height;

    hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES | WS_EX_LAYERED,
        CORRAL_WINDOW_CLASS,
        wtitle.c_str(),
        WS_POPUP | WS_VISIBLE,
        (int)config.Left, (int)config.Top,
        (int)config.Width, initialHeight,
        nullptr, nullptr, GetModuleHandleW(nullptr), this
    );

    // Sync config from actual window position/size (in case Windows adjusted it)
    SyncConfigFromWindow();

    // Initialize folder watcher for virtual corrals
    if (config.IsVirtual) {
        InitializeFolderWatcher();
    }
}

CorralWindow::~CorralWindow() {
    // Stop folder watcher
    if (folderWatcher) {
        folderWatcher->Stop();
        folderWatcher.reset();
    }
    ClearIcons();
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

// ============================================================================
// Static helpers
// ============================================================================

int CorralWindow::GetDesktopIconSize() {
    // Read icon size from registry
    // HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\Shell\Bags\1\Desktop\IconSize
    HKEY hKey;
    DWORD iconSize = 48;  // Default medium icons

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\Shell\\Bags\\1\\Desktop",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        DWORD size = sizeof(DWORD);
        DWORD type = REG_DWORD;
        RegQueryValueExW(hKey, L"IconSize", nullptr, &type, (LPBYTE)&iconSize, &size);
        RegCloseKey(hKey);
    }

    // Clamp to reasonable values
    if (iconSize < 16) iconSize = 16;
    if (iconSize > 256) iconSize = 256;

    return (int)iconSize;
}

void CorralWindow::GetDesktopIconSpacing(int& spacingX, int& spacingY) {
    // Read icon spacing from registry
    // HKEY_CURRENT_USER\Control Panel\Desktop\WindowMetrics
    // IconSpacing and IconVerticalSpacing are in twips (negative values)
    HKEY hKey;

    // Default spacing based on icon size
    spacingX = iconSize + 40;
    spacingY = iconSize + 40;

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Control Panel\\Desktop\\WindowMetrics",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        wchar_t buffer[32];
        DWORD bufferSize = sizeof(buffer);
        DWORD type = REG_SZ;

        // IconSpacing (horizontal)
        if (RegQueryValueExW(hKey, L"IconSpacing", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            int value = _wtoi(buffer);
            if (value < 0) value = -value;  // Stored as negative
            // Convert from twips to pixels (approx: value is already in a usable range for newer Windows)
            // Actually on modern Windows this is typically stored as -1125 to -1920 range
            // The actual pixel spacing = abs(value) * DPI / 96 / 15 (approximately)
            // Simplified: just use a reasonable calculation
            if (value > 0) {
                // The value divided by about 15 gives reasonable pixel spacing
                spacingX = value / 15;
                if (spacingX < iconSize + 20) spacingX = iconSize + 20;
                if (spacingX > iconSize + 80) spacingX = iconSize + 80;
            }
        }

        bufferSize = sizeof(buffer);
        // IconVerticalSpacing (vertical)
        if (RegQueryValueExW(hKey, L"IconVerticalSpacing", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            int value = _wtoi(buffer);
            if (value < 0) value = -value;
            if (value > 0) {
                spacingY = value / 15;
                if (spacingY < iconSize + 20) spacingY = iconSize + 20;
                if (spacingY > iconSize + 80) spacingY = iconSize + 80;
            }
        }

        RegCloseKey(hKey);
    }
}

std::wstring CorralWindow::GetDesktopPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path))) {
        return std::wstring(path);
    }
    return L"";
}

std::wstring CorralWindow::GetPublicDesktopPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, path))) {
        return std::wstring(path);
    }
    return L"";
}

// ============================================================================
// Public methods
// ============================================================================

void CorralWindow::Show() {
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        // Set to bottom z-order (above desktop, below other apps)
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        if (config.IsVirtual) {
            // For virtual corrals, show window immediately and load icons asynchronously
            UpdateLayeredContent();
            PostMessageW(hwnd, WM_DEFERRED_LOAD, 0, 0);
        } else {
            LoadFiles();  // This calls UpdateLayeredContent
        }
    }
}

void CorralWindow::Hide() {
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

void CorralWindow::UpdateWallpaperBackground() {
    if (hwnd) {
        UpdateLayeredContent();
    }
}

void CorralWindow::SyncConfigFromWindow() {
    if (hwnd) {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        config.Left = rect.left;
        config.Top = rect.top;
        config.Width = rect.right - rect.left;
        // When rolled up, preserve the saved full height in config
        if (!config.IsRolledUp) {
            config.Height = rect.bottom - rect.top;
            savedHeight = config.Height;
        }
        // When rolled up, config.Height keeps the savedHeight value

        // Update monitor position tracking
        App* app = App::GetInstance();
        if (app && app->GetMonitorManager()) {
            MonitorManager* monMgr = app->GetMonitorManager();
            const MonitorInfo* mon = monMgr->FindMonitorForRect(rect);
            if (mon) {
                // Update target monitor ID
                config.TargetMonitorId = mon->deviceId;

                // Store position for this monitor (relative to monitor origin)
                MonitorPosition& pos = config.MonitorPositions[mon->deviceId];
                pos.Left = (int)config.Left - mon->bounds.left;
                pos.Top = (int)config.Top - mon->bounds.top;
                pos.Width = (int)config.Width;
                pos.Height = (int)(config.IsRolledUp ? savedHeight : config.Height);
                pos.RefWidth = mon->width;
                pos.RefHeight = mon->height;
            }
        }
    }
}

void CorralWindow::LoadFiles() {
    if (config.IsVirtual) {
        LoadVirtualFolderIcons();
    } else {
        LoadIconImages();
    }
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::AddFile(const std::string& fileName) {
    // Virtual corrals don't accept manual file additions
    if (config.IsVirtual) return;

    // Check if already in this corral
    auto it = std::find(config.Files.begin(), config.Files.end(), fileName);
    if (it == config.Files.end()) {
        config.Files.push_back(fileName);
        LoadFiles();
    }
}

// ============================================================================
// Icon loading and layout
// ============================================================================

void CorralWindow::ClearIcons() {
    for (auto& icon : icons) {
        if (icon.hIcon) {
            DestroyIcon(icon.hIcon);
            icon.hIcon = nullptr;
        }
        if (icon.hIconSmall) {
            DestroyIcon(icon.hIconSmall);
            icon.hIconSmall = nullptr;
        }
    }
    icons.clear();
    selectedIcon = -1;
}

void CorralWindow::LoadFileDetails(CorralIcon& icon) {
    // Get file type description
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(icon.fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_TYPENAME)) {
        icon.fileType = sfi.szTypeName;
    }

    // Get file size and modified time
    WIN32_FILE_ATTRIBUTE_DATA fileData = {};
    if (GetFileAttributesExW(icon.fullPath.c_str(), GetFileExInfoStandard, &fileData)) {
        icon.fileSize = ((ULONGLONG)fileData.nFileSizeHigh << 32) | fileData.nFileSizeLow;
        icon.modifiedTime = fileData.ftLastWriteTime;
    }

    // Get sync status
    icon.syncStatus = GetSyncStatus(icon.fullPath);
}

SyncStatus CorralWindow::GetSyncStatus(const std::wstring& path) {
    // Detect cloud files by their attributes and reparse tags, not by path names.
    // This works for OneDrive, Dropbox, Google Drive, iCloud, and other cloud providers.

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return SyncStatus::None;
    }

    // Check Windows cloud file attributes (most reliable method)
    // These attributes are set by cloud sync providers like OneDrive

    // Cloud-only files: not locally available, will download on access
    if (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS ||
        attrs & FILE_ATTRIBUTE_RECALL_ON_OPEN ||
        attrs & FILE_ATTRIBUTE_UNPINNED) {
        return SyncStatus::CloudOnly;
    }

    // Pinned files: "Always keep on this device" - fully synced locally
    if (attrs & FILE_ATTRIBUTE_PINNED) {
        return SyncStatus::Synced;
    }

    // Check for cloud reparse point (OneDrive uses 0x9000xxxx tags)
    // Files synced by OneDrive have reparse points even when locally available
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(path.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            // OneDrive cloud tags: 0x9000001A through 0x9000F01A
            // Check if high word matches 0x9000 (cloud files filter)
            DWORD tag = findData.dwReserved0;
            if ((tag & 0xFFFF0000) == 0x90000000) {
                return SyncStatus::Synced;
            }
        }
    }

    // Not a cloud-managed file - show nothing
    return SyncStatus::None;
}

void CorralWindow::LoadIconImages() {
    ClearIcons();

    std::wstring desktopPath = GetDesktopPath();
    std::wstring publicDesktopPath = GetPublicDesktopPath();

    // Determine which icon flag to use based on view mode
    iconSize = GetIconSizeForViewMode();
    UINT iconFlag = (iconSize <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    bool isDetailsView = (config.GetViewMode() == ViewMode::Details);

    for (const auto& fileName : config.Files) {
        CorralIcon ci;
        ci.fileName = fileName;
        ci.wFileName = std::wstring(fileName.begin(), fileName.end());

        // Create display name (hide .lnk extension like Windows does)
        ci.displayName = ci.wFileName;
        if (ci.displayName.length() > 4) {
            std::wstring ext = ci.displayName.substr(ci.displayName.length() - 4);
            // Case-insensitive comparison for .lnk
            if (ext == L".lnk" || ext == L".LNK" || ext == L".Lnk") {
                ci.displayName = ci.displayName.substr(0, ci.displayName.length() - 4);
            }
        }

        // Try user desktop first, then public desktop
        std::wstring userPath = desktopPath + L"\\" + ci.wFileName;
        std::wstring pubPath = publicDesktopPath + L"\\" + ci.wFileName;

        if (GetFileAttributesW(userPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ci.fullPath = userPath;
        }
        else if (GetFileAttributesW(pubPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ci.fullPath = pubPath;
        }
        else {
            ci.fullPath = userPath;
        }

        // Load the shell icon
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi),
            SHGFI_ICON | iconFlag)) {
            ci.hIcon = sfi.hIcon;
        } else {
            // Fallback: Try to get icon by file extension instead of actual file
            // This helps with corrupted icon cache or permission issues
            DWORD fileAttribs = GetFileAttributesW(ci.fullPath.c_str());
            if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
                fileAttribs = FILE_ATTRIBUTE_NORMAL;
            }
            if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                ci.hIcon = sfi.hIcon;
            }
        }

        // Always load small icon for details view
        if (isDetailsView || iconSize > 16) {
            SHFILEINFOW sfiSmall = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfiSmall, sizeof(sfiSmall),
                SHGFI_ICON | SHGFI_SMALLICON)) {
                ci.hIconSmall = sfiSmall.hIcon;
            } else {
                // Fallback for small icon as well
                DWORD fileAttribs = GetFileAttributesW(ci.fullPath.c_str());
                if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
                    fileAttribs = FILE_ATTRIBUTE_NORMAL;
                }
                if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfiSmall, sizeof(sfiSmall),
                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    ci.hIconSmall = sfiSmall.hIcon;
                }
            }
        }

        // Load file details for details view
        if (isDetailsView) {
            LoadFileDetails(ci);
        }

        icons.push_back(std::move(ci));
    }
}

void CorralWindow::LoadVirtualFolderIcons() {
    ClearIcons();

    if (config.VirtualFolderPath.empty()) return;

    std::wstring folderPath = Utf8ToWide(config.VirtualFolderPath);

    // Enumerate folder contents
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = folderPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    iconSize = GetIconSizeForViewMode();
    UINT iconFlag = (iconSize <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    bool isDetailsView = (config.GetViewMode() == ViewMode::Details);

    do {
        // Skip . and ..
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        // Skip hidden files
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            continue;
        }

        CorralIcon ci;
        ci.wFileName = findData.cFileName;
        ci.fileName = WideToUtf8(ci.wFileName);
        ci.displayName = ci.wFileName;
        ci.fullPath = folderPath + L"\\" + ci.wFileName;

        // Hide .lnk extension like Windows does
        if (ci.displayName.length() > 4) {
            std::wstring ext = ci.displayName.substr(ci.displayName.length() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".lnk") {
                ci.displayName = ci.displayName.substr(0, ci.displayName.length() - 4);
            }
        }

        // Load shell icon
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | iconFlag)) {
            ci.hIcon = sfi.hIcon;
        } else {
            // Fallback: Try to get icon by file extension instead of actual file
            // This helps with corrupted icon cache or permission issues
            DWORD fileAttribs = findData.dwFileAttributes;
            if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                ci.hIcon = sfi.hIcon;
            }
        }

        // Load small icon for details view
        if (isDetailsView || iconSize > 16) {
            SHFILEINFOW sfiSmall = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfiSmall, sizeof(sfiSmall),
                              SHGFI_ICON | SHGFI_SMALLICON)) {
                ci.hIconSmall = sfiSmall.hIcon;
            } else {
                // Fallback for small icon as well
                DWORD fileAttribs = findData.dwFileAttributes;
                if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfiSmall, sizeof(sfiSmall),
                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    ci.hIconSmall = sfiSmall.hIcon;
                }
            }
        }

        if (isDetailsView) {
            LoadFileDetails(ci);
        }

        icons.push_back(std::move(ci));
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

void CorralWindow::InitializeFolderWatcher() {
    if (!config.IsVirtual || config.VirtualFolderPath.empty()) return;

    folderWatcher = std::make_unique<FolderWatcher>();
    std::wstring folderPath = Utf8ToWide(config.VirtualFolderPath);
    folderWatcher->SetPath(folderPath);
    folderWatcher->SetChangeCallback([this]() {
        // Post message to window to handle on main thread
        if (hwnd) {
            PostMessageW(hwnd, WM_FOLDER_CHANGED, 0, 0);
        }
    });
    folderWatcher->Start();
}

void CorralWindow::OnFolderContentsChanged() {
    LoadFiles();
}

void CorralWindow::ChangeFolderPath() {
    if (!config.IsVirtual) return;

    std::wstring newPath = BrowseForLocalFolder(hwnd, L"Select Folder for Virtual Corral");
    if (newPath.empty()) return;

    std::wstring errorMsg;
    if (!ValidateLocalFolder(newPath, errorMsg)) {
        MessageBoxW(hwnd, errorMsg.c_str(), L"Invalid Folder", MB_OK | MB_ICONWARNING);
        return;
    }

    // Stop existing watcher
    if (folderWatcher) {
        folderWatcher->Stop();
        folderWatcher.reset();
    }

    // Update config
    config.VirtualFolderPath = WideToUtf8(newPath);

    // Update title to folder name
    size_t lastSlash = newPath.find_last_of(L"\\/");
    std::wstring folderName = (lastSlash != std::wstring::npos) ?
                              newPath.substr(lastSlash + 1) : newPath;
    config.Title = WideToUtf8(folderName);
    SetWindowTextW(hwnd, folderName.c_str());

    // Restart watcher with new path
    InitializeFolderWatcher();

    // Reload icons
    LoadFiles();

    // Save config
    if (App::GetInstance()) {
        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::CalculateIconLayout() {
    if (config.GetViewMode() == ViewMode::Details) {
        CalculateIconLayoutDetails();
    } else {
        CalculateIconLayoutGrid();
    }
    // Clamp scroll position after layout change
    ClampScrollPosition();
}

void CorralWindow::CalculateIconLayoutGrid() {
    int x = ICON_PADDING_LEFT;
    int y = ICON_AREA_TOP;
    int clientWidth = (int)config.Width;

    // Reserve space for scrollbar if needed (will be recalculated)
    int rightPadding = ICON_PADDING_LEFT;

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        int iconImgX = x + (iconSpacingX - iconSize) / 2;
        int iconImgY = y;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
        icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

        x += iconSpacingX;
        if (x + iconSpacingX > clientWidth - rightPadding) {
            x = ICON_PADDING_LEFT;
            y += iconSpacingY;
        }
    }

    // Calculate total content height
    if (!icons.empty()) {
        int lastIconBottom = icons.back().rect.bottom;
        contentHeight = lastIconBottom + ICON_PADDING_LEFT;
    } else {
        contentHeight = ICON_AREA_TOP;
    }

    // If we need a scrollbar, recalculate with scrollbar space
    if (NeedsScrollbar() && rightPadding == ICON_PADDING_LEFT) {
        rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;

        x = ICON_PADDING_LEFT;
        y = ICON_AREA_TOP;

        for (size_t i = 0; i < icons.size(); i++) {
            auto& icon = icons[i];

            int iconImgX = x + (iconSpacingX - iconSize) / 2;
            int iconImgY = y;

            icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
            icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

            x += iconSpacingX;
            if (x + iconSpacingX > clientWidth - rightPadding) {
                x = ICON_PADDING_LEFT;
                y += iconSpacingY;
            }
        }

        // Recalculate content height
        if (!icons.empty()) {
            int lastIconBottom = icons.back().rect.bottom;
            contentHeight = lastIconBottom + ICON_PADDING_LEFT;
        }
    }
}

void CorralWindow::CalculateIconLayoutDetails() {
    // Details view: list layout with rows
    // Each row has: [icon 16px] [name] [type] [size] [date] [sync status]
    int y = ICON_AREA_TOP;
    int clientWidth = (int)config.Width;
    int rightPadding = ICON_PADDING_LEFT;

    // Check if we'll need scrollbar
    int estimatedHeight = ICON_AREA_TOP + (int)icons.size() * DETAILS_ROW_HEIGHT + ICON_PADDING_LEFT;
    int visibleHeight = (int)config.Height - ICON_AREA_TOP;
    if (estimatedHeight > visibleHeight) {
        rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;
    }

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        // Icon is at the left of each row
        int iconImgX = ICON_PADDING_LEFT + 2;
        int iconImgY = y + (DETAILS_ROW_HEIGHT - ICON_SIZE_DETAILS) / 2;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + ICON_SIZE_DETAILS, iconImgY + ICON_SIZE_DETAILS };
        // Full row rect for selection and hit testing
        icon.rect = { ICON_PADDING_LEFT, y, clientWidth - rightPadding, y + DETAILS_ROW_HEIGHT };

        y += DETAILS_ROW_HEIGHT;
    }

    // Calculate total content height
    if (!icons.empty()) {
        contentHeight = y + ICON_PADDING_LEFT;
    } else {
        contentHeight = ICON_AREA_TOP;
    }
}

int CorralWindow::HitTestIcon(int x, int y) {
    // Adjust for scroll offset
    int adjustedY = y + scrollPosition;
    POINT pt = { x, adjustedY };

    for (int i = 0; i < (int)icons.size(); i++) {
        if (PtInRect(&icons[i].rect, pt)) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Icon rename support
// ============================================================================

RECT CorralWindow::GetIconLabelRect(int iconIndex) const {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) {
        return { 0, 0, 0, 0 };
    }

    const CorralIcon& icon = icons[iconIndex];

    // For details view, label is in the name column
    if (config.GetViewMode() == ViewMode::Details) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int clientWidth = rect.right - rect.left;
        int rightPadding = ICON_PADDING_LEFT;
        if (NeedsScrollbar()) {
            rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;
        }
        int contentWidth = clientWidth - ICON_PADDING_LEFT - rightPadding;

        int nameCol = icon.iconRect.left + ICON_SIZE_DETAILS + 4;
        int typeCol = nameCol + (int)(contentWidth * 0.40);

        return { nameCol, icon.rect.top, typeCol - 4, icon.rect.bottom };
    }

    // For icon views, label is below the icon
    int labelTop = icon.iconRect.bottom + 2;
    return { icon.rect.left, labelTop, icon.rect.right, icon.rect.bottom };
}

bool CorralWindow::HitTestIconLabel(int x, int y, int iconIndex) const {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) {
        return false;
    }

    // Get label rect in content coordinates and convert to screen coordinates
    RECT labelRect = GetIconLabelRect(iconIndex);
    labelRect.top -= scrollPosition;
    labelRect.bottom -= scrollPosition;

    POINT pt = { x, y };
    return PtInRect(&labelRect, pt) != FALSE;
}

void CorralWindow::StartIconRename(int iconIndex) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size() || isRenamingIcon) {
        return;
    }

    isRenamingIcon = true;
    renamingIconIndex = iconIndex;
    originalName = icons[iconIndex].displayName;

    // Get the label rect in client coordinates
    RECT labelRect = GetIconLabelRect(iconIndex);

    // Adjust for scroll position
    labelRect.top -= scrollPosition;
    labelRect.bottom -= scrollPosition;

    // Temporarily disable layered window style so child controls render properly
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);

    // Create edit control as a normal child window
    hEditControl = CreateWindowExW(
        0,
        L"EDIT",
        icons[iconIndex].displayName.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER,
        labelRect.left,
        labelRect.top,
        labelRect.right - labelRect.left,
        labelRect.bottom - labelRect.top,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (hEditControl) {
        // Set font to match the label
        HFONT hFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SendMessageW(hEditControl, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Select all text
        SendMessageW(hEditControl, EM_SETSEL, 0, -1);

        // Set focus
        SetFocus(hEditControl);

        // Subclass the edit control to handle Enter/Escape
        SetWindowSubclass(hEditControl, EditSubclassProc, 0, (DWORD_PTR)this);
    }
}

void CorralWindow::EndIconRename(bool save) {
    if (!isRenamingIcon || !hEditControl) {
        return;
    }

    if (save && renamingIconIndex >= 0 && renamingIconIndex < (int)icons.size()) {
        // Get the new name from edit control
        int len = GetWindowTextLengthW(hEditControl);
        if (len > 0) {
            std::wstring newName(len + 1, L'\0');
            GetWindowTextW(hEditControl, &newName[0], len + 1);
            newName.resize(len);

            // Trim whitespace
            size_t start = newName.find_first_not_of(L" \t\r\n");
            size_t end = newName.find_last_not_of(L" \t\r\n");
            if (start != std::wstring::npos && end != std::wstring::npos) {
                newName = newName.substr(start, end - start + 1);
            }

            // Only rename if name actually changed and is not empty
            if (!newName.empty() && newName != originalName) {
                // Get the original file path
                std::wstring oldPath = icons[renamingIconIndex].fullPath;

                // Build new path
                size_t lastSlash = oldPath.find_last_of(L"\\");
                std::wstring newPath;
                if (lastSlash != std::wstring::npos) {
                    newPath = oldPath.substr(0, lastSlash + 1) + newName;

                    // Add .lnk extension if it's a shortcut and not already present
                    bool oldPathIsLnk = oldPath.length() >= 4 && oldPath.substr(oldPath.length() - 4) == L".lnk";
                    bool newNameIsLnk = newName.length() >= 4 && newName.substr(newName.length() - 4) == L".lnk";
                    if (oldPathIsLnk && !newNameIsLnk) {
                        newPath += L".lnk";
                    }
                }

                // Attempt to rename the file
                if (!newPath.empty() && MoveFileW(oldPath.c_str(), newPath.c_str())) {
                    // Update the icon data
                    icons[renamingIconIndex].fullPath = newPath;
                    icons[renamingIconIndex].wFileName = newPath.substr(lastSlash + 1);

                    // Update display name (without .lnk)
                    std::wstring displayName = icons[renamingIconIndex].wFileName;
                    bool displayNameIsLnk = displayName.length() >= 4 && displayName.substr(displayName.length() - 4) == L".lnk";
                    if (displayNameIsLnk) {
                        displayName = displayName.substr(0, displayName.length() - 4);
                    }
                    icons[renamingIconIndex].displayName = displayName;

                    // Update config
                    if (renamingIconIndex < (int)config.Files.size()) {
                        config.Files[renamingIconIndex] = std::string(icons[renamingIconIndex].wFileName.begin(),
                                                                       icons[renamingIconIndex].wFileName.end());
                    }

                    if (App::GetInstance()) {
                        App::GetInstance()->SaveConfig();
                    }
                } else {
                    // Rename failed - could show error message
                    MessageBoxW(hwnd, L"Failed to rename file. The file may be in use or you may not have permission.",
                               L"Rename Error", MB_OK | MB_ICONERROR);
                }
            }
        }
    }

    // Clean up edit control
    // Note: Set state flags BEFORE DestroyWindow to prevent re-entrancy
    // (DestroyWindow triggers WM_KILLFOCUS which would call EndIconRename again)
    HWND editToDestroy = hEditControl;
    hEditControl = nullptr;
    isRenamingIcon = false;
    renamingIconIndex = -1;
    originalName.clear();

    if (editToDestroy) {
        DestroyWindow(editToDestroy);
    }

    // Restore layered window style (was disabled for child edit control to work)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

    // Redraw to show the updated label
    UpdateLayeredContent();
}

LRESULT CALLBACK CorralWindow::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    CorralWindow* window = (CorralWindow*)dwRefData;

    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            // Enter - save and end rename
            window->EndIconRename(true);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            // Escape - cancel rename
            window->EndIconRename(false);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        // Lost focus - save rename
        window->EndIconRename(true);
        return 0;

    case WM_NCDESTROY:
        // Remove subclass before destruction
        RemoveWindowSubclass(hwnd, EditSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Scrollbar support
// ============================================================================

bool CorralWindow::NeedsScrollbar() const {
    return contentHeight > GetVisibleHeight();
}

int CorralWindow::GetContentHeight() const {
    return contentHeight;
}

int CorralWindow::GetVisibleHeight() const {
    RECT rect;
    GetClientRect(hwnd, &rect);
    return rect.bottom - ICON_AREA_TOP;
}

RECT CorralWindow::GetScrollbarTrackRect() const {
    RECT rect;
    GetClientRect(hwnd, &rect);
    return {
        rect.right - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN,
        ICON_AREA_TOP + SCROLLBAR_MARGIN,
        rect.right - SCROLLBAR_MARGIN,
        rect.bottom - SCROLLBAR_MARGIN
    };
}

RECT CorralWindow::GetScrollbarThumbRect() const {
    if (!NeedsScrollbar()) return { 0, 0, 0, 0 };

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();

    // Thumb size proportional to visible/content ratio
    int thumbHeight = (std::max)(SCROLLBAR_MIN_THUMB,
        (int)((float)visibleHeight / contentHeight * trackHeight));

    // Thumb position based on scroll position
    int scrollRange = contentHeight - visibleHeight;
    int thumbRange = trackHeight - thumbHeight;
    int thumbTop = track.top;
    if (scrollRange > 0) {
        thumbTop = track.top + (int)((float)scrollPosition / scrollRange * thumbRange);
    }

    return {
        track.left,
        thumbTop,
        track.right,
        thumbTop + thumbHeight
    };
}

bool CorralWindow::HitTestScrollbar(int x, int y) const {
    if (!NeedsScrollbar()) return false;
    RECT track = GetScrollbarTrackRect();
    POINT pt = { x, y };
    return PtInRect(&track, pt) != FALSE;
}

bool CorralWindow::HitTestScrollbarThumb(int x, int y) const {
    if (!NeedsScrollbar()) return false;
    RECT thumb = GetScrollbarThumbRect();
    POINT pt = { x, y };
    return PtInRect(&thumb, pt) != FALSE;
}

void CorralWindow::OnMouseWheel(int delta) {
    // Scroll 3 lines per notch (WHEEL_DELTA = 120)
    int scrollAmount = (delta / WHEEL_DELTA) * iconSpacingY;
    scrollPosition -= scrollAmount;
    ClampScrollPosition();
    UpdateLayeredContent();
}

void CorralWindow::StartScrollbarDrag(int y) {
    isDraggingScrollbar = true;
    scrollbarDragStartY = y;
    scrollbarDragStartPos = scrollPosition;
    SetCapture(hwnd);
}

void CorralWindow::DoScrollbarDrag(int y) {
    if (!isDraggingScrollbar) return;

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();
    int thumbHeight = (std::max)(SCROLLBAR_MIN_THUMB,
        (int)((float)visibleHeight / contentHeight * trackHeight));
    int thumbRange = trackHeight - thumbHeight;
    int scrollRange = contentHeight - visibleHeight;

    if (thumbRange > 0 && scrollRange > 0) {
        int deltaY = y - scrollbarDragStartY;
        int newScrollPos = scrollbarDragStartPos + (int)((float)deltaY / thumbRange * scrollRange);
        scrollPosition = newScrollPos;
        ClampScrollPosition();
        UpdateLayeredContent();
    }
}

void CorralWindow::EndScrollbarDrag() {
    if (isDraggingScrollbar) {
        isDraggingScrollbar = false;
        ReleaseCapture();
    }
}

void CorralWindow::ClampScrollPosition() {
    int maxScroll = contentHeight - GetVisibleHeight();
    if (maxScroll < 0) maxScroll = 0;
    if (scrollPosition < 0) scrollPosition = 0;
    if (scrollPosition > maxScroll) scrollPosition = maxScroll;
}

// ============================================================================
// Resize support
// ============================================================================

int CorralWindow::HitTestResize(int x, int y) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    bool nearRight = (x >= rect.right - RESIZE_BORDER);
    bool nearBottom = (y >= rect.bottom - RESIZE_BORDER);

    if (nearRight && nearBottom) return HTBOTTOMRIGHT;
    if (nearRight) return HTRIGHT;
    if (nearBottom) return HTBOTTOM;

    return 0;
}

void CorralWindow::StartResize(int hitTest, int x, int y) {
    isResizing = true;
    resizeMode = hitTest;
    GetCursorPos(&resizeStart);
    GetWindowRect(hwnd, &resizeStartRect);
    SetCapture(hwnd);
}

void CorralWindow::DoResize(int screenX, int screenY) {
    if (!isResizing) return;

    int dx = screenX - resizeStart.x;
    int dy = screenY - resizeStart.y;

    int newWidth = resizeStartRect.right - resizeStartRect.left;
    int newHeight = resizeStartRect.bottom - resizeStartRect.top;

    if (resizeMode == HTRIGHT || resizeMode == HTBOTTOMRIGHT) {
        newWidth += dx;
    }
    if (resizeMode == HTBOTTOM || resizeMode == HTBOTTOMRIGHT) {
        newHeight += dy;
    }

    // Minimum size
    if (newWidth < 100) newWidth = 100;
    if (newHeight < 80) newHeight = 80;

    // Apply snap to resize unless Shift is held
    if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
        ApplyResizeSnap(newWidth, newHeight);
    }

    SetWindowPos(hwnd, nullptr, 0, 0, newWidth, newHeight,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CorralWindow::EndResize() {
    if (isResizing) {
        isResizing = false;
        ReleaseCapture();
        SyncConfigFromWindow();
        CalculateIconLayout();
        UpdateLayeredContent();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// File operations
// ============================================================================

void CorralWindow::OpenFile(int iconIndex) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) return;

    const auto& icon = icons[iconIndex];
    ShellExecuteW(hwnd, L"open", icon.fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CorralWindow::ShowShellContextMenu(int iconIndex, int screenX, int screenY) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) return;

    const auto& icon = icons[iconIndex];

    LPITEMIDLIST pidlFull = nullptr;
    HRESULT hr = SHParseDisplayName(icon.fullPath.c_str(), nullptr, &pidlFull, 0, nullptr);
    if (FAILED(hr) || !pidlFull) return;

    IShellFolder* pParentFolder = nullptr;
    LPCITEMIDLIST pidlChild = nullptr;
    hr = SHBindToParent(pidlFull, IID_IShellFolder, (void**)&pParentFolder, &pidlChild);
    if (FAILED(hr) || !pParentFolder) {
        CoTaskMemFree(pidlFull);
        return;
    }

    IContextMenu* pContextMenu = nullptr;
    hr = pParentFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, nullptr, (void**)&pContextMenu);
    if (FAILED(hr) || !pContextMenu) {
        pParentFolder->Release();
        CoTaskMemFree(pidlFull);
        return;
    }

    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        hr = pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL | CMF_EXPLORE);
        if (SUCCEEDED(hr)) {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 0x7FFF + 1, L"Remove from Corral");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                screenX, screenY, 0, hwnd, nullptr);
            PostMessageW(hwnd, WM_NULL, 0, 0);

            if (cmd == 0x7FFF + 1) {
                auto it = std::find(config.Files.begin(), config.Files.end(), icon.fileName);
                if (it != config.Files.end()) {
                    config.Files.erase(it);
                    LoadFiles();
                    if (App::GetInstance()) {
                        App::GetInstance()->SaveConfig();
                    }
                }
            }
            else if (cmd > 0) {
                CMINVOKECOMMANDINFO ci = {};
                ci.cbSize = sizeof(ci);
                ci.hwnd = hwnd;
                ci.lpVerb = MAKEINTRESOURCEA(cmd - 1);
                ci.nShow = SW_SHOWNORMAL;
                pContextMenu->InvokeCommand(&ci);
            }
        }
        DestroyMenu(hMenu);
    }

    pContextMenu->Release();
    pParentFolder->Release();
    CoTaskMemFree(pidlFull);
}

// ============================================================================
// Window procedure
// ============================================================================

LRESULT CALLBACK CorralWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    CorralWindow* window = nullptr;

    if (uMsg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        window = (CorralWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    else {
        window = (CorralWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (window) {
        switch (uMsg) {
        case WM_PAINT:
            window->OnPaint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            window->OnSize();
            return 0;
        case WM_MOVE:
            window->OnMove();
            return 0;
        case WM_ACTIVATE:
            // Push back to bottom z-order when activated (keeps corral below other apps)
            // Skip this if we're renaming an icon to avoid z-order fighting with the edit popup
            if (!window->isRenamingIcon) {
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                int hit = window->HitTestResize(pt.x, pt.y);
                if (hit == HTBOTTOMRIGHT) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
                    return TRUE;
                }
                else if (hit == HTRIGHT) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
                else if (hit == HTBOTTOM) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                    return TRUE;
                }
            }
            break;
        }
        case WM_LBUTTONDOWN:
            window->OnLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONDBLCLK:
            window->OnLeftButtonDblClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE: {
            // Track mouse for WM_MOUSELEAVE
            if (!window->mouseInsideWindow) {
                window->mouseInsideWindow = true;
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);

                // Trigger hover expand if rolled up
                if (window->config.IsRolledUp && !window->isHoverExpanded && !window->isAnimating) {
                    window->StartHoverExpand();
                }
            }

            if (window->isResizing) {
                POINT pt;
                GetCursorPos(&pt);
                window->DoResize(pt.x, pt.y);
            }
            else if (window->isDraggingScrollbar) {
                window->DoScrollbarDrag(GET_Y_LPARAM(lParam));
            }
            else if (window->isDraggingIcon) {
                // Icon reordering drag
                window->OnIconDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            else if (window->draggedIconIndex >= 0) {
                // Potential drag - check if mouse moved beyond threshold
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int dx = x - window->iconDragStart.x;
                int dy = y - window->iconDragStart.y;
                int distance = (int)sqrt(dx * dx + dy * dy);

                if (distance > window->DRAG_THRESHOLD) {
                    // Start actual drag
                    window->isDraggingIcon = true;
                    window->UpdateLayeredContent();
                }
            }
            else if (window->isDragging) {
                POINT pt;
                GetCursorPos(&pt);
                int dx = pt.x - window->dragStart.x;
                int dy = pt.y - window->dragStart.y;

                // Use dragStartRect for stable reference (rubberbanding)
                int newLeft = window->dragStartRect.left + dx;
                int newTop = window->dragStartRect.top + dy;
                int width = window->dragStartRect.right - window->dragStartRect.left;
                int height = window->dragStartRect.bottom - window->dragStartRect.top;

                // Apply snap unless Shift is held (pixel-perfect positioning)
                if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                    window->ApplySnap(newLeft, newTop, width, height);
                }

                MoveWindow(hwnd, newLeft, newTop, width, height, TRUE);
                // Note: We don't update dragStart here to allow pulling out of snaps
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (window->isResizing) {
                window->EndResize();
            }
            else if (window->isDraggingScrollbar) {
                window->EndScrollbarDrag();
            }
            else if (window->isDraggingIcon) {
                window->OnIconDragEnd();
                ReleaseCapture();
            }
            else if (window->draggedIconIndex >= 0) {
                // Mouse up without dragging - just a selection click
                window->draggedIconIndex = -1;
                ReleaseCapture();
            }
            else if (window->isDragging) {
                window->isDragging = false;
                ReleaseCapture();
                window->SyncConfigFromWindow();
                if (App::GetInstance()) {
                    App::GetInstance()->SaveConfig();
                }
            }
            return 0;
        case WM_RBUTTONDOWN:
            window->OnRightButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_DROPFILES:
            window->OnDropFiles((HDROP)wParam);
            return 0;
        case WM_MOUSEWHEEL:
            window->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_F2 && window->selectedIcon >= 0 &&
                window->selectedIcon < (int)window->icons.size()) {
                // F2 - Rename selected icon
                window->StartIconRename(window->selectedIcon);
                return 0;
            }
            else if (wParam == VK_DELETE && window->selectedIcon >= 0 &&
                window->selectedIcon < (int)window->config.Files.size()) {
                // Remove selected icon from corral
                window->config.Files.erase(window->config.Files.begin() + window->selectedIcon);
                window->selectedIcon = -1;
                window->LoadFiles();
                if (App::GetInstance()) {
                    App::GetInstance()->SaveConfig();
                }
                return 0;
            }
            break;
        case WM_TIMER:
            if (wParam == ANIMATION_TIMER_ID) {
                window->OnAnimationTimer();
                return 0;
            }
            else if (wParam == HOVER_CHECK_TIMER_ID) {
                window->OnHoverCheckTimer();
                return 0;
            }
            break;
        case WM_FOLDER_CHANGED:
            // Virtual folder contents changed - reload icons
            window->OnFolderContentsChanged();
            return 0;
        case WM_DEFERRED_LOAD:
            // Deferred icon loading for virtual corrals (keeps UI responsive)
            window->LoadFiles();
            return 0;
        case WM_MOUSELEAVE:
            window->mouseInsideWindow = false;
            // Start collapse if hover-expanded
            if (window->isHoverExpanded && !window->isAnimating) {
                window->StartHoverCollapse();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Paint - Uses UpdateLayeredWindow for true per-pixel transparency
// ============================================================================

void CorralWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // Check if we're currently in non-layered mode (during rename)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) {
        // Paint a solid background when not layered
        RECT rect;
        GetClientRect(hwnd, &rect);

        // Use the config background color
        BYTE bgR = 40, bgG = 40, bgB = 40;
        if (!config.ColorHex.empty() && config.ColorHex[0] == '#' && config.ColorHex.length() >= 7) {
            unsigned int colorValue;
            sscanf_s(config.ColorHex.c_str() + 1, "%x", &colorValue);
            bgR = (colorValue >> 16) & 0xFF;
            bgG = (colorValue >> 8) & 0xFF;
            bgB = colorValue & 0xFF;
        }

        HBRUSH brush = CreateSolidBrush(RGB(bgR, bgG, bgB));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        // Draw title bar background (darker)
        HBRUSH titleBrush = CreateSolidBrush(RGB(bgR / 2, bgG / 2, bgB / 2));
        RECT titleBarRect = { 0, 0, rect.right, TITLE_BAR_HEIGHT };
        FillRect(hdc, &titleBarRect, titleBrush);
        DeleteObject(titleBrush);

        // Draw title text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT titleFont = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
        std::wstring wtitle = Utf8ToWide(config.Title);
        RECT titleRect = { 8, 8, rect.right - 8, 30 };
        DrawTextW(hdc, wtitle.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);

        // Draw icons
        HFONT labelFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SelectObject(hdc, labelFont);

        for (size_t i = 0; i < icons.size(); i++) {
            const auto& icon = icons[i];
            int drawTop = icon.iconRect.top - scrollPosition;

            // Skip if outside visible area
            if (drawTop + iconSize < ICON_AREA_TOP || drawTop > rect.bottom) continue;

            // Draw icon
            if (icon.hIcon) {
                DrawIconEx(hdc, icon.iconRect.left, drawTop, icon.hIcon,
                    iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }

            // Draw label (skip the one being edited)
            if ((int)i != renamingIconIndex) {
                RECT labelRect = {
                    icon.rect.left,
                    icon.iconRect.bottom + 2 - scrollPosition,
                    icon.rect.right,
                    icon.rect.bottom - scrollPosition
                };
                DrawTextW(hdc, icon.displayName.c_str(), -1, &labelRect,
                    DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
            }
        }

        DeleteObject(labelFont);
    }

    EndPaint(hwnd, &ps);

    // For layered windows, we use UpdateLayeredWindow instead of regular painting
    if (exStyle & WS_EX_LAYERED) {
        UpdateLayeredContent();
    }
}

void CorralWindow::UpdateLayeredContent() {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) return;

    // Create a 32-bit DIB for per-pixel alpha
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    void* bits = nullptr;
    HBITMAP memBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!memBitmap || !bits) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
    DWORD* pixels = (DWORD*)bits;

    // Get overlay color and alpha from config
    BYTE bgAlpha = 153;  // Default ~60% opacity
    BYTE bgR = 0, bgG = 0, bgB = 0;

    if (!config.ColorHex.empty() && config.ColorHex[0] == '#' && config.ColorHex.length() >= 9) {
        unsigned int colorValue;
        sscanf_s(config.ColorHex.c_str() + 1, "%x", &colorValue);
        bgAlpha = (colorValue >> 24) & 0xFF;
        bgR = (colorValue >> 16) & 0xFF;
        bgG = (colorValue >> 8) & 0xFF;
        bgB = colorValue & 0xFF;
    }

    // Premultiply colors (required for per-pixel alpha blending)
    BYTE pmR = (BYTE)((bgR * bgAlpha) / 255);
    BYTE pmG = (BYTE)((bgG * bgAlpha) / 255);
    BYTE pmB = (BYTE)((bgB * bgAlpha) / 255);
    DWORD bgPixel = (bgAlpha << 24) | (pmR << 16) | (pmG << 8) | pmB;

    // Fill background with semi-transparent color
    for (int i = 0; i < w * h; i++) {
        pixels[i] = bgPixel;
    }

    // Title bar - darker overlay
    BYTE titleAlpha = 220;
    BYTE titlePmR = (BYTE)((bgR * titleAlpha) / 255 / 2);  // Darker
    BYTE titlePmG = (BYTE)((bgG * titleAlpha) / 255 / 2);
    BYTE titlePmB = (BYTE)((bgB * titleAlpha) / 255 / 2);
    DWORD titlePixel = (titleAlpha << 24) | (titlePmR << 16) | (titlePmG << 8) | titlePmB;

    for (int y = 0; y < TITLE_BAR_HEIGHT && y < h; y++) {
        for (int x = 0; x < w; x++) {
            pixels[y * w + x] = titlePixel;
        }
    }

    // Draw border (1px solid line at full opacity)
    DWORD borderPixel = (255 << 24) | (100 << 16) | (100 << 8) | 100;
    // Top edge
    for (int x = 0; x < w; x++) pixels[x] = borderPixel;
    // Bottom edge
    for (int x = 0; x < w; x++) pixels[(h - 1) * w + x] = borderPixel;
    // Left edge
    for (int y = 0; y < h; y++) pixels[y * w] = borderPixel;
    // Right edge
    for (int y = 0; y < h; y++) pixels[y * w + (w - 1)] = borderPixel;

    // Now use GDI to draw content (text, icons) on top
    // GDI doesn't handle alpha properly, so we draw and then fix alpha

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    // Draw title (with catch-all symbol if applicable)
    HFONT titleFont = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(memDC, titleFont);

    std::wstring wtitle = Utf8ToWide(config.Title);

    // Add folder symbol for virtual corrals
    if (config.IsVirtual) {
        wtitle = L"\U0001F4C1 " + wtitle;  // Folder emoji (📁)
    }
    // Add catch-all symbol if this is the catch-all corral
    else if (config.IsCatchAll) {
        wtitle = L"\u2B07 " + wtitle;  // Down arrow (⬇) symbol
    }

    RECT titleRect = { 10, 6, w - 10, 28 };
    DrawTextW(memDC, wtitle.c_str(), (int)wtitle.length(), &titleRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(memDC, oldFont);
    DeleteObject(titleFont);

    // Fix alpha for title area (GDI sets alpha to 0)
    for (int y = 0; y < TITLE_BAR_HEIGHT && y < h; y++) {
        for (int x = 0; x < w; x++) {
            DWORD pixel = pixels[y * w + x];
            BYTE r = (pixel >> 16) & 0xFF;
            BYTE g = (pixel >> 8) & 0xFF;
            BYTE b = pixel & 0xFF;
            // If GDI drew here (non-background color), make it fully opaque
            if (r > titlePmR || g > titlePmG || b > titlePmB) {
                pixels[y * w + x] = (255 << 24) | (r << 16) | (g << 8) | b;
            } else {
                pixels[y * w + x] = titlePixel;
            }
        }
    }

    // Draw icons (skip when rolled up, but show when hover-expanded)
    if (!icons.empty() && (!config.IsRolledUp || isHoverExpanded)) {
        HFONT iconFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldIconFont = (HFONT)SelectObject(memDC, iconFont);

        // Visible area for clipping (icon area only)
        int visibleTop = ICON_AREA_TOP;
        int visibleBottom = h;

        // Set GDI clipping region to prevent icons from drawing into header
        HRGN clipRegion = CreateRectRgn(0, visibleTop, w, h);
        SelectClipRgn(memDC, clipRegion);

        bool isDetailsView = (config.GetViewMode() == ViewMode::Details);

        for (int i = 0; i < (int)icons.size(); i++) {
            const auto& icon = icons[i];

            // Apply scroll offset to icon positions
            int drawTop = icon.rect.top - scrollPosition;
            int drawBottom = icon.rect.bottom - scrollPosition;
            int iconDrawTop = icon.iconRect.top - scrollPosition;

            // Skip if completely outside visible area
            if (drawBottom < visibleTop || drawTop >= visibleBottom) continue;

            // Selection highlight (only for grid views, details view handled separately)
            if (i == selectedIcon && !isDraggingIcon && !isDetailsView) {
                BYTE selAlpha = 180;
                BYTE selR = 60, selG = 120, selB = 200;
                BYTE selPmR = (BYTE)((selR * selAlpha) / 255);
                BYTE selPmG = (BYTE)((selG * selAlpha) / 255);
                BYTE selPmB = (BYTE)((selB * selAlpha) / 255);
                DWORD selPixel = (selAlpha << 24) | (selPmR << 16) | (selPmG << 8) | selPmB;

                for (int y = drawTop; y < drawBottom && y < h; y++) {
                    if (y < visibleTop) continue;  // Clip to visible area
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++) {
                        if (x >= 0 && y >= 0) {
                            pixels[y * w + x] = selPixel;
                        }
                    }
                }
            }

            // Drop target indicator
            if (isDraggingIcon && i == dropTargetIndex && i != draggedIconIndex) {
                DWORD dropPixel = (255 << 24) | (100 << 16) | (200 << 8) | 255;
                int left = icon.rect.left - 2;
                int top = drawTop - 2;
                int right = icon.rect.right + 2;
                int bottom = drawBottom + 2;
                // Draw rectangle border (with clipping)
                for (int x = left; x < right && x < w; x++) {
                    if (x >= 0 && top >= visibleTop && top < h) pixels[top * w + x] = dropPixel;
                    if (x >= 0 && bottom - 1 >= visibleTop && bottom - 1 < h) pixels[(bottom - 1) * w + x] = dropPixel;
                }
                for (int y = top; y < bottom && y < h; y++) {
                    if (y < visibleTop) continue;
                    if (left >= 0 && y >= 0) pixels[y * w + left] = dropPixel;
                    if (right - 1 >= 0 && right - 1 < w && y >= 0) pixels[y * w + (right - 1)] = dropPixel;
                }
            }

            if (isDetailsView) {
                // Details view: icon + name + type + size + date + sync status

                // Draw selection highlight for this row (before drawing content)
                if (i == selectedIcon && !isDraggingIcon) {
                    BYTE selAlpha = 200;
                    BYTE selR = 60, selG = 120, selB = 200;
                    BYTE selPmR = (BYTE)((selR * selAlpha) / 255);
                    BYTE selPmG = (BYTE)((selG * selAlpha) / 255);
                    BYTE selPmB = (BYTE)((selB * selAlpha) / 255);
                    DWORD selPixel = (selAlpha << 24) | (selPmR << 16) | (selPmG << 8) | selPmB;

                    for (int y = drawTop; y < drawBottom && y < h; y++) {
                        if (y < visibleTop) continue;
                        for (int x = icon.rect.left; x < icon.rect.right && x < w; x++) {
                            if (x >= 0 && y >= 0) {
                                pixels[y * w + x] = selPixel;
                            }
                        }
                    }
                }

                int currentIconSize = ICON_SIZE_DETAILS;
                HICON hIconToDraw = icon.hIconSmall ? icon.hIconSmall : icon.hIcon;

                // Draw small icon
                if (hIconToDraw) {
                    DrawIconEx(memDC,
                        icon.iconRect.left, iconDrawTop,
                        hIconToDraw,
                        currentIconSize, currentIconSize,
                        0, nullptr, DI_NORMAL);

                    // Fix alpha for icon area
                    for (int py = iconDrawTop; py < iconDrawTop + currentIconSize && py < h; py++) {
                        if (py < visibleTop) continue;
                        for (int px = icon.iconRect.left; px < icon.iconRect.left + currentIconSize && px < w; px++) {
                            if (px >= 0 && py >= 0) {
                                DWORD pixel = pixels[py * w + px];
                                BYTE a = (pixel >> 24) & 0xFF;
                                if (a == 0) {
                                    BYTE r = (pixel >> 16) & 0xFF;
                                    BYTE g = (pixel >> 8) & 0xFF;
                                    BYTE b = pixel & 0xFF;
                                    if (r > 0 || g > 0 || b > 0) {
                                        pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                                    }
                                }
                            }
                        }
                    }
                }

                // Column layout for details view
                // Columns: Name (40%) | Type (20%) | Size (15%) | Date (15%) | Sync (10%)
                int contentWidth = icon.rect.right - icon.rect.left - ICON_SIZE_DETAILS - 8;
                int nameCol = icon.iconRect.left + ICON_SIZE_DETAILS + 4;
                int typeCol = nameCol + (int)(contentWidth * 0.40);
                int sizeCol = typeCol + (int)(contentWidth * 0.20);
                int dateCol = sizeCol + (int)(contentWidth * 0.15);
                int syncCol = dateCol + (int)(contentWidth * 0.15);

                SetTextColor(memDC, RGB(255, 255, 255));

                // Draw name
                RECT nameRect = { nameCol, drawTop, typeCol - 4, drawBottom };
                DrawTextW(memDC, icon.displayName.c_str(), (int)icon.displayName.length(),
                    &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Draw type (dimmer color)
                SetTextColor(memDC, RGB(180, 180, 180));
                RECT typeRect = { typeCol, drawTop, sizeCol - 4, drawBottom };
                DrawTextW(memDC, icon.fileType.c_str(), (int)icon.fileType.length(),
                    &typeRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Draw size
                std::wstring sizeStr;
                if (icon.fileSize < 1024) {
                    sizeStr = std::to_wstring(icon.fileSize) + L" B";
                } else if (icon.fileSize < 1024 * 1024) {
                    sizeStr = std::to_wstring(icon.fileSize / 1024) + L" KB";
                } else if (icon.fileSize < 1024 * 1024 * 1024) {
                    sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024)) + L" MB";
                } else {
                    sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024 * 1024)) + L" GB";
                }
                RECT sizeRect = { sizeCol, drawTop, dateCol - 4, drawBottom };
                DrawTextW(memDC, sizeStr.c_str(), (int)sizeStr.length(),
                    &sizeRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                // Draw date
                FILETIME localTime;
                SYSTEMTIME sysTime;
                FileTimeToLocalFileTime(&icon.modifiedTime, &localTime);
                FileTimeToSystemTime(&localTime, &sysTime);
                wchar_t dateStr[32];
                swprintf_s(dateStr, L"%02d/%02d/%04d", sysTime.wMonth, sysTime.wDay, sysTime.wYear);
                RECT dateRect = { dateCol, drawTop, syncCol - 4, drawBottom };
                DrawTextW(memDC, dateStr, -1,
                    &dateRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                // Draw sync status indicator
                if (icon.syncStatus != SyncStatus::None) {
                    const wchar_t* syncSymbol = L"";
                    COLORREF syncColor = RGB(180, 180, 180);
                    switch (icon.syncStatus) {
                        case SyncStatus::Synced:
                            syncSymbol = L"\u2713";  // Check mark
                            syncColor = RGB(100, 200, 100);  // Green
                            break;
                        case SyncStatus::Syncing:
                            syncSymbol = L"\u21BB";  // Circular arrows
                            syncColor = RGB(100, 150, 255);  // Blue
                            break;
                        case SyncStatus::Pending:
                            syncSymbol = L"\u23F1";  // Stopwatch/clock
                            syncColor = RGB(100, 150, 255);  // Blue
                            break;
                        case SyncStatus::Error:
                            syncSymbol = L"\u2717";  // X mark
                            syncColor = RGB(255, 100, 100);  // Red
                            break;
                        case SyncStatus::CloudOnly:
                            syncSymbol = L"\u2601";  // Cloud
                            syncColor = RGB(150, 150, 255);  // Light blue
                            break;
                        default:
                            break;
                    }
                    SetTextColor(memDC, syncColor);
                    RECT syncRect = { syncCol, drawTop, icon.rect.right - 4, drawBottom };
                    DrawTextW(memDC, syncSymbol, -1,
                        &syncRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }

                // Fix alpha for entire row text area
                for (int py = drawTop; py < drawBottom && py < h; py++) {
                    if (py < visibleTop) continue;
                    for (int px = nameCol; px < icon.rect.right && px < w; px++) {
                        if (px >= 0 && py >= 0) {
                            DWORD pixel = pixels[py * w + px];
                            BYTE a = (pixel >> 24) & 0xFF;
                            BYTE r = (pixel >> 16) & 0xFF;
                            BYTE g = (pixel >> 8) & 0xFF;
                            BYTE b = pixel & 0xFF;
                            if (a == 0 && (r > 0 || g > 0 || b > 0)) {
                                pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                            }
                        }
                    }
                }

                // Draw selection border for selected row in details view
                if (i == selectedIcon && !isDraggingIcon) {
                    DWORD borderPixel = (255 << 24) | (100 << 16) | (150 << 8) | 255;
                    // Top border
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++) {
                        if (x >= 0 && drawTop >= visibleTop && drawTop < h) {
                            pixels[drawTop * w + x] = borderPixel;
                        }
                    }
                    // Bottom border
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++) {
                        if (x >= 0 && (drawBottom - 1) >= visibleTop && (drawBottom - 1) < h) {
                            pixels[(drawBottom - 1) * w + x] = borderPixel;
                        }
                    }
                }

            } else {
                // Grid view (Small/Medium/Large icons)
                // Icon image
                if (icon.hIcon) {
                    DrawIconEx(memDC,
                        icon.iconRect.left, iconDrawTop,
                        icon.hIcon,
                        iconSize, iconSize,
                        0, nullptr, DI_NORMAL);

                    // Fix alpha for icon area (with clipping)
                    for (int py = iconDrawTop; py < iconDrawTop + iconSize && py < h; py++) {
                        if (py < visibleTop) continue;
                        for (int px = icon.iconRect.left; px < icon.iconRect.left + iconSize && px < w; px++) {
                            if (px >= 0 && py >= 0) {
                                DWORD pixel = pixels[py * w + px];
                                BYTE a = (pixel >> 24) & 0xFF;
                                if (a == 0) {
                                    BYTE r = (pixel >> 16) & 0xFF;
                                    BYTE g = (pixel >> 8) & 0xFF;
                                    BYTE b = pixel & 0xFF;
                                    if (r > 0 || g > 0 || b > 0) {
                                        pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                                    }
                                }
                            }
                        }
                    }
                }

                // Label
                SetTextColor(memDC, RGB(255, 255, 255));
                int labelTop = iconDrawTop + iconSize + 2;
                RECT labelRect = {
                    icon.rect.left,
                    labelTop,
                    icon.rect.right,
                    drawBottom
                };
                // Only draw if label area is visible
                if (labelTop < visibleBottom && drawBottom > visibleTop) {
                    DrawTextW(memDC, icon.displayName.c_str(), (int)icon.displayName.length(),
                        &labelRect, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);

                    // Fix alpha for label area (with clipping)
                    for (int py = labelTop; py < drawBottom && py < h; py++) {
                        if (py < visibleTop) continue;
                        for (int px = icon.rect.left; px < icon.rect.right && px < w; px++) {
                            if (px >= 0 && py >= 0) {
                                DWORD pixel = pixels[py * w + px];
                                BYTE a = (pixel >> 24) & 0xFF;
                                BYTE r = (pixel >> 16) & 0xFF;
                                BYTE g = (pixel >> 8) & 0xFF;
                                BYTE b = pixel & 0xFF;
                                if (a == 0 && (r > 0 || g > 0 || b > 0)) {
                                    pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove clipping region
        SelectClipRgn(memDC, nullptr);
        DeleteObject(clipRegion);

        SelectObject(memDC, oldIconFont);
        DeleteObject(iconFont);
    }

    // Draw scrollbar (only when needed, blends with appearance)
    if (NeedsScrollbar() && (!config.IsRolledUp || isHoverExpanded)) {
        RECT track = GetScrollbarTrackRect();
        RECT thumb = GetScrollbarThumbRect();

        // Draw track (subtle, semi-transparent)
        BYTE trackAlpha = 60;
        BYTE trackPmR = (BYTE)((255 * trackAlpha) / 255);
        BYTE trackPmG = (BYTE)((255 * trackAlpha) / 255);
        BYTE trackPmB = (BYTE)((255 * trackAlpha) / 255);
        DWORD trackPixel = (trackAlpha << 24) | (trackPmR << 16) | (trackPmG << 8) | trackPmB;

        for (int y = track.top; y < track.bottom && y < h; y++) {
            for (int x = track.left; x < track.right && x < w; x++) {
                if (x >= 0 && y >= 0) {
                    pixels[y * w + x] = trackPixel;
                }
            }
        }

        // Draw thumb (more visible, rounded appearance via color blend)
        BYTE thumbAlpha = 140;
        BYTE thumbPmR = (BYTE)((200 * thumbAlpha) / 255);
        BYTE thumbPmG = (BYTE)((200 * thumbAlpha) / 255);
        BYTE thumbPmB = (BYTE)((200 * thumbAlpha) / 255);
        DWORD thumbPixel = (thumbAlpha << 24) | (thumbPmR << 16) | (thumbPmG << 8) | thumbPmB;

        // Draw rounded thumb (simple rounded corners)
        int radius = (SCROLLBAR_WIDTH - 2) / 2;
        for (int y = thumb.top; y < thumb.bottom && y < h; y++) {
            for (int x = thumb.left; x < thumb.right && x < w; x++) {
                if (x >= 0 && y >= 0) {
                    // Simple rounded corners check
                    int dx = 0, dy = 0;
                    if (y < thumb.top + radius) dy = thumb.top + radius - y;
                    if (y > thumb.bottom - radius - 1) dy = y - (thumb.bottom - radius - 1);
                    if (x < thumb.left + radius) dx = thumb.left + radius - x;
                    if (x > thumb.right - radius - 1) dx = x - (thumb.right - radius - 1);

                    if (dx * dx + dy * dy <= radius * radius) {
                        pixels[y * w + x] = thumbPixel;
                    }
                }
            }
        }
    }

    // Resize grip (bottom-right corner, skip when rolled up)
    if (!config.IsRolledUp) {
        DWORD gripPixel = (255 << 24) | (150 << 16) | (150 << 8) | 150;
        for (int i = 0; i < 8; i++) {
            int x = w - 2;
            int y = h - 10 + i;
            if (x >= 0 && x < w && y >= 0 && y < h) {
                pixels[y * w + x] = gripPixel;
            }
        }
        for (int i = 0; i < 8; i++) {
            int x = w - 10 + i;
            int y = h - 2;
            if (x >= 0 && x < w && y >= 0 && y < h) {
                pixels[y * w + x] = gripPixel;
            }
        }
    }

    // Update the layered window
    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { w, h };
    POINT ptDst = { rect.left, rect.top };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

// ============================================================================
// Event handlers
// ============================================================================

void CorralWindow::OnSize() {
    SyncConfigFromWindow();
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::OnMove() {
    SyncConfigFromWindow();
    UpdateLayeredContent();
}

void CorralWindow::OnLeftButtonDown(int x, int y) {
    // Check resize area first (but not when rolled up)
    if (!config.IsRolledUp) {
        int resizeHit = HitTestResize(x, y);
        if (resizeHit) {
            StartResize(resizeHit, x, y);
            return;
        }
    }

    // Check if clicked on scrollbar
    if (HitTestScrollbar(x, y)) {
        if (HitTestScrollbarThumb(x, y)) {
            // Clicked on thumb - start dragging
            StartScrollbarDrag(y);
        } else {
            // Clicked in track - jump to position
            RECT track = GetScrollbarTrackRect();
            RECT thumb = GetScrollbarThumbRect();
            int thumbHeight = thumb.bottom - thumb.top;
            int trackHeight = track.bottom - track.top;
            int thumbRange = trackHeight - thumbHeight;
            int scrollRange = contentHeight - GetVisibleHeight();

            // Calculate new scroll position - center thumb on click point
            int clickOffset = y - track.top - (thumbHeight / 2);
            if (clickOffset < 0) clickOffset = 0;
            if (clickOffset > thumbRange) clickOffset = thumbRange;

            if (thumbRange > 0) {
                scrollPosition = (int)((float)clickOffset / thumbRange * scrollRange);
                ClampScrollPosition();
                UpdateLayeredContent();
            }
        }
        return;
    }

    // Check if clicked on an icon - select it (drag starts on mouse move)
    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        // Check if we clicked on the label of an already-selected icon
        if (hit == selectedIcon && HitTestIconLabel(x, y, hit)) {
            // Clicked on label of selected icon - start rename
            StartIconRename(hit);
            return;
        }

        // Otherwise, just select the icon
        selectedIcon = hit;
        draggedIconIndex = hit;  // Potential drag candidate
        iconDragStart = { x, y };  // Store starting position
        dropTargetIndex = -1;
        SetFocus(hwnd);  // Take keyboard focus so F2 works
        SetCapture(hwnd);
        UpdateLayeredContent();  // Show selection highlight
        return;
    }

    // Deselect
    if (selectedIcon >= 0) {
        selectedIcon = -1;
        UpdateLayeredContent();
    }

    // Title bar - start dragging window
    if (y < TITLE_BAR_HEIGHT) {
        isDragging = true;
        GetCursorPos(&dragStart);
        GetWindowRect(hwnd, &dragStartRect);
        SetCapture(hwnd);
    }
}

void CorralWindow::OnLeftButtonDblClick(int x, int y) {
    // Double-click on title bar toggles roll-up
    if (y < TITLE_BAR_HEIGHT) {
        ToggleRollUp();
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        OpenFile(hit);
    }
}

void CorralWindow::OnRightButtonDown(int x, int y) {
    // Check title bar first - don't let scrolled icons consume clicks here
    if (y < TITLE_BAR_HEIGHT) {
        ShowContextMenu(x, y);
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        selectedIcon = hit;
        UpdateLayeredContent();

        POINT pt = { x, y };
        ClientToScreen(hwnd, &pt);
        ShowShellContextMenu(hit, pt.x, pt.y);
        return;
    }

    ShowContextMenu(x, y);
}

void CorralWindow::OnDropFiles(HDROP hDrop) {
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

    bool changed = false;
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t filePath[MAX_PATH];
        DragQueryFileW(hDrop, i, filePath, MAX_PATH);

        std::wstring fullPath(filePath);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        std::wstring fileName = (lastSlash != std::wstring::npos) ?
            fullPath.substr(lastSlash + 1) : fullPath;

        int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string fileNameUtf8(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameUtf8[0], size, nullptr, nullptr);

        auto it = std::find(config.Files.begin(), config.Files.end(), fileNameUtf8);
        if (it == config.Files.end()) {
            config.Files.push_back(fileNameUtf8);
            changed = true;

            if (App::GetInstance()) {
                App::GetInstance()->RemoveFileFromOtherCorrals(fileName, &config);
            }
        }
    }

    DragFinish(hDrop);

    if (changed) {
        LoadFiles();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Menus and dialogs
// ============================================================================

void CorralWindow::ShowContextMenu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Rename");
    AppendMenuW(menu, MF_STRING, 2, L"Appearance...");

    // Change Folder option for virtual corrals
    if (config.IsVirtual) {
        AppendMenuW(menu, MF_STRING, 7, L"Change Folder...");
    }

    // View submenu
    HMENU viewMenu = CreatePopupMenu();
    ViewMode currentMode = config.GetViewMode();
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::SmallIcons ? MF_CHECKED : 0), 10, L"Small Icons");
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::MediumIcons ? MF_CHECKED : 0), 11, L"Medium Icons");
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::LargeIcons ? MF_CHECKED : 0), 12, L"Large Icons");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::Details ? MF_CHECKED : 0), 13, L"Details");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)viewMenu, L"View");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Catch-all option - only for non-virtual corrals
    if (!config.IsVirtual) {
        UINT catchAllFlags = MF_STRING | (config.IsCatchAll ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(menu, catchAllFlags, 3, L"Catch-All (receives new files)");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    // Show Desktop Icons with checkmark
    UINT iconFlags = MF_STRING | (DesktopIcons::AreIconsVisible() ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(menu, iconFlags, 5, L"Show Desktop Icons");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 6, L"Create New Corral");
    AppendMenuW(menu, MF_STRING, 8, L"New Virtual Corral");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 4, L"Delete Corral");

    POINT pt = { x, y };
    ClientToScreen(hwnd, &pt);

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    DestroyMenu(menu);

    switch (cmd) {
    case 1: ShowRenameDialog(); break;
    case 2: ShowAppearanceDialog(); break;
    case 3: ToggleCatchAll(); break;
    case 4: DeleteCorral(); break;
    case 5:
        if (App::GetInstance()) {
            App::GetInstance()->ToggleDesktopIcons();
        }
        break;
    case 6:
        if (App::GetInstance()) {
            // Find a good position for the new corral
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);

            // Get screen dimensions
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            POINT newPos;
            int corralWidth = 300;
            int corralHeight = 200;
            int gap = 20;

            // Try to place to the right of current corral
            newPos.x = currentRect.right + gap + corralWidth / 2;
            newPos.y = currentRect.top + corralHeight / 2;

            // If that would go off screen, try below
            if (newPos.x + corralWidth / 2 > mi.rcWork.right) {
                newPos.x = currentRect.left + corralWidth / 2;
                newPos.y = currentRect.bottom + gap + corralHeight / 2;
            }

            // If that would also go off screen, offset from current
            if (newPos.y + corralHeight / 2 > mi.rcWork.bottom) {
                newPos.x = currentRect.left + 50 + corralWidth / 2;
                newPos.y = currentRect.top + 50 + corralHeight / 2;
            }

            App::GetInstance()->CreateCorralAt(newPos);
        }
        break;
    case 7:
        // Change Folder (virtual corrals only)
        ChangeFolderPath();
        break;
    case 8:
        // New Virtual Corral
        if (App::GetInstance()) {
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);

            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            POINT newPos;
            int corralWidth = 300;
            int corralHeight = 200;
            int gap = 20;

            newPos.x = currentRect.right + gap + corralWidth / 2;
            newPos.y = currentRect.top + corralHeight / 2;

            if (newPos.x + corralWidth / 2 > mi.rcWork.right) {
                newPos.x = currentRect.left + corralWidth / 2;
                newPos.y = currentRect.bottom + gap + corralHeight / 2;
            }

            if (newPos.y + corralHeight / 2 > mi.rcWork.bottom) {
                newPos.x = currentRect.left + 50 + corralWidth / 2;
                newPos.y = currentRect.top + 50 + corralHeight / 2;
            }

            App::GetInstance()->CreateVirtualCorralAt(newPos);
        }
        break;
    case 10: SetViewMode(ViewMode::SmallIcons); break;
    case 11: SetViewMode(ViewMode::MediumIcons); break;
    case 12: SetViewMode(ViewMode::LargeIcons); break;
    case 13: SetViewMode(ViewMode::Details); break;
    }
}

void CorralWindow::SetViewMode(ViewMode mode) {
    if (config.GetViewMode() == mode) return;

    config.SetViewMode(mode);
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    // Reload icons to get appropriate size (uses LoadFiles which handles both normal and virtual corrals)
    scrollPosition = 0;  // Reset scroll when changing view
    LoadFiles();

    if (App::GetInstance()) {
        App::GetInstance()->SaveConfig();
    }
}

int CorralWindow::GetIconSizeForViewMode() const {
    switch (config.GetViewMode()) {
    case ViewMode::SmallIcons: return ICON_SIZE_SMALL;
    case ViewMode::MediumIcons: return ICON_SIZE_MEDIUM;
    case ViewMode::LargeIcons: return ICON_SIZE_LARGE;
    case ViewMode::Details: return ICON_SIZE_DETAILS;
    default: return ICON_SIZE_SMALL;
    }
}

void CorralWindow::UpdateIconSpacingForViewMode() {
    switch (config.GetViewMode()) {
    case ViewMode::SmallIcons:
        iconSpacingX = 72;
        iconSpacingY = 68;
        break;
    case ViewMode::MediumIcons:
        iconSpacingX = 80;
        iconSpacingY = 80;
        break;
    case ViewMode::LargeIcons:
        // Larger spacing for large icons to fit longer labels
        iconSpacingX = 100;
        iconSpacingY = 100;
        break;
    case ViewMode::Details:
        // Not used for details view, but set reasonable defaults
        iconSpacingX = 72;
        iconSpacingY = DETAILS_ROW_HEIGHT;
        break;
    default:
        iconSpacingX = 72;
        iconSpacingY = 68;
        break;
    }
}

static INT_PTR CALLBACK RenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
        wchar_t* initialText = (wchar_t*)lParam;
        SetDlgItemTextW(hDlg, 101, initialText);

        HWND hEdit = GetDlgItem(hDlg, 101);
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);

        HWND hParent = GetParent(hDlg);
        if (hParent) {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;

            // Get monitor work area
            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            int cx = parentRect.left + (parentRect.right - parentRect.left - dlgWidth) / 2;
            int cy = parentRect.top + (parentRect.bottom - parentRect.top - dlgHeight) / 2;

            // Clamp to monitor bounds
            if (cx + dlgWidth > mi.rcWork.right) cx = mi.rcWork.right - dlgWidth;
            if (cx < mi.rcWork.left) cx = mi.rcWork.left;
            if (cy + dlgHeight > mi.rcWork.bottom) cy = mi.rcWork.bottom - dlgHeight;
            if (cy < mi.rcWork.top) cy = mi.rcWork.top;

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buffer[256];
            GetDlgItemTextW(hDlg, 101, buffer, 256);
            wchar_t* result = (wchar_t*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            wcscpy_s(result, 256, buffer);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void CorralWindow::ShowRenameDialog() {
    WORD dlgTemplate[512] = {};
    WORD* p = dlgTemplate;

    DLGTEMPLATE* dlg = (DLGTEMPLATE*)p;
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = 3;
    dlg->cx = 180;
    dlg->cy = 55;
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0;
    *p++ = 0;

    const wchar_t* dlgTitle = L"Rename Corral";
    size_t titleLen = wcslen(dlgTitle) + 1;
    memcpy(p, dlgTitle, titleLen * sizeof(wchar_t));
    p += titleLen;

    *p++ = 9;
    const wchar_t* fontName = L"Segoe UI";
    size_t fontLen = wcslen(fontName) + 1;
    memcpy(p, fontName, fontLen * sizeof(wchar_t));
    p += fontLen;

    if ((ULONG_PTR)p % 4) p++;

    DLGITEMTEMPLATE* item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    item->x = 8; item->y = 8; item->cx = 164; item->cy = 14;
    item->id = 101;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0081;
    *p++ = 0; *p++ = 0;

    if ((ULONG_PTR)p % 4) p++;

    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 70; item->y = 30; item->cx = 50; item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    const wchar_t* okText = L"OK";
    memcpy(p, okText, 3 * sizeof(wchar_t));
    p += 3; *p++ = 0;

    if ((ULONG_PTR)p % 4) p++;

    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 124; item->y = 30; item->cx = 50; item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    const wchar_t* cancelText = L"Cancel";
    memcpy(p, cancelText, 7 * sizeof(wchar_t));
    p += 7; *p++ = 0;

    wchar_t nameBuffer[256] = {};
    std::wstring currentTitle = Utf8ToWide(config.Title);
    wcsncpy_s(nameBuffer, currentTitle.c_str(), _TRUNCATE);

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE*)dlgTemplate,
        hwnd,
        RenameDlgProc,
        (LPARAM)nameBuffer
    );

    if (result == IDOK && wcslen(nameBuffer) > 0) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, nullptr, 0, nullptr, nullptr);
        std::string newTitle(sz - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, &newTitle[0], sz, nullptr, nullptr);

        config.Title = newTitle;
        SetWindowTextW(hwnd, nameBuffer);
        UpdateLayeredContent();

        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

struct AppearanceDlgData {
    BYTE alpha;
    COLORREF color;
    HBRUSH hBrush;
    HWND previewWindow;
    std::string* colorHex;
    bool useAsDefault;
    bool applyToAll;
};

static INT_PTR CALLBACK AppearanceDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppearanceDlgData* data = (AppearanceDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (AppearanceDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        // Create brush for initial color
        data->hBrush = CreateSolidBrush(data->color);

        // Setup slider
        HWND hSlider = GetDlgItem(hDlg, 102);
        SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hSlider, TBM_SETPOS, TRUE, data->alpha);

        // Update label
        wchar_t label[32];
        int pct = (data->alpha * 100) / 255;
        swprintf_s(label, L"%d%%", pct);
        SetDlgItemTextW(hDlg, 103, label);

        // Position below parent corral for live preview visibility
        HWND hParent = GetParent(hDlg);
        if (hParent) {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;
            int parentWidth = parentRect.right - parentRect.left;

            // Get monitor work area
            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            // Center horizontally, position below the corral window
            int cx = parentRect.left + (parentWidth - dlgWidth) / 2;
            int cy = parentRect.bottom + 5;  // 5px below the corral

            // If dialog would go below screen, try above the corral
            if (cy + dlgHeight > mi.rcWork.bottom) {
                cy = parentRect.top - dlgHeight - 5;  // 5px above the corral
            }

            // If still outside (top), clamp to top of work area
            if (cy < mi.rcWork.top) {
                cy = mi.rcWork.top;
            }

            // Clamp horizontal position to stay within monitor bounds
            if (cx + dlgWidth > mi.rcWork.right) {
                cx = mi.rcWork.right - dlgWidth;
            }
            if (cx < mi.rcWork.left) {
                cx = mi.rcWork.left;
            }

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        if (GetDlgCtrlID((HWND)lParam) == 105) { // Color swatch
            return (INT_PTR)data->hBrush;
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 106) { // Change Color button
            static COLORREF customColors[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(CHOOSECOLORW);
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = data->color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&cc)) {
                data->color = cc.rgbResult;

                // Update brush
                if (data->hBrush) DeleteObject(data->hBrush);
                data->hBrush = CreateSolidBrush(data->color);

                // Force redraw of swatch
                InvalidateRect(GetDlgItem(hDlg, 105), nullptr, TRUE);

                // Update live preview
                BYTE r = GetRValue(data->color);
                BYTE g = GetGValue(data->color);
                BYTE b = GetBValue(data->color);
                char hexBuf[16];
                sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
                *data->colorHex = hexBuf;
                if (data->previewWindow) InvalidateRect(data->previewWindow, nullptr, FALSE);
            }
            return TRUE;
        }
        if (id == IDOK) {
            // Read checkbox states before closing
            data->useAsDefault = (SendDlgItemMessageW(hDlg, 107, BM_GETCHECK, 0, 0) == BST_CHECKED);
            data->applyToAll = (SendDlgItemMessageW(hDlg, 108, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hDlg, 102)) {
            data->alpha = (BYTE)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);

            wchar_t label[32];
            int pct = (data->alpha * 100) / 255;
            swprintf_s(label, L"%d%%", pct);
            SetDlgItemTextW(hDlg, 103, label);

            // Live preview
            BYTE r = GetRValue(data->color);
            BYTE g = GetGValue(data->color);
            BYTE b = GetBValue(data->color);
            char hexBuf[16];
            sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
            *data->colorHex = hexBuf;
            if (data->previewWindow) InvalidateRect(data->previewWindow, nullptr, FALSE);
        }
        return TRUE;
    }
    case WM_DESTROY:
        if (data && data->hBrush) {
            DeleteObject(data->hBrush);
            data->hBrush = nullptr;
        }
        return TRUE;
    }
    return FALSE;
}

void CorralWindow::ShowAppearanceDialog() {
    auto Align = [](WORD* p) { return (ULONG_PTR)p % 4 ? p + 1 : p; };

    // Build dynamic title with corral name
    std::wstring wTitle(config.Title.begin(), config.Title.end());
    std::wstring dlgTitleStr = L"Appearance: " + wTitle;
    const wchar_t* strDlgTitle = dlgTitleStr.c_str();
    const wchar_t* strFontName = L"Segoe UI";
    const wchar_t* strGrpColor = L"Background Color";
    const wchar_t* strBtnChange = L"Change...";
    const wchar_t* strGrpOpacity = L"Opacity";
    const wchar_t* strTrackbarClass = L"msctls_trackbar32";
    const wchar_t* strLabelPercent = L"100%";
    const wchar_t* strChkDefault = L"Use as default for new corrals";
    const wchar_t* strChkApplyAll = L"Apply to all corrals";
    const wchar_t* strBtnOK = L"OK";
    const wchar_t* strBtnCancel = L"Cancel";

    WORD dlgTemplate[2048] = {};
    WORD* p = dlgTemplate;

    DLGTEMPLATE* dlg = (DLGTEMPLATE*)p;
    dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = 10;  // 8 original + 2 checkboxes
    dlg->cx = 200;
    dlg->cy = 130;   // Increased height for checkboxes
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0; *p++ = 0;

    size_t len = wcslen(strDlgTitle) + 1;
    memcpy(p, strDlgTitle, len * sizeof(wchar_t)); p += len;

    *p++ = 9;
    len = wcslen(strFontName) + 1;
    memcpy(p, strFontName, len * sizeof(wchar_t)); p += len;

    // 1. Group Box "Background Color"
    p = Align(p);
    DLGITEMTEMPLATE* item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    item->x = 5; item->y = 5; item->cx = 190; item->cy = 35;
    item->id = (WORD)-1;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strGrpColor) + 1; memcpy(p, strGrpColor, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 2. Color Swatch (Static with Border) (ID 105)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY;
    item->x = 15; item->y = 18; item->cx = 30; item->cy = 14;
    item->id = 105;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0082; *p++ = 0; *p++ = 0;

    // 3. Change Button (ID 106)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 55; item->y = 18; item->cx = 60; item->cy = 14;
    item->id = 106;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnChange) + 1; memcpy(p, strBtnChange, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 4. Group Box "Opacity"
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    item->x = 5; item->y = 45; item->cx = 190; item->cy = 30;
    item->id = (WORD)-1;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strGrpOpacity) + 1; memcpy(p, strGrpOpacity, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 5. Slider (ID 102) - use TBS_NOTICKS to remove tick marks
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS;
    item->x = 15; item->y = 56; item->cx = 140; item->cy = 15;
    item->id = 102;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    len = wcslen(strTrackbarClass) + 1;
    memcpy(p, strTrackbarClass, len * sizeof(wchar_t));
    p += len;
    *p++ = 0;
    *p++ = 0;

    // 6. Label % (ID 103)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | SS_RIGHT;
    item->x = 160; item->y = 58; item->cx = 25; item->cy = 12;
    item->id = 103;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0082;
    len = wcslen(strLabelPercent) + 1; memcpy(p, strLabelPercent, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 7. Checkbox "Use as default for new corrals" (ID 107)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
    item->x = 10; item->y = 82; item->cx = 180; item->cy = 12;
    item->id = 107;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strChkDefault) + 1; memcpy(p, strChkDefault, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 8. Checkbox "Apply to all corrals" (ID 108)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
    item->x = 10; item->y = 96; item->cx = 180; item->cy = 12;
    item->id = 108;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strChkApplyAll) + 1; memcpy(p, strChkApplyAll, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 9. OK Button
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 90; item->y = 112; item->cx = 50; item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnOK) + 1; memcpy(p, strBtnOK, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 10. Cancel Button
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 145; item->y = 112; item->cx = 50; item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnCancel) + 1; memcpy(p, strBtnCancel, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // Data prep
    AppearanceDlgData dlgData;
    dlgData.alpha = 153;
    dlgData.color = RGB(0,0,0);
    dlgData.previewWindow = hwnd;
    dlgData.colorHex = &config.ColorHex;
    dlgData.hBrush = nullptr;
    dlgData.useAsDefault = false;
    dlgData.applyToAll = false;

    if (!config.ColorHex.empty() && config.ColorHex[0] == '#' && config.ColorHex.length() >= 9) {
        unsigned int colorValue;
        sscanf_s(config.ColorHex.c_str() + 1, "%x", &colorValue);
        dlgData.alpha = (colorValue >> 24) & 0xFF;
        BYTE r = (colorValue >> 16) & 0xFF;
        BYTE g = (colorValue >> 8) & 0xFF;
        BYTE b = colorValue & 0xFF;
        dlgData.color = RGB(r, g, b);
    }

    std::string originalColor = config.ColorHex;

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE*)dlgTemplate,
        hwnd,
        AppearanceDlgProc,
        (LPARAM)&dlgData
    );

    if (result != IDOK) {
        config.ColorHex = originalColor;
        UpdateLayeredContent();
    } else {
        App* app = App::GetInstance();
        if (app) {
            // Handle "Use as default for new corrals"
            if (dlgData.useAsDefault) {
                app->SetDefaultColorHex(config.ColorHex);
            }

            // Handle "Apply to all corrals"
            if (dlgData.applyToAll) {
                app->ApplyColorToAllCorrals(config.ColorHex);
            }

            app->SaveConfig();
        }
    }
}

void CorralWindow::DeleteCorral() {
    if (MessageBoxW(hwnd, L"Delete this corral?", L"Confirm Delete", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        if (App::GetInstance()) {
            App::GetInstance()->RemoveCorral(&config);
        }
    }
}

void CorralWindow::ToggleRollUp() {
    // Cancel any hover animation in progress
    isHoverExpanded = false;
    isAnimating = false;
    KillTimer(hwnd, ANIMATION_TIMER_ID);
    KillTimer(hwnd, HOVER_CHECK_TIMER_ID);

    config.IsRolledUp = !config.IsRolledUp;

    if (config.IsRolledUp) {
        // Save current height before rolling up
        savedHeight = config.Height;
        SetWindowPos(hwnd, nullptr, 0, 0, (int)config.Width, TITLE_BAR_HEIGHT,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else {
        // Restore to saved height
        SetWindowPos(hwnd, nullptr, 0, 0, (int)config.Width, (int)savedHeight,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        config.Height = savedHeight;
    }

    SyncConfigFromWindow();
    CalculateIconLayout();
    UpdateLayeredContent();

    if (App::GetInstance()) {
        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::ToggleCatchAll() {
    App* app = App::GetInstance();
    if (!app) return;

    if (config.IsCatchAll) {
        // Already catch-all - can't unset (must always have one)
        // Just ignore or could show a message
        return;
    }

    // Set this corral as catch-all and remove from all others
    for (const auto& corral : app->GetCorrals()) {
        if (corral.get() != this && corral->GetConfig().IsCatchAll) {
            corral->GetConfig().IsCatchAll = false;
            corral->UpdateWallpaperBackground();  // Remove symbol
        }
    }

    config.IsCatchAll = true;
    UpdateLayeredContent();  // Show symbol
    app->SaveConfig();
}

void CorralWindow::StartHoverExpand() {
    if (!config.IsRolledUp || isHoverExpanded || isAnimating) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Determine expand direction based on screen position
    // If expanding downward would go off screen, expand upward
    int expandedHeight = (int)savedHeight;
    int bottomIfDown = rect.top + expandedHeight;

    expandUpward = (bottomIfDown > screenHeight - 50);  // 50px buffer for taskbar

    animationStartHeight = rect.bottom - rect.top;
    animationTargetHeight = expandedHeight;
    animationStartTop = rect.top;

    if (expandUpward) {
        // Move window up while expanding
        animationTargetTop = rect.top - (expandedHeight - animationStartHeight);
    } else {
        animationTargetTop = rect.top;  // Stay in place
    }

    animationStartTime = GetTickCount();
    isAnimating = true;
    isHoverExpanded = true;

    // Start animation timer (60 FPS)
    SetTimer(hwnd, ANIMATION_TIMER_ID, 16, nullptr);
}

void CorralWindow::StartHoverCollapse() {
    if (!isHoverExpanded || isAnimating) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);

    animationStartHeight = rect.bottom - rect.top;
    animationTargetHeight = TITLE_BAR_HEIGHT;
    animationStartTop = rect.top;

    if (expandUpward) {
        // Move window back down while collapsing
        animationTargetTop = rect.top + (animationStartHeight - TITLE_BAR_HEIGHT);
    } else {
        animationTargetTop = rect.top;  // Stay in place
    }

    animationStartTime = GetTickCount();
    isAnimating = true;

    SetTimer(hwnd, ANIMATION_TIMER_ID, 16, nullptr);
}

void CorralWindow::OnAnimationTimer() {
    DWORD elapsed = GetTickCount() - animationStartTime;
    float progress = (float)elapsed / ANIMATION_DURATION;

    if (progress >= 1.0f) {
        progress = 1.0f;
        isAnimating = false;
        KillTimer(hwnd, ANIMATION_TIMER_ID);

        // If we just collapsed, mark as not hover-expanded
        if (animationTargetHeight == TITLE_BAR_HEIGHT) {
            isHoverExpanded = false;
        }

        // Start hover check timer to detect when mouse leaves
        if (isHoverExpanded) {
            SetTimer(hwnd, HOVER_CHECK_TIMER_ID, 100, nullptr);
        } else {
            KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
        }
    }

    // Ease-out interpolation for smoother animation
    float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

    int currentHeight = animationStartHeight + (int)((animationTargetHeight - animationStartHeight) * easedProgress);
    int currentTop = animationStartTop + (int)((animationTargetTop - animationStartTop) * easedProgress);

    SetWindowPos(hwnd, nullptr, (int)config.Left, currentTop, (int)config.Width, currentHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // Recalculate layout and redraw
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::OnHoverCheckTimer() {
    // Check if mouse is still inside the window
    POINT pt;
    GetCursorPos(&pt);

    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (!PtInRect(&rect, pt)) {
        // Mouse has left - collapse
        mouseInsideWindow = false;
        KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
        if (isHoverExpanded && !isAnimating) {
            StartHoverCollapse();
        }
    }
}

// ============================================================================
// Snap support
// ============================================================================

void CorralWindow::ApplySnap(int& newLeft, int& newTop, int width, int height) {
    App* app = App::GetInstance();
    if (!app) return;

    int newRight = newLeft + width;
    int newBottom = newTop + height;

    // Snap to screen edges
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Left edge of screen
    if (std::abs(newLeft) < SNAP_DISTANCE) {
        newLeft = SNAP_GAP;
    }
    // Right edge of screen
    if (std::abs(newRight - screenWidth) < SNAP_DISTANCE) {
        newLeft = screenWidth - width - SNAP_GAP;
    }
    // Top edge of screen
    if (std::abs(newTop) < SNAP_DISTANCE) {
        newTop = SNAP_GAP;
    }
    // Bottom edge of screen (account for taskbar ~40px)
    if (std::abs(newBottom - screenHeight + 40) < SNAP_DISTANCE) {
        newTop = screenHeight - height - 40 - SNAP_GAP;
    }

    // Snap to other corrals
    const auto& corrals = app->GetCorrals();
    for (const auto& other : corrals) {
        if (other->GetHWND() == hwnd) continue;  // Skip self

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        int otherLeft = otherRect.left;
        int otherTop = otherRect.top;
        int otherRight = otherRect.right;
        int otherBottom = otherRect.bottom;

        // Check vertical alignment (are we in the same vertical band?)
        bool verticalOverlap = (newTop < otherBottom + SNAP_DISTANCE) && (newBottom > otherTop - SNAP_DISTANCE);
        // Check horizontal alignment (are we in the same horizontal band?)
        bool horizontalOverlap = (newLeft < otherRight + SNAP_DISTANCE) && (newRight > otherLeft - SNAP_DISTANCE);

        if (verticalOverlap) {
            // Snap our right edge to their left edge (with gap)
            if (std::abs(newRight - otherLeft + SNAP_GAP) < SNAP_DISTANCE) {
                newLeft = otherLeft - width - SNAP_GAP;
            }
            // Snap our left edge to their right edge (with gap)
            if (std::abs(newLeft - otherRight - SNAP_GAP) < SNAP_DISTANCE) {
                newLeft = otherRight + SNAP_GAP;
            }
            // Snap our left edge to their left edge (align)
            if (std::abs(newLeft - otherLeft) < SNAP_DISTANCE) {
                newLeft = otherLeft;
            }
            // Snap our right edge to their right edge (align)
            if (std::abs(newRight - otherRight) < SNAP_DISTANCE) {
                newLeft = otherRight - width;
            }
        }

        if (horizontalOverlap) {
            // Snap our bottom edge to their top edge (with gap)
            if (std::abs(newBottom - otherTop + SNAP_GAP) < SNAP_DISTANCE) {
                newTop = otherTop - height - SNAP_GAP;
            }
            // Snap our top edge to their bottom edge (with gap)
            if (std::abs(newTop - otherBottom - SNAP_GAP) < SNAP_DISTANCE) {
                newTop = otherBottom + SNAP_GAP;
            }
            // Snap our top edge to their top edge (align)
            if (std::abs(newTop - otherTop) < SNAP_DISTANCE) {
                newTop = otherTop;
            }
            // Snap our bottom edge to their bottom edge (align)
            if (std::abs(newBottom - otherBottom) < SNAP_DISTANCE) {
                newTop = otherBottom - height;
            }
        }
    }
}

void CorralWindow::ApplyResizeSnap(int& newWidth, int& newHeight) {
    App* app = App::GetInstance();
    if (!app) return;

    RECT myRect;
    GetWindowRect(hwnd, &myRect);
    int myLeft = myRect.left;
    int myTop = myRect.top;
    int newRight = myLeft + newWidth;
    int newBottom = myTop + newHeight;

    // Snap to screen edges
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Right edge of screen
    if (std::abs(newRight - screenWidth + SNAP_GAP) < SNAP_DISTANCE) {
        newWidth = screenWidth - myLeft - SNAP_GAP;
    }
    // Bottom edge of screen (account for taskbar ~40px)
    if (std::abs(newBottom - screenHeight + 40 + SNAP_GAP) < SNAP_DISTANCE) {
        newHeight = screenHeight - myTop - 40 - SNAP_GAP;
    }

    // Snap to other corrals
    const auto& corrals = app->GetCorrals();
    for (const auto& other : corrals) {
        if (other->GetHWND() == hwnd) continue;

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        // Check if we're near horizontally (for right edge snap)
        bool nearVertically = (myTop < otherRect.bottom + SNAP_DISTANCE) && (newBottom > otherRect.top - SNAP_DISTANCE);
        // Check if we're near vertically (for bottom edge snap)
        bool nearHorizontally = (myLeft < otherRect.right + SNAP_DISTANCE) && (newRight > otherRect.left - SNAP_DISTANCE);

        if (nearVertically) {
            // Snap right edge to their left edge (with gap)
            if (std::abs(newRight - otherRect.left + SNAP_GAP) < SNAP_DISTANCE) {
                newWidth = otherRect.left - myLeft - SNAP_GAP;
            }
            // Snap right edge to their right edge (align)
            if (std::abs(newRight - otherRect.right) < SNAP_DISTANCE) {
                newWidth = otherRect.right - myLeft;
            }
        }

        if (nearHorizontally) {
            // Snap bottom edge to their top edge (with gap)
            if (std::abs(newBottom - otherRect.top + SNAP_GAP) < SNAP_DISTANCE) {
                newHeight = otherRect.top - myTop - SNAP_GAP;
            }
            // Snap bottom edge to their bottom edge (align)
            if (std::abs(newBottom - otherRect.bottom) < SNAP_DISTANCE) {
                newHeight = otherRect.bottom - myTop;
            }
        }
    }

    // Enforce minimum size
    if (newWidth < 100) newWidth = 100;
    if (newHeight < 80) newHeight = 80;
}

// ============================================================================
// Icon reordering
// ============================================================================

void CorralWindow::OnIconDrag(int x, int y) {
    if (!isDraggingIcon || draggedIconIndex < 0) return;

    // Find which icon slot we're over
    int newDropTarget = -1;
    POINT pt = { x, y };

    for (int i = 0; i < (int)icons.size(); i++) {
        if (PtInRect(&icons[i].rect, pt)) {
            newDropTarget = i;
            break;
        }
    }

    if (newDropTarget != dropTargetIndex) {
        dropTargetIndex = newDropTarget;
        UpdateLayeredContent();
    }

    // Track if we're outside this corral (for drag-out detection)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    iconDragOutside = !PtInRect(&clientRect, pt);
}

void CorralWindow::OnIconDragEnd() {
    if (!isDraggingIcon || draggedIconIndex < 0 || draggedIconIndex >= (int)config.Files.size()) {
        isDraggingIcon = false;
        draggedIconIndex = -1;
        dropTargetIndex = -1;
        iconDragOutside = false;
        return;
    }

    std::string draggedFile = config.Files[draggedIconIndex];

    // Check if dragged outside the corral
    if (iconDragOutside) {
        POINT screenPt;
        GetCursorPos(&screenPt);

        // Check if dropped on another corral
        App* app = App::GetInstance();
        CorralWindow* targetCorral = nullptr;

        if (app) {
            for (const auto& other : app->GetCorrals()) {
                if (other.get() == this) continue;

                RECT otherRect;
                GetWindowRect(other->GetHWND(), &otherRect);
                if (PtInRect(&otherRect, screenPt)) {
                    targetCorral = other.get();
                    break;
                }
            }
        }

        if (targetCorral) {
            // Move to another corral
            config.Files.erase(config.Files.begin() + draggedIconIndex);
            targetCorral->AddFile(draggedFile);
            LoadFiles();
            if (app) {
                app->SaveConfig();
            }
        }
        else {
            // Dropped outside all corrals - remove from this corral (back to desktop)
            config.Files.erase(config.Files.begin() + draggedIconIndex);
            LoadFiles();
            if (app) {
                app->SaveConfig();
            }
        }
    }
    else if (dropTargetIndex >= 0 && dropTargetIndex != draggedIconIndex && dropTargetIndex < (int)config.Files.size()) {
        // Reorder within corral
        config.Files.erase(config.Files.begin() + draggedIconIndex);

        int insertAt = dropTargetIndex;
        if (draggedIconIndex < dropTargetIndex) {
            insertAt--;
        }
        if (insertAt < 0) insertAt = 0;
        if (insertAt > (int)config.Files.size()) insertAt = (int)config.Files.size();

        config.Files.insert(config.Files.begin() + insertAt, draggedFile);

        LoadFiles();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }

    isDraggingIcon = false;
    draggedIconIndex = -1;
    dropTargetIndex = -1;
    iconDragOutside = false;
    UpdateLayeredContent();
}
