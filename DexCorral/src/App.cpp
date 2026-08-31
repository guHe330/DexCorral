/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * App.cpp - Main application controller and message pump
 *
 * Manages application initialization, shutdown, and the main message loop.
 * Handles corral window creation/destruction, application-wide settings,
 * appearance synchronization, and desktop icon management (pushing icons
 * out of the way when they overlap with corrals).
 */

#include "App.h"
#include "CorralWindow.h"
#include "DesktopIcons.h"
#include "IconUtils.h"
#include "DesktopMonitor.h"
#include "HookBridge.h"
#include "CorralHook.h"
#include "Version.h"
#include "../resources/resource.h"
#include "UpdateChecker.h"
#include "Strings.h"
#include "LayoutMath.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <Psapi.h>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <stdexcept>
#include <utility>

// Debug log defined in dllmain.cpp (same DLL) — gated by the DebugLogging
// config flag (bootstrapped from config.json before the App loads it).
void DllLog(const wchar_t* format, ...);

// Timer ID for retrying Shell_NotifyIconW(NIM_ADD) when the shell isn't ready at startup.
static const UINT_PTR TRAY_RETRY_TIMER = 1;

// Timer for deferred catch-all adoption of new desktop files. Periodic: keeps
// firing while Explorer's inline rename edit box is open, killed once the
// pending files have been adopted.
static const UINT_PTR ADOPTION_TIMER = 2;
static const UINT ADOPTION_POLL_MS = 800;

// Posted by the mouse hook on a desktop double-click; handled on the app
// thread (hit-testing the desktop ListView is too slow for a WH_MOUSE_LL
// callback). lParam carries the click point as POINTS.
static const UINT WM_QUICKHIDE_DBLCLK = WM_APP + 102;

// Posted by the async update checker when a GitHub Releases query finishes.
// lParam = UpdateCheckResult* (the handler takes ownership and deletes it).
static const UINT WM_UPDATE_CHECK_DONE = WM_APP + 103;

// DesktopMonitor's FileRenamed/FileDeleted callbacks fire on its own background
// ReadDirectoryChangesW threads (see MonitorThread in DesktopMonitor.cpp) — unlike
// FileAdded (deferred through the locked pendingAdoptions queue), these two still need
// to run promptly, not wait out ADOPTION_POLL_MS. Post them here instead of mutating
// corrals/tab.Files directly from that thread: corrals, icons, and Files vectors are
// only ever safe to touch from the thread that owns messageWindow's message loop.
// lParam owns a heap-allocated payload that the handler in MessageWindowProc must free.
static const UINT WM_DESKTOP_FILE_RENAMED = WM_APP + 104;
static const UINT WM_DESKTOP_FILE_DELETED = WM_APP + 105;

// DexCorralShellExt::InvokeCommand (ShellExtension.cpp) runs on EXPLORER'S OWN UI
// thread — Explorer calls IContextMenu::InvokeCommand directly when the user picks
// "New DexCorral"/"New Virtual DexCorral" from the desktop's native right-click menu.
// It must not call CreateCorralAt/CreateVirtualCorralAt itself: those read/mutate
// App::corrals (and create a new window), which belong to this thread. The click
// point rides along as (wParam, lParam) so the corral appears where the user asked.
static const UINT WM_CREATE_CORRAL_AT = WM_APP + 106;
static const UINT WM_CREATE_VIRTUAL_CORRAL_AT = WM_APP + 107;

// Posted by WinEventProc (the system-wide EVENT_SYSTEM_FOREGROUND hook) to re-pin
// every corral to the bottom of the z-order. The hook callback must NOT do this
// work itself: an out-of-context WinEvent callback is delivered like a sent message,
// so it can arrive while this thread is blocked inside a cross-thread SendMessage
// (e.g. DesktopIcons::PositionIconsByPath waiting on Explorer's UI thread). Calling
// SetWindowPos on corrals from there re-enters USER32 on windows owned by Progman —
// which belongs to Explorer's UI thread — and deadlocks both threads, hanging Explorer.
// s_RepinPending coalesces bursts of foreground changes into a single repin.
static const UINT WM_REPIN_CORRALS = WM_APP + 108;
static volatile LONG s_RepinPending = 0;

// Stop event — signaled by DLL_PROCESS_DETACH to tell the App message loop to quit
static HANDLE g_AppStopEvent = nullptr;

// Safe-mode tray notice — armed when the hook refused to install (safe mode)
// and shown as soon as the tray icon is actually visible (the icon may only
// get added after retries during early Explorer startup).
static bool s_SafeModeNoticePending = false;

static void ShowSafeModeNoticeIfPending(TrayIcon *tray)
{
    if (!s_SafeModeNoticePending || !tray || !tray->IsVisible())
        return;
    if (tray->ShowBalloon(Tr(Str::SafeMode_Title), Tr(Str::SafeMode_Body)))
    {
        s_SafeModeNoticePending = false;
        DllLog(L"App: safe mode tray notice shown");
    }
}

// Entry point called from the DLL worker thread
extern "C" int RunApp(HANDLE hStopEvent)
{
    g_AppStopEvent = hStopEvent;
    try {
        App app;
        return app.Run();
    } catch (const std::exception& e) {
        DllLog(L"RunApp: EXCEPTION — %S", e.what());
        return -1;
    } catch (...) {
        DllLog(L"RunApp: UNKNOWN EXCEPTION");
        return -1;
    }
}

App *App::instance = nullptr;

static const wchar_t *MESSAGE_WINDOW_CLASS = L"DexCorralMessageWindow";
App::App() : messageWindow(nullptr)
{
    instance = this;
}

App::~App()
{
    Shutdown();
    instance = nullptr;
}

void App::Initialize()
{
    DllLog(L"App::Initialize: starting");

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
        GetModuleHandleW(nullptr), this);

    // Register with HookBridge so the hook can post notifications here
    HookBridge::SetAppMessageWindow(messageWindow);

    // Register for Explorer restart so we can re-add the tray icon
    wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Register for shell image-list changes (e.g. Recycle Bin full→empty)
    SHChangeNotifyEntry shcnentry = {};
    shcnentry.pidl = nullptr; // all items
    shcnentry.fRecursive = TRUE;
    shellNotifyId = SHChangeNotifyRegister(
        messageWindow,
        SHCNRF_ShellLevel | SHCNRF_NewDelivery,
        SHCNE_UPDATEIMAGE,
        WM_APP + 101, // custom message
        1, &shcnentry);

    // Load configuration
    LoadConfig();

    // Select UI language before any user-visible string is built:
    // config.json "Language" wins; else the installer's choice; else English.
    std::wstring lang = CorralWindow::Utf8ToWide(config.Language);
    if (lang.empty())
        lang = GetInstallerLanguage();
    SetLanguage(lang);

    HookBridge::SetDebugLogging(config.DebugLogging);
    DllLog(L"App::Initialize: config loaded — %zu corrals, DebugLogging=%d",
           config.Corrals.size(), config.DebugLogging ? 1 : 0);

    // Create monitor manager (before corrals)
    monitorManager = std::make_unique<MonitorManager>();

    // Setup mouse hook
    mouseHook = std::make_unique<MouseHook>();
    mouseHook->SetLeftButtonDownCallback([this](POINT pt)
                                         { OnLeftButtonDown(pt); });
    mouseHook->SetLeftButtonUpCallback([this](POINT pt)
                                       { OnLeftButtonUp(pt); });
    mouseHook->SetMouseMoveCallback([this](POINT pt)
                                    { OnMouseMove(pt); });
    // Note: Mousewheel events are NOT hooked - they pass through naturally to windows
    mouseHook->Start();

    // Corrals are pinned to the bottom of the z-order via SetWindowPos(HWND_BOTTOM),
    // but that's a one-time placement, not a persistent style — ordinary activation
    // of some other window can leave a corral sitting above other apps again. Listen
    // for foreground changes anywhere on the desktop and re-pin. WINEVENT_SKIPOWNPROCESS
    // filters out our own SetForegroundWindow calls (e.g. the tray menu's message window).
    foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                     nullptr, WinEventProc, 0, 0,
                                     WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Create tray icon. HookBridge::GetDllModule() is this DLL's own module handle —
    // GetModuleHandleW(nullptr) would resolve to explorer.exe here since this code runs
    // injected into Explorer's process.
    HICON icon = LoadIconW(HookBridge::GetDllModule(), MAKEINTRESOURCE(IDI_APPICON));
    if (!icon)
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    trayIcon = std::make_unique<TrayIcon>(messageWindow, icon, Tr(Str::Tray_Tooltip));
    if (!trayIcon->IsVisible()) {
        // Shell notification area not ready yet (we're loaded very early in Explorer startup).
        // Start a retry timer; WM_TASKBARCREATED will also retry when Explorer is fully up.
        DllLog(L"App::Initialize: tray NIM_ADD pending — starting retry timer");
        SetTimer(messageWindow, TRAY_RETRY_TIMER, 500, nullptr);
    } else {
        DllLog(L"App::Initialize: tray icon added successfully");
    }

    // Hook safe mode: the hook refused to install after repeated Explorer
    // deaths — tell the user why their icons are not being hidden
    if (IsCorralHookSafeMode()) {
        DllLog(L"App::Initialize: hook is in SAFE MODE — desktop icons stay visible this session");
        s_SafeModeNoticePending = true;
        ShowSafeModeNoticeIfPending(trayIcon.get());
    }

    // Restore corrals
    RestoreCorrals();
    DllLog(L"App::Initialize: %zu corrals restored", corrals.size());

    // Restore desktop icons state
    // Default to true if not specified (backward compatibility)
    DesktopIcons::SetIconsVisible(config.DesktopIconsVisible);

    if (corrals.empty())
    {
        // First run: a default catch-all corral in the centre of the primary
        // monitor's work area (work area, not screen: it must clear the taskbar).
        RECT work;
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
        {
            work.left = 0;
            work.top = 0;
            work.right = GetSystemMetrics(SM_CXSCREEN);
            work.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
        POINT centerPt = {(work.left + work.right) / 2, (work.top + work.bottom) / 2};

        CorralWindowConfig defaultConfig = MakeDefaultCorralConfig();
        CorralTabConfig tab = MakeDefaultTabConfig(CorralWindow::WideToUtf8(Tr(Str::Name_Desktop)));
        tab.IsCatchAll = true; // First corral is catch-all
        defaultConfig.Tabs.push_back(tab);

        SIZE size = DefaultCorralSize(centerPt);
        defaultConfig.Left = (double)(centerPt.x - size.cx / 2);
        defaultConfig.Top = (double)(centerPt.y - size.cy / 2);
        defaultConfig.Width = size.cx;
        defaultConfig.Height = size.cy;

        auto corral = std::make_unique<CorralWindow>(defaultConfig);
        corral->Show();
        corrals.push_back(std::move(corral));

        SaveConfig();
    }

    // Enforce at most one catch-all corral (catch-all is optional)
    EnsureCatchAllCorral();

    // Start desktop monitoring
    // NOTE: FileRenamed/FileDeleted fire on DesktopMonitor's own background threads —
    // post across to the app thread instead of calling App::OnDesktopFile* directly (see
    // WM_DESKTOP_FILE_RENAMED comment above). FileAdded is safe to call directly: it only
    // pushes into the mutex-guarded pendingAdoptions queue and arms a timer.
    desktopMonitor = std::make_unique<DesktopMonitor>();
    desktopMonitor->SetFileAddedCallback([this](const std::wstring &fileName)
                                         { OnDesktopFileAdded(fileName); });
    desktopMonitor->SetFileRenamedCallback([this](const std::wstring &oldName, const std::wstring &newName)
                                           {
                                               auto *names = new std::pair<std::wstring, std::wstring>(oldName, newName);
                                               PostMessageW(messageWindow, WM_DESKTOP_FILE_RENAMED, 0, (LPARAM)names);
                                           });
    desktopMonitor->SetFileDeletedCallback([this](const std::wstring &fileName)
                                           {
                                               auto *name = new std::wstring(fileName);
                                               PostMessageW(messageWindow, WM_DESKTOP_FILE_DELETED, 0, (LPARAM)name);
                                           });
    desktopMonitor->Start();

    // Hook is in-process (shell extension) — just update hidden icon list
    UpdateHookHiddenIcons();
    PositionHiddenIconsUnderCorrals();
    HookBridge::RefreshDesktop();

    // Opt-in update check (no-op unless CheckForUpdates is enabled and the 24h
    // throttle has elapsed). Async — never blocks startup.
    StartUpdateCheck(false);

    DllLog(L"App::Initialize: done");
}

void App::Shutdown()
{
    // Unregister shell change notifications
    if (shellNotifyId)
    {
        SHChangeNotifyDeregister(shellNotifyId);
        shellNotifyId = 0;
    }

    if (foregroundHook)
    {
        UnhookWinEvent(foregroundHook);
        foregroundHook = nullptr;
    }

    // Stop desktop monitor first
    if (desktopMonitor)
    {
        desktopMonitor->Stop();
    }

    // Save config before exit
    SaveConfig();

    // Clear hidden icons so hook stops hiding them
    HookBridge::ClearHiddenIcons();
    HookBridge::RefreshDesktop();

    // IMPORTANT: Show desktop icons before exit so user isn't stuck
    DesktopIcons::SetIconsVisible(true);

    // Clean up corrals
    topCorral = nullptr;
    hoverExpandedCorral = nullptr;
    corrals.clear();

    // Stop mouse hook
    if (mouseHook)
    {
        mouseHook->Stop();
    }
}

void App::LoadConfig()
{
    config = Config::Load();
}

void App::SaveConfig()
{
    // Update desktop icons state. Quick-hide is a transient state — persist
    // the visibility the user will get back when quick-hide ends.
    config.DesktopIconsVisible = quickHideActive ? desktopIconsVisibleBeforeQuickHide
                                                 : DesktopIcons::AreIconsVisible();

    // Sync all corral configs from their current window states
    config.Corrals.clear();
    for (auto &corral : corrals)
    {
        corral->SyncConfigFromWindow();
        config.Corrals.push_back(corral->GetConfig());
    }
    Config::Save(config);

    // Update hook hidden icons (hook is always active in shell extension mode)
    UpdateHookHiddenIcons();
    PositionHiddenIconsUnderCorrals();
    HookBridge::RefreshDesktop();
}

void App::RestoreCorrals()
{
    for (auto &corralConfig : config.Corrals)
    {
        // Skip corrals with invalid dimensions (corrupted config)
        if (corralConfig.Width < 50 || corralConfig.Height < 50)
        {
            corralConfig.Width = 300;
            corralConfig.Height = 200;
        }
        if (corralConfig.Left < -1000 || corralConfig.Top < -1000)
        {
            corralConfig.Left = GetSystemMetrics(SM_CXSCREEN) / 2 - 150;
            corralConfig.Top = GetSystemMetrics(SM_CYSCREEN) / 2 - 100;
        }

        auto corral = std::make_unique<CorralWindow>(corralConfig);
        corral->Show();
        corrals.push_back(std::move(corral));
    }
}

void App::RemoveCorral(CorralWindowConfig *configToRemove)
{
    for (auto it = corrals.begin(); it != corrals.end(); ++it)
    {
        if (&(*it)->GetConfig() == configToRemove)
        {
            ForgetCorral(it->get());
            corrals.erase(it);
            break;
        }
    }
    SaveConfig();
}

void App::RepinBand(CorralWindow *newTop)
{
    if (newTop)
        topCorral = newTop;

    // Corrals stay collectively below ordinary windows; this only fixes their
    // order *within* that band. SetWindowPos(HWND_BOTTOM) sinks a window below
    // everything, so sending them bottom-ward in top-to-bottom order leaves the
    // first one processed on top.
    auto sink = [](CorralWindow *c)
    {
        if (c && c->GetHWND())
            SetWindowPos(c->GetHWND(), HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    };

    if (topCorral)
        sink(topCorral);

    // The rest newest-first, so a corral created earlier never buries a newer one.
    for (auto it = corrals.rbegin(); it != corrals.rend(); ++it)
    {
        if (it->get() != topCorral)
            sink(it->get());
    }
}

void App::ForgetCorral(CorralWindow *corral)
{
    if (topCorral == corral)
        topCorral = nullptr;
    if (hoverExpandedCorral == corral)
        hoverExpandedCorral = nullptr;
}

bool App::BeginHoverExpand(CorralWindow *corral)
{
    if (!corral)
        return false;
    // Suppress outright rather than queueing: a sliver of a neighbour peeking
    // out beside the expanded corral must stay inert. (ForgetCorral clears the
    // owner when its window goes away, so a stale pointer can't wedge this.)
    if (hoverExpandedCorral && hoverExpandedCorral != corral)
        return false;
    hoverExpandedCorral = corral;
    return true;
}

void App::EndHoverExpand(CorralWindow *corral)
{
    if (hoverExpandedCorral == corral)
        hoverExpandedCorral = nullptr;
}

void App::RemoveFileFromOtherCorrals(const std::wstring &fileName, CorralTabConfig *exceptTab)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    bool changed = false;
    for (auto &corral : corrals)
    {
        auto &corralConfig = corral->GetConfig();
        for (auto &tab : corralConfig.Tabs)
        {
            if (&tab != exceptTab)
            {
                auto it = std::find(tab.Files.begin(), tab.Files.end(), fileNameStr);
                if (it != tab.Files.end())
                {
                    tab.Files.erase(it);
                    changed = true;
                }
            }
        }
    }

    if (changed)
    {
        SaveConfig();
        RefreshAllCorrals();
    }
}

void App::RefreshAllCorrals()
{
    for (auto &corral : corrals)
    {
        corral->LoadFiles();
    }
}

void App::RefreshAllCorralBackgrounds()
{
    for (auto &corral : corrals)
    {
        corral->RecalculateLayout();
    }
}

void App::SetDefaultColorHex(const std::string &colorHex)
{
    config.DefaultColorHex = colorHex;
}

void App::SetDefaultAppearance(const AppearanceSettings &settings)
{
    config.DefaultTitleBarHeight = settings.TitleBarHeight;
    config.DefaultHeaderFontName = settings.HeaderFontName;
    config.DefaultHeaderFontSize = settings.HeaderFontSize;
    config.DefaultHeaderFontColor = settings.HeaderFontColor;
    config.DefaultHeaderFontOpacity = ChromeAlpha::ClampTextOpacity(settings.HeaderFontOpacity);
    config.DefaultHeaderOpacity = ChromeAlpha::ClampHeaderOpacity(settings.HeaderOpacity);
    config.DefaultBorderOpacity = ChromeAlpha::ClampBorderOpacity(settings.BorderOpacity);
    config.DefaultIconOpacity = settings.IconOpacity;
    config.DefaultIconLabelOpacity = ChromeAlpha::ClampTextOpacity(settings.IconLabelOpacity);
    config.DefaultIconTintColor = settings.IconTintColor;
    config.DefaultIconTintStrength = settings.IconTintStrength;
    config.DefaultIconSpacingXPercent = settings.IconSpacingXPercent;
    config.DefaultIconSpacingYPercent = settings.IconSpacingYPercent;
}

void App::ApplyColorToAllCorrals(const std::string &colorHex)
{
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            tab.ColorHex = colorHex;
        }
        corral->RecalculateLayout();
    }
}

void App::ApplyAppearanceToAllCorrals(const AppearanceSettings &settings,
                                      const AppearanceApplyFlags &apply)
{
    for (auto &corral : corrals)
    {
        auto &cfg = corral->GetConfig();

        if (apply.Color)
        {
            for (auto &tab : cfg.Tabs)
            {
                tab.ColorHex = settings.ColorHex;
            }
        }
        if (apply.TitleBarHeight)
        {
            cfg.TitleBarHeight = settings.TitleBarHeight;
        }
        if (apply.HeaderOpacity)
        {
            cfg.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(settings.HeaderOpacity);
            corral->SetCurrentHeaderOpacity(cfg.HeaderOpacity);
        }
        if (apply.BorderOpacity)
        {
            cfg.BorderOpacity = ChromeAlpha::ClampBorderOpacity(settings.BorderOpacity);
            corral->SetCurrentBorderOpacity(cfg.BorderOpacity);
        }
        if (apply.Font || apply.FontColor || apply.FontOpacity)
        {
            // Font settings are per-tab; the dialog edits the active tab, so
            // that is where they land here too.
            int activeIdx = cfg.ActiveTabIndex;
            if (activeIdx >= 0 && activeIdx < (int)cfg.Tabs.size())
            {
                auto &tab = cfg.Tabs[activeIdx];
                if (apply.Font)
                {
                    tab.HeaderFontName = settings.HeaderFontName;
                    tab.HeaderFontSize = settings.HeaderFontSize;
                }
                if (apply.FontColor)
                    tab.HeaderFontColor = settings.HeaderFontColor;
                if (apply.FontOpacity)
                    tab.HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(settings.HeaderFontOpacity);
            }
        }
        if (apply.IconOpacity)
        {
            cfg.IconOpacity = settings.IconOpacity;
            corral->SetCurrentOpacity(settings.IconOpacity);
        }
        if (apply.IconLabelOpacity)
        {
            cfg.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(settings.IconLabelOpacity);
        }
        if (apply.Tint)
        {
            cfg.IconTintColor = settings.IconTintColor;
            cfg.IconTintStrength = settings.IconTintStrength;
            corral->SetCurrentTintStrength(settings.IconTintStrength);
        }
        if (apply.Spacing)
        {
            cfg.IconSpacingXPercent = settings.IconSpacingXPercent;
            cfg.IconSpacingYPercent = settings.IconSpacingYPercent;
        }

        // Every branch above changes something the layout or the paint depends
        // on, so this is unconditional.
        corral->RecalculateLayout();
    }
}

void App::ToggleDesktopIcons()
{
    bool currentlyVisible = DesktopIcons::AreIconsVisible();
    DesktopIcons::SetIconsVisible(!currentlyVisible);
    SaveConfig();
}

void App::ToggleQuickHide()
{
    quickHideActive = !quickHideActive;

    if (quickHideActive)
    {
        // Remember the native-icon state so the restore puts back exactly
        // what the user had (icons may already be hidden via the tray toggle)
        desktopIconsVisibleBeforeQuickHide = DesktopIcons::AreIconsVisible();
        if (desktopIconsVisibleBeforeQuickHide)
        {
            DesktopIcons::SetIconsVisible(false);
        }
        for (auto &corral : corrals)
        {
            if (!corral->GetConfig().ExcludeFromQuickHide)
            {
                corral->StartQuickHide();
            }
        }
        DllLog(L"App::ToggleQuickHide: hidden (icons were %s)",
               desktopIconsVisibleBeforeQuickHide ? L"visible" : L"hidden");
    }
    else
    {
        if (desktopIconsVisibleBeforeQuickHide)
        {
            DesktopIcons::SetIconsVisible(true);
        }
        for (auto &corral : corrals)
        {
            corral->StartQuickShow(); // No-op for corrals that were not hidden
        }
        DllLog(L"App::ToggleQuickHide: restored");
    }
}

bool App::IsPointOnEmptyDesktop(POINT pt, bool checkIcons)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd)
        return false;

    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, _countof(cls));

    bool onDesktop = false;
    if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0)
    {
        // Wallpaper layer — hit directly when the icon ListView is hidden
        onDesktop = true;
    }
    else if (wcscmp(cls, L"SysListView32") == 0 || wcscmp(cls, L"SHELLDLL_DefView") == 0)
    {
        // Must be Explorer's desktop view, not some other app's ListView.
        // (Deliberately not a GetParent walk: GetParent returns the owner for
        // popups, and corral windows are owned by Progman.)
        HWND root = GetAncestor(hwnd, GA_ROOT);
        wchar_t rootCls[64] = {};
        GetClassNameW(root, rootCls, _countof(rootCls));
        onDesktop = (root == GetShellWindow() ||
                     wcscmp(rootCls, L"Progman") == 0 || wcscmp(rootCls, L"WorkerW") == 0);
    }
    if (!onDesktop)
        return false;

    // The hook filters LVM_HITTEST, so corral-owned (parked) icons never count
    return !checkIcons || !DesktopIcons::IsPointOnIcon(pt.x, pt.y);
}

void App::ToggleShortcutArrows()
{
    bool currentlyHidden = DesktopIcons::AreShortcutArrowsHidden();
    bool newState = !currentlyHidden;

    // Confirm with user since this requires Explorer restart
    const wchar_t *message = newState
                                 ? Tr(Str::Arrow_HideConfirm)
                                 : Tr(Str::Arrow_RestoreConfirm);

    int result = MessageBoxW(nullptr, message, Tr(Str::App_Name), MB_YESNO | MB_ICONQUESTION);
    if (result != IDYES)
    {
        return;
    }

    if (DesktopIcons::SetShortcutArrowsHidden(newState))
    {
        config.HideShortcutArrows = newState;
        SaveConfig();
        DesktopIcons::RestartExplorer();
    }
    else
    {
        MessageBoxW(nullptr, Tr(Str::Arrow_ChangeFailed), Tr(Str::App_Name), MB_OK | MB_ICONWARNING);
    }
}

// Resolve a desktop file name to its full path (user desktop first, then
// public desktop; defaults to the user desktop path if neither exists)
static std::wstring ResolveDesktopFilePath(const std::wstring &fileName)
{
    if (fileName.empty())
        return L""; // never resolve to the desktop folder itself

    wchar_t buf[MAX_PATH];
    std::wstring userPath, pubPath;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, buf)))
        userPath = std::wstring(buf) + L"\\" + fileName;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, buf)))
        pubPath = std::wstring(buf) + L"\\" + fileName;

    if (!userPath.empty() && GetFileAttributesW(userPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        return userPath;
    if (!pubPath.empty() && GetFileAttributesW(pubPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        return pubPath;
    return userPath.empty() ? fileName : userPath;
}

std::vector<HiddenIconInfo> App::CollectCorralIconIdentities() const
{
    // Collect all icons across all corrals and ALL tabs (not just active tab -
    // inactive tab icons count too). Each entry carries the display name plus
    // the canonical parsing name (full path or ::{CLSID}) so consumers can
    // match items unambiguously even when display names collide.
    std::vector<HiddenIconInfo> result;
    for (const auto &corral : corrals)
    {
        for (const auto &tab : corral->GetConfig().Tabs)
        {
            if (tab.IsVirtual)
                continue; // Virtual tabs don't own desktop icons

            for (const auto &fileUtf8 : tab.Files)
            {
                if (fileUtf8.empty())
                    continue;

                HiddenIconInfo info;

                // Special icon: resolve CLSID to display name; parsing name is ::{CLSID}
                if (CorralWindow::IsSpecialIconEntry(fileUtf8))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(fileUtf8);
                    info.displayName = DesktopIcons::GetSpecialIconDisplayName(clsid);
                    info.parsingName = L"::" + clsid;
                    result.push_back(std::move(info));
                    continue;
                }

                // Convert UTF-8 filename to wide
                int size = MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), nullptr, 0);
                std::wstring wName(size, 0);
                MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), &wName[0], size);

                info.parsingName = ResolveDesktopFilePath(wName);

                // Shell display name matches the desktop ListView's item text
                // (respects "Hide extensions for known file types")
                info.displayName = DesktopIcons::GetShellDisplayName(info.parsingName);
                result.push_back(std::move(info));
            }
        }
    }
    return result;
}

// How long a renamed icon's old identity stays hidden alongside the new one.
// Covers the shell's asynchronous processing of the rename, including slow
// cases (OneDrive-backed desktops).
static const DWORD TRANSIENT_HIDDEN_MS = 5000;

void App::AddTransientHiddenIcon(const std::wstring &displayName, const std::wstring &parsingName)
{
    if (displayName.empty() && parsingName.empty())
        return;

    std::lock_guard<std::mutex> lock(transientHiddenLock);
    transientHiddenIcons.push_back({{displayName, parsingName}, GetTickCount() + TRANSIENT_HIDDEN_MS});
}

void App::UpdateHookHiddenIcons()
{
    auto icons = CollectCorralIconIdentities();

    // Append unexpired transition aliases (old identities of just-renamed
    // icons) so a desktop item the shell hasn't updated yet stays hidden
    {
        std::lock_guard<std::mutex> lock(transientHiddenLock);
        DWORD now = GetTickCount();
        transientHiddenIcons.erase(
            std::remove_if(transientHiddenIcons.begin(), transientHiddenIcons.end(),
                           [now](const TransientHiddenIcon &t)
                           { return (LONG)(t.expiresAtTick - now) <= 0; }),
            transientHiddenIcons.end());
        for (const auto &t : transientHiddenIcons)
        {
            icons.push_back(t.icon);
        }
    }

    HookBridge::UpdateHiddenIcons(icons);
}

void App::PositionHiddenIconsUnderCorrals()
{
    // Position hidden icons at per-icon screen positions matching their visual
    // location in the corral. Icons visible in the corral viewport get positioned
    // at their actual screen coords (under the corral window). Icons scrolled out
    // of view get positioned just outside the corral edge. If DexCorral crashes,
    // icons reappear near where the corral was.
    std::vector<IconPositionRequest> requests;

    for (const auto &corral : corrals)
    {
        // Get per-icon positions from the active tab (already in ListView client coords)
        auto iconPositions = corral->GetIconScreenPositions();
        requests.insert(requests.end(), iconPositions.begin(), iconPositions.end());

        // For non-active tabs, position icons at corral center (they're not rendered)
        RECT r;
        if (!GetWindowRect(corral->GetHWND(), &r))
            continue;
        HWND hListView = DesktopIcons::GetDesktopListView();
        POINT center = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
        if (hListView)
        {
            ScreenToClient(hListView, &center);
        }

        int activeTabIndex = corral->GetConfig().ActiveTabIndex;
        for (int t = 0; t < (int)corral->GetConfig().Tabs.size(); t++)
        {
            if (t == activeTabIndex)
                continue; // Already handled by GetIconScreenPositions
            const auto &tab = corral->GetConfig().Tabs[t];
            if (tab.IsVirtual)
                continue;
            for (const auto &fileUtf8 : tab.Files)
            {
                IconPositionRequest req;
                if (CorralWindow::IsSpecialIconEntry(fileUtf8))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(fileUtf8);
                    req.displayName = DesktopIcons::GetSpecialIconDisplayName(clsid);
                    req.parsingName = L"::" + clsid;
                }
                else
                {
                    int size = MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), nullptr, 0);
                    std::wstring wName(size, 0);
                    MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), &wName[0], size);
                    req.parsingName = ResolveDesktopFilePath(wName);
                    req.displayName = DesktopIcons::GetShellDisplayName(req.parsingName);
                }
                if (!req.displayName.empty() || !req.parsingName.empty())
                {
                    req.pt = {center.x, center.y};
                    requests.push_back(std::move(req));
                }
            }
        }
    }

    if (!requests.empty())
    {
        DesktopIcons::PositionIconsByPath(requests);
    }
}

// ============================================================================
// Desktop icon push-out-of-way support
// ============================================================================

void App::CacheDesktopIconPositions()
{
    cachedDesktopIconPositions = DesktopIcons::GetAllIconsWithIdentity();

    // Remove icons that belong to corrals (they're managed, not free).
    // Compare canonically (parsing name) when both sides have one, so a free
    // name-twin of a corral-owned icon is not filtered out by mistake.
    auto corralIcons = CollectCorralIconIdentities();
    auto isCorralOwned = [&corralIcons](const DesktopIconInfo &icon)
    {
        for (const auto &owned : corralIcons)
        {
            if (!icon.parsingName.empty() && !owned.parsingName.empty())
            {
                if (_wcsicmp(owned.parsingName.c_str(), icon.parsingName.c_str()) == 0)
                    return true;
            }
            else if (!icon.displayName.empty() && !owned.displayName.empty() &&
                     _wcsicmp(owned.displayName.c_str(), icon.displayName.c_str()) == 0)
            {
                return true;
            }
        }
        return false;
    };
    cachedDesktopIconPositions.erase(
        std::remove_if(cachedDesktopIconPositions.begin(), cachedDesktopIconPositions.end(), isCorralOwned),
        cachedDesktopIconPositions.end());

    desktopIconCacheValid = true;
}

void App::InvalidateDesktopIconCache()
{
    desktopIconCacheValid = false;
    cachedDesktopIconPositions.clear();
}

std::vector<RECT> App::GetAllCorralRects() const
{
    // Returns corral rects in desktop ListView client coordinates
    // (LVM_GETITEMPOSITION returns client coords, so we need to match)
    HWND hListView = DesktopIcons::GetDesktopListView();
    std::vector<RECT> rects;
    for (const auto &corral : corrals)
    {
        RECT r;
        if (GetWindowRect(corral->GetHWND(), &r))
        {
            if (hListView)
            {
                // Convert screen coords to ListView client coords
                POINT topLeft = {r.left, r.top};
                POINT bottomRight = {r.right, r.bottom};
                ScreenToClient(hListView, &topLeft);
                ScreenToClient(hListView, &bottomRight);
                r.left = topLeft.x;
                r.top = topLeft.y;
                r.right = bottomRight.x;
                r.bottom = bottomRight.y;
            }
            rects.push_back(r);
        }
    }
    return rects;
}

static bool RectsOverlap(const RECT &a, const RECT &b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

/**
 * Pushes desktop icons out of the way if they overlap with corral windows.
 *
 * Algorithm:
 * 1. Cache current desktop icon positions if not already cached
 * 2. For each cached icon that overlaps with a corral:
 *    - Calculate distance from icon center to each edge of the overlapping corral
 *    - Try pushing the icon out from the nearest edge first (shortest distance)
 *    - Search for a free position one grid step away (75px increments)
 *    - Continue stepping outward until a free position is found
 * 3. Update Explorer with new positions for moved icons
 *
 * Uses a 75x75px icon footprint for collision detection (standard desktop icon size).
 * Icons already off-screen (< -1000) are skipped. Icons are pushed to the nearest
 * free grid position rather than to arbitrary coordinates, maintaining alignment.
 */
void App::PushDesktopIconsFromCorrals()
{
    if (!desktopIconCacheValid)
    {
        CacheDesktopIconPositions();
    }

    auto corralRects = GetAllCorralRects();
    if (corralRects.empty())
        return;

    /// Icon bounding box size (approximate desktop icon footprint)
    const int ICON_W = 75;
    const int ICON_H = 75;
    const int STEP = 75; /// Push step size (one grid jump)

    // Get screen bounds in ListView client coordinates
    HWND hListView = DesktopIcons::GetDesktopListView();
    int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenRight = screenLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenBottom = screenTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (hListView)
    {
        POINT tl = {screenLeft, screenTop};
        POINT br = {screenRight, screenBottom};
        ScreenToClient(hListView, &tl);
        ScreenToClient(hListView, &br);
        screenLeft = tl.x;
        screenTop = tl.y;
        screenRight = br.x;
        screenBottom = br.y;
    }

    // Helper: check if a position is free (no corral overlap, no icon overlap, on screen)
    auto isPositionFree = [&](int x, int y, const DesktopIconInfo *skip) -> bool
    {
        if (x < screenLeft || y < screenTop ||
            x + ICON_W > screenRight || y + ICON_H > screenBottom)
            return false;
        RECT r = {x, y, x + ICON_W, y + ICON_H};
        for (const auto &cr : corralRects)
        {
            if (RectsOverlap(r, cr))
                return false;
        }
        for (const auto &other : cachedDesktopIconPositions)
        {
            if (&other == skip)
                continue;
            if (other.pt.x < -1000 || other.pt.y < -1000)
                continue;
            RECT otherRect = {other.pt.x, other.pt.y, other.pt.x + ICON_W, other.pt.y + ICON_H};
            if (RectsOverlap(r, otherRect))
                return false;
        }
        return true;
    };

    bool anyMoved = false;
    std::vector<IconPositionRequest> toMove;

    for (auto &icon : cachedDesktopIconPositions)
    {
        POINT2D &pos = icon.pt;

        // Skip icons far off-screen (already hidden)
        if (pos.x < -1000 || pos.y < -1000)
            continue;

        RECT iconRect = {pos.x, pos.y, pos.x + ICON_W, pos.y + ICON_H};

        // Find which corral this icon overlaps (if any)
        const RECT *overlappingCorral = nullptr;
        for (const auto &cr : corralRects)
        {
            if (RectsOverlap(iconRect, cr))
            {
                overlappingCorral = &cr;
                break;
            }
        }
        if (!overlappingCorral)
            continue;

        // Find nearest edge of the overlapping corral and push one step past it
        // Calculate distance to each edge from icon center
        int iconCenterX = pos.x + ICON_W / 2;
        int iconCenterY = pos.y + ICON_H / 2;
        int distLeft = iconCenterX - overlappingCorral->left;
        int distRight = overlappingCorral->right - iconCenterX;
        int distTop = iconCenterY - overlappingCorral->top;
        int distBottom = overlappingCorral->bottom - iconCenterY;

        // Try each direction, ordered by shortest distance to edge
        struct PushDir
        {
            int dx;
            int dy;
            int dist;
        };
        PushDir dirs[4] = {
            {-1, 0, distLeft},
            {1, 0, distRight},
            {0, -1, distTop},
            {0, 1, distBottom}};
        // Sort by distance (nearest edge first)
        for (int i = 0; i < 3; i++)
            for (int j = i + 1; j < 4; j++)
                if (dirs[j].dist < dirs[i].dist)
                    std::swap(dirs[i], dirs[j]);

        bool found = false;
        int newX = pos.x, newY = pos.y;

        for (const auto &dir : dirs)
        {
            for (int step = 1; step <= 40; step++)
            {
                int candidateX = pos.x + dir.dx * STEP * step;
                int candidateY = pos.y + dir.dy * STEP * step;

                if (isPositionFree(candidateX, candidateY, &icon))
                {
                    newX = candidateX;
                    newY = candidateY;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }

        if (newX != pos.x || newY != pos.y)
        {
            // The snapshot carries each icon's identity, so the move targets
            // exactly this icon even if another shares its display name
            toMove.push_back({icon.displayName, icon.parsingName, {newX, newY}});
            // Update cache so subsequent icons see the new position
            pos.x = newX;
            pos.y = newY;
            anyMoved = true;
        }
    }

    // Apply all moves
    DesktopIcons::PositionIconsByPath(toMove);
    if (anyMoved)
    {
        HookBridge::RefreshDesktop();
    }
}

void App::OnLeftButtonDown(POINT pt)
{
    // Runs inside the WH_MOUSE_LL callback — keep it cheap. Pair consecutive
    // clicks into a double-click here; the expensive empty-desktop validation
    // happens on the app thread (WM_QUICKHIDE_DBLCLK handler).
    DWORD now = GetTickCount();
    bool isDoubleClick =
        lastClickTick != 0 &&
        (now - lastClickTick) <= GetDoubleClickTime() &&
        abs(pt.x - lastClickPt.x) <= GetSystemMetrics(SM_CXDOUBLECLK) &&
        abs(pt.y - lastClickPt.y) <= GetSystemMetrics(SM_CYDOUBLECLK);

    if (isDoubleClick)
    {
        lastClickTick = 0; // A triple-click must not toggle twice
        POINTS pts = {(SHORT)pt.x, (SHORT)pt.y};
        PostMessageW(messageWindow, WM_QUICKHIDE_DBLCLK, 0, MAKELPARAM(pts.x, pts.y));
    }
    else
    {
        lastClickTick = now;
        lastClickPt = pt;
    }
}

void CALLBACK App::WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                LONG idObject, LONG idChild, DWORD idEventThread, DWORD idEventTime)
{
    if (event != EVENT_SYSTEM_FOREGROUND || idObject != OBJID_WINDOW || idChild != CHILDID_SELF || !hwnd)
        return;

    App *app = App::instance;
    if (!app || !app->messageWindow)
        return;

    // Corrals never legitimately become the foreground window (WM_MOUSEACTIVATE
    // returns MA_NOACTIVATE), but guard anyway so this can't fight a stray activation.
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, _countof(cls));
    if (wcscmp(cls, L"DexCorralWindowClass") == 0)
        return;

    // Post, never act: see WM_REPIN_CORRALS. Doing SetWindowPos here would run
    // inside the callback's (possibly re-entrant, possibly SendMessage-blocked)
    // context and can wedge Explorer.
    if (InterlockedCompareExchange(&s_RepinPending, 1, 0) == 0)
    {
        if (!PostMessageW(app->messageWindow, WM_REPIN_CORRALS, 0, 0))
            InterlockedExchange(&s_RepinPending, 0);
    }
}

void App::OnLeftButtonUp(POINT pt)
{
    // Global mouse up - could be used for drag-end if needed
    // But currently CorralWindow handles its own drag
}

void App::OnMouseMove(POINT pt)
{
    // Handle mouse move if needed
}

void App::ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 3, Tr(Str::Menu_About));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1, Tr(Str::Menu_CreateNewCorral));
    AppendMenuW(menu, MF_STRING, 5, Tr(Str::Menu_NewVirtualCorral));

    // Icon visibility toggle
    UINT flags = DesktopIcons::AreIconsVisible() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | flags, 2, Tr(Str::Menu_ShowDesktopIcons));

    // Quick-hide toggle (same as double-clicking an empty desktop spot)
    UINT quickHideFlags = quickHideActive ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | quickHideFlags, 6, Tr(Str::Menu_QuickHideEverything));

    // Update check (opt-in)
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT updateFlags = config.CheckForUpdates ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | updateFlags, 7, Tr(Str::Menu_CheckUpdatesAuto));
    AppendMenuW(menu, MF_STRING, 8, Tr(Str::Menu_CheckUpdatesNow));

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(messageWindow);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, messageWindow, nullptr);
    PostMessageW(messageWindow, WM_NULL, 0, 0);

    DestroyMenu(menu);

    switch (cmd)
    {
    case 3:
        ShowAbout();
        break;
    case 1:
    {
        POINT cursorPt;
        GetCursorPos(&cursorPt);
        ShowCreationMenu(cursorPt);
        break;
    }
    case 2:
        ToggleDesktopIcons();
        break;
    case 6:
        ToggleQuickHide();
        break;
    case 5:
    {
        POINT cursorPt;
        GetCursorPos(&cursorPt);
        CreateVirtualCorralAt(cursorPt);
        break;
    }
    case 7:
        config.CheckForUpdates = !config.CheckForUpdates;
        SaveConfig();
        if (config.CheckForUpdates)
            StartUpdateCheck(true); // immediate confirmation that checking works
        break;
    case 8:
        StartUpdateCheck(true);
        break;
    }
}

void App::ShowCreationMenu(POINT pt)
{
    CreateCorralAt(pt);
}

void App::ShowAbout()
{
    // Build the About message with GPL-3.0 license notice
    std::wstring aboutText = TrFmt(Str::About_Body, DEXCORRAL_VERSION);

    MessageBoxW(messageWindow, aboutText.c_str(), Tr(Str::Title_About), MB_OK | MB_ICONINFORMATION);
}

void App::StartUpdateCheck(bool userInitiated)
{
    if (!userInitiated)
    {
        // Automatic check: only when opted in, and at most once per 24h.
        if (!config.CheckForUpdates)
            return;
        long long now = (long long)time(nullptr);
        const long long kDaySeconds = 24LL * 60 * 60;
        if (config.LastUpdateCheckEpoch != 0 && now - config.LastUpdateCheckEpoch < kDaySeconds)
            return;
        config.LastUpdateCheckEpoch = now;
        SaveConfig();
    }

    UpdateChecker::StartAsyncCheck(messageWindow, WM_UPDATE_CHECK_DONE, userInitiated);
}

void App::CreateCorralAt(POINT desiredCenter, HWND exclude)
{
    CorralWindowConfig newConfig = MakeDefaultCorralConfig();
    newConfig.Tabs.push_back(MakeDefaultTabConfig(CorralWindow::WideToUtf8(Tr(Str::Name_NewCorral))));

    SIZE size = DefaultCorralSize(desiredCenter);
    POINT topLeft = {desiredCenter.x - size.cx / 2, desiredCenter.y - size.cy / 2};
    POINT center = FindNearestFreeCorralPosition(topLeft, size.cx, size.cy, exclude);

    newConfig.Left = (double)(center.x - size.cx / 2);
    newConfig.Top = (double)(center.y - size.cy / 2);
    newConfig.Width = size.cx;
    newConfig.Height = size.cy;

    auto corral = std::make_unique<CorralWindow>(newConfig);
    corral->Show();
    corrals.push_back(std::move(corral));
    SaveConfig();
}

UINT App::DpiForPoint(POINT pt)
{
    // GetDpiForMonitor lives in Shcore.dll (Win8.1+); bound lazily so no extra
    // import library is needed and pre-8.1 simply falls back to 96.
    typedef HRESULT(WINAPI * PFN_GetDpiForMonitor)(HMONITOR, int, UINT *, UINT *);
    static PFN_GetDpiForMonitor pfn = []() -> PFN_GetDpiForMonitor
    {
        HMODULE h = LoadLibraryW(L"Shcore.dll");
        return h ? (PFN_GetDpiForMonitor)GetProcAddress(h, "GetDpiForMonitor") : nullptr;
    }();

    if (pfn)
    {
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0, dpiY = 0;
        if (hMon && SUCCEEDED(pfn(hMon, 0 /*MDT_EFFECTIVE_DPI*/, &dpiX, &dpiY)) && dpiX)
            return dpiX;
    }
    return 96;
}

SIZE App::DefaultCorralSize(POINT nearPt)
{
    // 300x200 at 96 DPI. Everything drawn inside the corral scales with the
    // monitor DPI, so the frame has to as well or it comes out undersized.
    const UINT dpi = DpiForPoint(nearPt);
    SIZE s = {MulDiv(300, (int)dpi, 96), MulDiv(200, (int)dpi, 96)};

    // Never seed a corral larger than a third of the work area it lands on.
    MONITORINFO mi = {sizeof(mi)};
    HMONITOR hMon = MonitorFromPoint(nearPt, MONITOR_DEFAULTTONEAREST);
    if (hMon && GetMonitorInfoW(hMon, &mi))
    {
        LONG maxW = (mi.rcWork.right - mi.rcWork.left) / 3;
        LONG maxH = (mi.rcWork.bottom - mi.rcWork.top) / 3;
        if (maxW > 0 && s.cx > maxW)
            s.cx = maxW;
        if (maxH > 0 && s.cy > maxH)
            s.cy = maxH;
    }
    return s;
}

CorralWindowConfig App::MakeDefaultCorralConfig() const
{
    CorralWindowConfig c;
    c.TitleBarHeight = config.DefaultTitleBarHeight;
    c.HeaderOpacity = config.DefaultHeaderOpacity;
    c.BorderOpacity = config.DefaultBorderOpacity;
    c.IconOpacity = config.DefaultIconOpacity;
    c.IconLabelOpacity = config.DefaultIconLabelOpacity;
    c.IconTintColor = config.DefaultIconTintColor;
    c.IconTintStrength = config.DefaultIconTintStrength;
    c.IconSpacingXPercent = config.DefaultIconSpacingXPercent;
    c.IconSpacingYPercent = config.DefaultIconSpacingYPercent;
    return c;
}

CorralTabConfig App::MakeDefaultTabConfig(const std::string &title) const
{
    CorralTabConfig t;
    t.Title = title;
    t.ColorHex = config.DefaultColorHex;
    t.HeaderFontName = config.DefaultHeaderFontName;
    t.HeaderFontSize = config.DefaultHeaderFontSize;
    t.HeaderFontColor = config.DefaultHeaderFontColor;
    t.HeaderFontOpacity = config.DefaultHeaderFontOpacity;
    return t;
}

POINT App::FindNearestFreeCorralPosition(POINT desiredTopLeft, int width, int height, HWND exclude)
{
    // Work area of the monitor under the desired point (falls back to primary).
    RECT work;
    POINT probe = {desiredTopLeft.x + width / 2, desiredTopLeft.y + height / 2};
    HMONITOR hMon = MonitorFromPoint(probe, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (hMon && GetMonitorInfoW(hMon, &mi))
        work = mi.rcWork;
    else if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
    {
        work.left = 0;
        work.top = 0;
        work.right = GetSystemMetrics(SM_CXSCREEN);
        work.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    // Existing corral rects (current size, so rolled-up/expanded state is honoured),
    // excluding the source window so a detached tab may sit right beside it.
    // Desktop icons are deliberately not avoided: PushDesktopIconsFromCorrals
    // already moves them out from under a corral.
    std::vector<RECT> existing;
    for (const auto &corral : corrals)
    {
        if (!corral || corral->GetHWND() == exclude)
            continue;
        RECT r;
        if (GetWindowRect(corral->GetHWND(), &r))
            existing.push_back(r);
    }

    // Past roughly one corral-diagonal of displacement, landing where the user
    // asked (overlapping) beats teleporting across the monitor.
    const int maxShift = width + height;

    POINT topLeft = LayoutMath::FindNearestFreeTopLeft(desiredTopLeft, width, height,
                                                       work, existing, maxShift);
    return {topLeft.x + width / 2, topLeft.y + height / 2};
}

void App::CreateVirtualCorralAt(POINT desiredCenter)
{
    // Show folder browser dialog
    IFileDialog *pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return;

    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (SUCCEEDED(hr))
    {
        hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }

    if (SUCCEEDED(hr))
    {
        pfd->SetTitle(Tr(Str::Dlg_SelectVirtualFolder));
    }

    hr = pfd->Show(messageWindow);
    std::wstring folderPath;
    if (SUCCEEDED(hr))
    {
        IShellItem *psi = nullptr;
        hr = pfd->GetResult(&psi);
        if (SUCCEEDED(hr))
        {
            PWSTR pszPath = nullptr;
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (SUCCEEDED(hr))
            {
                folderPath = pszPath;
                CoTaskMemFree(pszPath);
            }
            psi->Release();
        }
    }
    pfd->Release();

    if (folderPath.empty())
        return;

    // Validate folder
    DWORD attrs = GetFileAttributesW(folderPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBoxW(messageWindow, Tr(Str::Err_InvalidFolderSelected), Tr(Str::Title_Error), MB_OK | MB_ICONWARNING);
        return;
    }

    // Check for network path
    if (folderPath.length() >= 2 && folderPath[0] == L'\\' && folderPath[1] == L'\\')
    {
        MessageBoxW(messageWindow, Tr(Str::Err_NetworkPaths), Tr(Str::Title_Error), MB_OK | MB_ICONWARNING);
        return;
    }

    if (folderPath.length() >= 2 && folderPath[1] == L':')
    {
        wchar_t rootPath[4] = {folderPath[0], L':', L'\\', L'\0'};
        if (GetDriveTypeW(rootPath) == DRIVE_REMOTE)
        {
            MessageBoxW(messageWindow, Tr(Str::Err_NetworkDrives), Tr(Str::Title_Error), MB_OK | MB_ICONWARNING);
            return;
        }
    }

    // Extract folder name for title
    size_t lastSlash = folderPath.find_last_of(L"\\/");
    std::wstring folderName = (lastSlash != std::wstring::npos) ? folderPath.substr(lastSlash + 1) : folderPath;

    // Convert wide string to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), (int)folderPath.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8Path(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), (int)folderPath.size(), &utf8Path[0], size, nullptr, nullptr);

    size = WideCharToMultiByte(CP_UTF8, 0, folderName.c_str(), (int)folderName.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8Name(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, folderName.c_str(), (int)folderName.size(), &utf8Name[0], size, nullptr, nullptr);

    // Create config
    CorralWindowConfig newConfig = MakeDefaultCorralConfig();

    CorralTabConfig tab = MakeDefaultTabConfig(utf8Name);
    tab.IsVirtual = true;
    tab.VirtualFolderPath = utf8Path;
    tab.IsCatchAll = false; // Virtual corrals cannot be catch-all
    newConfig.Tabs.push_back(tab);

    SIZE size = DefaultCorralSize(desiredCenter);
    POINT topLeft = {desiredCenter.x - size.cx / 2, desiredCenter.y - size.cy / 2};
    POINT center = FindNearestFreeCorralPosition(topLeft, size.cx, size.cy);
    newConfig.Left = (double)(center.x - size.cx / 2);
    newConfig.Top = (double)(center.y - size.cy / 2);
    newConfig.Width = size.cx;
    newConfig.Height = size.cy;

    auto corral = std::make_unique<CorralWindow>(newConfig);
    corral->Show();
    corrals.push_back(std::move(corral));
    SaveConfig();
}

bool App::IsDesktopUnderMouse(POINT pt)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd)
        return false;

    wchar_t className[256];
    while (hwnd)
    {
        GetClassNameW(hwnd, className, 256);
        std::wstring classStr(className);

        if (classStr == L"Progman" || classStr == L"WorkerW")
        {
            return true;
        }

        HWND shellWindow = GetShellWindow();
        if (hwnd == shellWindow)
        {
            return true;
        }

        hwnd = GetParent(hwnd);
    }

    return false;
}

LRESULT CALLBACK App::MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    App *app = nullptr;

    if (uMsg == WM_CREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        app = (App *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else
    {
        app = (App *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (app && uMsg == TrayIcon::WM_TRAYICON)
    {
        if (lParam == WM_RBUTTONUP)
        {
            app->ShowTrayMenu();
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK)
        {
            app->ToggleDesktopIcons();
            return 0;
        }
        // User clicked the "update available" balloon — open the release page.
        if (lParam == NIN_BALLOONUSERCLICK)
        {
            if (!app->pendingUpdateUrl.empty())
            {
                ShellExecuteW(nullptr, L"open", app->pendingUpdateUrl.c_str(),
                              nullptr, nullptr, SW_SHOWNORMAL);
                app->pendingUpdateUrl.clear();
            }
            return 0;
        }
    }

    if (app && uMsg == WM_DISPLAYCHANGE)
    {
        app->OnDisplayChange();
        return 0;
    }

    // Double-click on the desktop (posted by the mouse hook) — quick-hide if
    // it landed on an empty spot. While quick-hide is active the icon check is
    // skipped: the ListView is hidden, so every desktop spot counts as empty.
    if (app && uMsg == WM_QUICKHIDE_DBLCLK)
    {
        POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
        if (app->IsPointOnEmptyDesktop(pt, !app->quickHideActive))
        {
            app->ToggleQuickHide();
        }
        return 0;
    }

    // Async update check finished — take ownership of the result and notify
    if (app && uMsg == WM_UPDATE_CHECK_DONE)
    {
        std::unique_ptr<UpdateCheckResult> result((UpdateCheckResult *)lParam);
        if (result && app->trayIcon)
        {
            if (result->Success && result->UpdateAvailable)
            {
                app->pendingUpdateUrl = result->ReleaseUrl;
                app->trayIcon->ShowBalloon(
                    Tr(Str::Update_AvailableTitle),
                    TrFmt(Str::Update_AvailableBody, result->LatestVersion));
            }
            else if (result->UserInitiated)
            {
                // Only surface the no-update / failure outcome for manual checks.
                if (result->Success)
                    app->trayIcon->ShowBalloon(
                        Tr(Str::Update_UpToDateTitle),
                        TrFmt(Str::Update_UpToDateBody, DEXCORRAL_VERSION));
                else
                    app->trayIcon->ShowBalloon(
                        Tr(Str::Update_FailedTitle),
                        Tr(Str::Update_FailedBody));
            }
        }
        return 0;
    }

    // Desktop file renamed/deleted — posted from DesktopMonitor's background watcher
    // threads (see WM_DESKTOP_FILE_RENAMED comment above). Always reclaim the
    // heap-allocated payload even if app is somehow null, to avoid leaking it.
    if (uMsg == WM_DESKTOP_FILE_RENAMED)
    {
        std::unique_ptr<std::pair<std::wstring, std::wstring>> names(
            reinterpret_cast<std::pair<std::wstring, std::wstring> *>(lParam));
        if (app && names)
        {
            app->OnDesktopFileRenamed(names->first, names->second);
        }
        return 0;
    }

    if (uMsg == WM_DESKTOP_FILE_DELETED)
    {
        std::unique_ptr<std::wstring> name(reinterpret_cast<std::wstring *>(lParam));
        if (app && name)
        {
            app->OnDesktopFileDeleted(*name);
        }
        return 0;
    }

    // "New DexCorral" / "New Virtual DexCorral" picked from Explorer's own desktop
    // context menu — posted from DexCorralShellExt::InvokeCommand, which runs on
    // Explorer's UI thread (see WM_CREATE_CORRAL_AT comment above). The payload is
    // the invocation point in screen coordinates; the placement search runs here,
    // on the thread that owns `corrals`.
    if (uMsg == WM_CREATE_CORRAL_AT || uMsg == WM_CREATE_VIRTUAL_CORRAL_AT)
    {
        if (app)
        {
            POINT pt = {(int)(LONG)wParam, (int)(LONG)lParam};
            if (uMsg == WM_CREATE_CORRAL_AT)
                app->CreateCorralAt(pt);
            else
                app->CreateVirtualCorralAt(pt);
        }
        return 0;
    }

    // Deferred z-order repin requested by WinEventProc, running here on the app
    // thread's normal message loop where re-entering USER32 is safe.
    if (app && uMsg == WM_REPIN_CORRALS)
    {
        InterlockedExchange(&s_RepinPending, 0);
        // Skip while a corral drag owns the capture — this thread holds capture only
        // for the duration of a corral interaction, and reshuffling the z-order under
        // an in-flight drag buys nothing. The drag's own paths re-pin when it ends.
        if (!GetCapture())
            app->RepinBand();
        return 0;
    }

    // Hook retry thread succeeded — refresh hidden icon data now that the hook is active
    if (app && uMsg == (WM_APP + 100))
    {
        app->UpdateHookHiddenIcons();
        app->PositionHiddenIconsUnderCorrals();
        HookBridge::RefreshDesktop();
        return 0;
    }

    // Shell image list changed (e.g. Recycle Bin emptied/filled)
    if (app && uMsg == (WM_APP + 101))
    {
        // Free the PIDL list delivered with SHCNRF_NewDelivery
        LONG lEvent = 0;
        PIDLIST_ABSOLUTE *pidls = nullptr;
        HANDLE hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &pidls, &lEvent);
        if (hLock)
        {
            SHChangeNotification_Unlock(hLock);
        }
        app->RefreshAllCorrals();
        return 0;
    }

    // Deferred catch-all adoption — waits out Explorer's inline rename
    if (app && uMsg == WM_TIMER && wParam == ADOPTION_TIMER)
    {
        app->ProcessPendingAdoptions();
        return 0;
    }

    // Retry timer — fired when Shell_NotifyIconW(NIM_ADD) failed at startup
    if (uMsg == WM_TIMER && wParam == TRAY_RETRY_TIMER)
    {
        if (app && app->trayIcon && app->trayIcon->Show()) {
            KillTimer(hwnd, TRAY_RETRY_TIMER);
            DllLog(L"App: TRAY_RETRY_TIMER — tray icon added successfully");
            ShowSafeModeNoticeIfPending(app->trayIcon.get());
        }
        return 0;
    }

    // Explorer restarted — re-add the tray icon
    if (app && app->wmTaskbarCreated && uMsg == app->wmTaskbarCreated)
    {
        DllLog(L"App: WM_TASKBARCREATED — Explorer restarted, retrying tray icon");
        if (app->trayIcon)
        {
            app->trayIcon->Show();
            ShowSafeModeNoticeIfPending(app->trayIcon.get());
        }
        return 0;
    }

    // Handle Windows shutdown/logoff
    if (uMsg == WM_QUERYENDSESSION)
    {
        return TRUE;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int App::Run()
{
    Initialize();

    DllLog(L"App::Run: entering message loop");

    // If we have a stop event (shell extension mode), watch it alongside messages
    if (g_AppStopEvent)
    {
        MSG msg;
        for (;;)
        {
            DWORD result = MsgWaitForMultipleObjects(1, &g_AppStopEvent, FALSE, INFINITE, QS_ALLINPUT);
            if (result == WAIT_OBJECT_0)
            {
                // Stop event signaled — DLL is being unloaded
                break;
            }
            // Process all pending messages
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                    goto done;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    done:
        return 0;
    }

    // Standalone mode (no stop event) — classic message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

void App::EnsureCatchAllCorral()
{
    // Find if any corral is marked as catch-all
    bool foundCatchAll = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            // Virtual corrals cannot be catch-all
            if (tab.IsVirtual)
            {
                tab.IsCatchAll = false;
                continue;
            }

            if (tab.IsCatchAll)
            {
                if (foundCatchAll)
                {
                    // Only one catch-all allowed
                    tab.IsCatchAll = false;
                }
                foundCatchAll = true;
            }
        }
    }

    // Catch-all is optional: if none exists, leave it disabled. New desktop
    // files simply won't be auto-collected until the user enables a catch-all.
}

CorralWindow *App::GetCatchAllCorral()
{
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            if (tab.IsCatchAll && !tab.IsVirtual)
            {
                return corral.get();
            }
        }
    }
    // Fallback to first non-virtual corral if no catch-all defined
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            if (!tab.IsVirtual)
            {
                return corral.get();
            }
        }
    }
    return nullptr;
}

void App::OnDisplayChange()
{
    if (!monitorManager)
        return;

    // Refresh monitor list
    monitorManager->Refresh();

    // Reposition all corrals
    UpdateCorralPositions();

    // Refresh all corral backgrounds
    RefreshAllCorralBackgrounds();
}

void App::UpdateCorralPositions()
{
    if (!monitorManager)
        return;

    const MonitorInfo *primaryMon = monitorManager->GetPrimaryMonitor();
    if (!primaryMon)
        return;

    bool configChanged = false;

    for (auto &corral : corrals)
    {
        CorralWindowConfig &cfg = corral->GetConfig();
        const std::string &targetId = cfg.TargetMonitorId;

        // Check if target monitor is active
        const MonitorInfo *targetMon = monitorManager->FindMonitor(targetId);

        if (targetMon)
        {
            // Target monitor is active - restore/scale position
            auto it = cfg.MonitorPositions.find(targetId);
            if (it != cfg.MonitorPositions.end())
            {
                MonitorPosition &stored = it->second;

                // Check if resolution changed
                if (stored.RefWidth != targetMon->width || stored.RefHeight != targetMon->height)
                {
                    // Scale position and size based on percentage (stored positions are relative to monitor)
                    double xPercent = (double)stored.Left / stored.RefWidth;
                    double yPercent = (double)stored.Top / stored.RefHeight;
                    double wPercent = (double)stored.Width / stored.RefWidth;
                    double hPercent = (double)stored.Height / stored.RefHeight;

                    // Calculate new relative position on monitor
                    int newRelLeft = (int)(xPercent * targetMon->width);
                    int newRelTop = (int)(yPercent * targetMon->height);
                    int newWidth = (int)(wPercent * targetMon->width);
                    int newHeight = (int)(hPercent * targetMon->height);

                    // Clamp relative position to monitor bounds
                    if (newRelLeft + newWidth > targetMon->width)
                    {
                        newRelLeft = targetMon->width - newWidth;
                    }
                    if (newRelTop + newHeight > targetMon->height)
                    {
                        newRelTop = targetMon->height - newHeight;
                    }
                    if (newRelLeft < 0)
                        newRelLeft = 0;
                    if (newRelTop < 0)
                        newRelTop = 0;

                    // Convert to screen coordinates
                    int screenLeft = targetMon->bounds.left + newRelLeft;
                    int screenTop = targetMon->bounds.top + newRelTop;

                    // Update stored position with new relative values
                    stored.Left = newRelLeft;
                    stored.Top = newRelTop;
                    stored.Width = newWidth;
                    stored.Height = newHeight;
                    stored.RefWidth = targetMon->width;
                    stored.RefHeight = targetMon->height;

                    // Apply to corral (using screen coordinates)
                    cfg.Left = screenLeft;
                    cfg.Top = screenTop;
                    cfg.Width = newWidth;
                    cfg.Height = newHeight;

                    SetWindowPos(corral->GetHWND(), nullptr,
                                 screenLeft, screenTop, newWidth, newHeight,
                                 SWP_NOZORDER | SWP_NOACTIVATE);

                    configChanged = true;
                }
            }
        }
        else if (!targetId.empty())
        {
            // Target monitor is offline - move to primary monitor
            // Calculate position based on percentage from stored position
            auto it = cfg.MonitorPositions.find(targetId);
            if (it != cfg.MonitorPositions.end())
            {
                MonitorPosition &stored = it->second;

                // Calculate percentages from original relative position
                double xPercent = (double)stored.Left / stored.RefWidth;
                double yPercent = (double)stored.Top / stored.RefHeight;
                double wPercent = (double)stored.Width / stored.RefWidth;
                double hPercent = (double)stored.Height / stored.RefHeight;

                // Apply to primary monitor (calculate relative position first)
                int newRelLeft = (int)(xPercent * primaryMon->width);
                int newRelTop = (int)(yPercent * primaryMon->height);
                int newWidth = (int)(wPercent * primaryMon->width);
                int newHeight = (int)(hPercent * primaryMon->height);

                // Clamp relative position to primary monitor bounds
                if (newRelLeft + newWidth > primaryMon->width)
                {
                    newRelLeft = primaryMon->width - newWidth;
                }
                if (newRelTop + newHeight > primaryMon->height)
                {
                    newRelTop = primaryMon->height - newHeight;
                }
                if (newRelLeft < 0)
                    newRelLeft = 0;
                if (newRelTop < 0)
                    newRelTop = 0;

                // Convert to screen coordinates
                int screenLeft = primaryMon->bounds.left + newRelLeft;
                int screenTop = primaryMon->bounds.top + newRelTop;

                // Update current position (but keep TargetMonitorId unchanged so it returns when monitor reconnects)
                cfg.Left = screenLeft;
                cfg.Top = screenTop;
                cfg.Width = newWidth;
                cfg.Height = newHeight;

                SetWindowPos(corral->GetHWND(), nullptr,
                             screenLeft, screenTop, newWidth, newHeight,
                             SWP_NOZORDER | SWP_NOACTIVATE);

                configChanged = true;
            }
        }
    }

    if (configChanged)
    {
        SaveConfig();
    }
}

void App::OnDesktopFileAdded(const std::wstring &fileName)
{
    if (fileName.empty())
        return;

    // Don't adopt immediately: a file created via the desktop "New" context
    // menu goes straight into Explorer's inline rename edit box, and yanking
    // the icon into the catch-all corral would kill the rename. Queue it and
    // let the (app-thread) adoption timer pick it up once no rename is active.
    {
        std::lock_guard<std::mutex> lock(pendingAdoptionsLock);
        pendingAdoptions.push_back(fileName);
    }
    // SetTimer from the monitor thread is fine: the timer fires on the thread
    // that owns messageWindow (the app thread)
    SetTimer(messageWindow, ADOPTION_TIMER, ADOPTION_POLL_MS, nullptr);
}

void App::ProcessPendingAdoptions()
{
    // Still renaming? Keep the periodic timer armed and try again next tick.
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView && SendMessageW(hListView, LVM_GETEDITCONTROL, 0, 0) != 0)
    {
        return;
    }
    KillTimer(messageWindow, ADOPTION_TIMER);

    std::vector<std::wstring> adoptions;
    {
        std::lock_guard<std::mutex> lock(pendingAdoptionsLock);
        adoptions.swap(pendingAdoptions);
    }

    bool changed = false;
    for (const auto &fileName : adoptions)
    {
        if (fileName.empty())
            continue;

        // Convert to UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string fileNameStr(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

        // Skip if the file vanished while pending (deleted, moved away)
        std::wstring fullPath = ResolveDesktopFilePath(fileName);
        DWORD attrs = GetFileAttributesW(fullPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }
        // Files Explorer doesn't show on the desktop (desktop.ini and other
        // hidden/system files) must never be adopted into a corral
        if (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
        {
            continue;
        }

        // Check if file is already in any corral
        bool tracked = false;
        for (auto &corral : corrals)
        {
            for (auto &tab : corral->GetConfig().Tabs)
            {
                auto &files = tab.Files;
                if (std::find(files.begin(), files.end(), fileNameStr) != files.end())
                {
                    tracked = true;
                    break;
                }
            }
            if (tracked)
                break;
        }
        if (tracked)
            continue;

        // Add to catch-all corral
        CorralWindow *catchAll = GetCatchAllCorral();
        if (catchAll)
        {
            catchAll->AddFile(fileNameStr);
            changed = true;
        }
    }

    if (changed)
    {
        SaveConfig();
        // Hide the adopted icons on the desktop and park them under the corral
        UpdateHookHiddenIcons();
        PositionHiddenIconsUnderCorrals();
        HookBridge::RefreshDesktop();
    }
}

void App::OnDesktopFileRenamed(const std::wstring &oldName, const std::wstring &newName)
{
    // Convert to UTF-8
    int oldSize = WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string oldNameStr(oldSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, &oldNameStr[0], oldSize, nullptr, nullptr);

    int newSize = WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string newNameStr(newSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, &newNameStr[0], newSize, nullptr, nullptr);

    // Follow renames in the pending adoption queue (e.g. user typing the name
    // of a freshly created "New Text Document")
    {
        std::lock_guard<std::mutex> lock(pendingAdoptionsLock);
        for (auto &pending : pendingAdoptions)
        {
            if (_wcsicmp(pending.c_str(), oldName.c_str()) == 0)
            {
                pending = newName;
            }
        }
    }

    // Update in all corrals
    bool changed = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            auto &files = tab.Files;
            auto it = std::find(files.begin(), files.end(), oldNameStr);
            if (it != files.end())
            {
                *it = newNameStr;
                corral->LoadFiles();
                changed = true;
            }
        }
    }

    if (changed)
    {
        SaveConfig();

        // Keep the OLD identity hidden as a transition alias: the shell
        // updates the desktop item asynchronously, and until it does, the
        // item still matches the pre-rename identity. Derive the old path
        // from the renamed file's directory (the old file no longer exists,
        // so it can't be resolved directly).
        std::wstring newPath = ResolveDesktopFilePath(newName);
        size_t slash = newPath.find_last_of(L'\\');
        std::wstring oldPath = (slash != std::wstring::npos)
                                   ? newPath.substr(0, slash + 1) + oldName
                                   : oldName;
        AddTransientHiddenIcon(DesktopIcons::GetShellDisplayName(oldPath), oldPath);

        // Refresh the hook's hidden list (now containing both identities)
        // and re-park, otherwise the renamed icon reappears on the desktop
        UpdateHookHiddenIcons();
        PositionHiddenIconsUnderCorrals();
    }
}

void App::OnDesktopFileDeleted(const std::wstring &fileName)
{
    // Drop the file from the pending adoption queue if it never got adopted
    {
        std::lock_guard<std::mutex> lock(pendingAdoptionsLock);
        pendingAdoptions.erase(
            std::remove_if(pendingAdoptions.begin(), pendingAdoptions.end(),
                           [&fileName](const std::wstring &pending)
                           { return _wcsicmp(pending.c_str(), fileName.c_str()) == 0; }),
            pendingAdoptions.end());
    }

    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    // Remove from all corrals
    bool changed = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            auto &files = tab.Files;
            auto it = std::find(files.begin(), files.end(), fileNameStr);
            if (it != files.end())
            {
                files.erase(it);
                corral->LoadFiles();
                changed = true;
            }
        }
    }

    if (changed)
    {
        SaveConfig();
    }
}
