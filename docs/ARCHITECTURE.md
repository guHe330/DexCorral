# DexCorral Architecture

A Windows desktop icon organizer that creates virtual corral windows to organize and hide desktop icons using layered windows, per-pixel alpha rendering, and Explorer process injection.

## Table of Contents

- [System Overview](#system-overview)
- [How It Works](#how-it-works)
- [Component Architecture](#component-architecture)
- [Explorer Hook Integration](#explorer-hook-integration)
- [Win32 API Reference](#win32-api-reference)

---

## System Overview

DexCorral is a desktop icon management tool that:

1. **Creates layered corral windows** with per-pixel alpha transparency on the desktop
2. **Injects a ShellExtension DLL into Explorer.exe** to hide corral-owned icons from the desktop ListView
3. **Manages desktop icons** across multiple corrals using cross-process memory manipulation
4. **Provides rich UI** with tabs, drag-drop, context menus, and resizable windows
5. **Persists configuration** in JSON format with automatic icon snapping and positioning

The application consists of one executable and one DLL:

| Component | Purpose | Type |
|-----------|---------|------|
| `DexCorral.exe` | Registration tool (registers/unregisters the shell extension) | Win32 GUI Application |
| `DexCorralHook.dll` | Monolith shell extension: Explorer hook + all app logic (corral windows, tray icon, config, etc.) | In-Process COM DLL |

---

## How It Works

### High-Level Flow

```
User runs DexCorral.exe --register (one-time setup)
         ↓
Shell extension (DexCorralHook.dll) registered as icon overlay handler
         ↓
Explorer restart loads DexCorralHook.dll via COM (DLL_PROCESS_ATTACH)
         ↓
Hook subclasses desktop ListView (SHELLDLL_DefView + SysListView32)
         ↓
Worker thread starts App message loop (corral windows, tray icon, config)
         ↓
User drags icons → Corral receives drop → Adds to active tab
         ↓
App updates hidden icon list (in-process) → Hook reads and hides icons
         ↓
Desktop icons are visually suppressed in Explorer (pure draw suppression)
         ↓
Corrals render with per-pixel alpha transparency on the desktop
```

### User Interaction Flow

1. **Create Corral**: Right-click desktop → "New DexCorral" (context menu via shell extension) → Window appears
2. **Add Icons**: Drag desktop icon → Drop into corral → Icon moves to tab
3. **Hide Icons**: App updates hidden icon list → Hook suppresses draw on next paint
4. **Customize**: Right-click corral → Settings → Configure appearance/tabs
5. **Save**: Config auto-saves to `%APPDATA%/DexCorral/config.json` on changes

---

## Component Architecture

### 1. Application Controller (`App.cpp`)

Runs on a worker thread inside `DexCorralHook.dll` (within Explorer.exe). Manages:

- **Window Management**: Creates, destroys, and positions corral windows
- **Configuration**: Loads/saves JSON config from `%APPDATA%/DexCorral/`
- **Event Loop**: Win32 message loop with custom message window for IPC
- **Integration**: Coordinates tray icon, desktop monitoring

**Key Responsibilities:**
- Update hidden icon list (via HookBridge) when files are added/removed; triggers hook compaction
- Restore icon positions when corrals are moved or closed
- Handle display configuration changes (multi-monitor)
- Manage desktop file monitoring for catch-all corral

**Configuration Structure** (`%APPDATA%/DexCorral/config.json`):
```json
{
  "Corrals": [
    {
      "Left": 100.0,
      "Top": 100.0,
      "Width": 400.0,
      "Height": 300.0,
      "IsRolledUp": false,
      "ActiveTabIndex": 0,
      "TargetMonitorId": "DELA0EC_28_07E1_D2",
      "MonitorPositions": {
        "DELA0EC_28_07E1_D2": {
          "Left": 100, "Top": 100, "Width": 400, "Height": 300,
          "RefWidth": 2560, "RefHeight": 1440
        }
      },
      "TitleBarHeight": 32,
      "HeaderFontName": "Segoe UI",
      "HeaderFontSize": 13,
      "HeaderFontColor": "#FFFFFF",
      "IconOpacity": 255,
      "IconTintColor": "#000000",
      "IconTintStrength": 0,
      "IconSpacingXPercent": 100,
      "IconSpacingYPercent": 100,
      "Tabs": [
        {
          "Title": "Documents",
          "ColorHex": "#99000000",
          "Files": ["file1.txt", "file2.pdf", "shell:{20D04FE0-3AEA-1069-A2D8-08002B30309D}"],
          "ViewModeInt": 0,
          "IsCatchAll": false,
          "IsVirtual": false,
          "VirtualFolderPath": ""
        },
        {
          "Title": "Downloads",
          "ColorHex": "#993366FF",
          "Files": [],
          "ViewModeInt": 1,
          "IsCatchAll": false,
          "IsVirtual": true,
          "VirtualFolderPath": "C:\\Users\\me\\Downloads"
        }
      ]
    }
  ],
  "DesktopIconsVisible": true,
  "DefaultColorHex": "#99000000",
  "HideShortcutArrows": false,
  "DefaultTitleBarHeight": 32,
  "DefaultHeaderFontName": "Segoe UI",
  "DefaultHeaderFontSize": 13,
  "DefaultHeaderFontColor": "#FFFFFF",
  "DefaultIconOpacity": 255,
  "DefaultIconTintColor": "#000000",
  "DefaultIconTintStrength": 0,
  "DefaultIconSpacingXPercent": 100,
  "DefaultIconSpacingYPercent": 100
}
```

### 2. Corral Windows (`CorralWindow.cpp`)

**Implementation split across multiple files:**
- `CorralWindow.cpp` - Core lifecycle, layout, tab management
- `CorralWindowRender.cpp` - Per-pixel alpha rendering
- `CorralWindowIcons.cpp` - Icon loading, drawing, grid layout, file operations
- `CorralWindowInput.cpp` - Mouse/keyboard handling, drag-drop, resize
- `CorralWindowDialogs.cpp` - In-memory dialog templates for settings
- `CorralWindowCommands.cpp` - Context menu actions, file operations

**Features:**
- **Layered Windows** (`WS_EX_LAYERED`): Per-pixel alpha transparency
- **32-bit DIB Rendering**: Custom alpha compositing
- **Icon Grid Layout**: Configurable icon spacing, view modes (small/medium/large/details)
- **Tab System**: Multiple folders per corral with quick switching
- **Drag-Drop**: Supports files, shortcuts, shell items via `IDropTarget`
- **Drop-on-Icon**: Dropping a file onto an icon inside a corral forwards the drop to that icon's shell `IDropTarget` (e.g., drop onto an exe to open with it)
- **Icon Hover Effects**: Visual highlight when mousing over icons (alpha-blended overlay, grid and details views)
- **Resize and Snap**: Snap to grid, edges, other corrals
- **Roll-Up/Hover-Expand**: Compact title-bar-only view that expands on hover
- **Scrollbar**: PowerShell-style scrollbar with custom rendering
- **Icon Rename**: Double-click label to rename files
- **Context Menu**: Right-click for delete, properties, view mode selection

**Rendering Pipeline:**
1. Create 32-bit DIB section
2. Fill with background color (pre-multiplied alpha)
3. Draw title bar and tabs
4. Draw icons with labels
5. Draw selection/hover highlights (alpha-blended compositing over existing pixels)
6. Alpha fix-up: GDI draws with alpha=0, so loop through pixels and restore alpha for non-transparent pixels
7. Update layered window via `UpdateLayeredWindow` with final DIB

### 3. Explorer Shell Extension (`CorralHook.cpp`)

Loaded into `explorer.exe` via COM shell extension registration to hide corral-owned icons and manage desktop layout.

**Subclassing Strategy:**
- Subclasses `SHELLDLL_DefView` for `WM_NOTIFY` custom draw messages and context menu interception
- Subclasses `SysListView32` for input filtering, item text retrieval, and auto-arrange management

**Hide Mechanism:**
- Pure draw suppression: Returns `CDRF_SKIPDEFAULT` from `NM_CUSTOMDRAW` → Explorer doesn't draw the item
- Hidden icon is positioned at valid screen coordinates (under corral center) for drag-drop occlusion
- No deletion or virtual visibility; item still exists in ListView but is invisible

**Input Filtering:**
- Intercepts `LVM_HITTEST` to make hidden icons invisible to hit testing
- Swallows mouse clicks that would select hidden icons
- After `WM_LBUTTONUP`, deselects hidden icons that entered rubber-band selection
- Intercepts arrow keys to skip hidden icons when navigating

**Auto-Arrange Management:**
- Takes over Explorer's auto-arrange: strips `LVS_AUTOARRANGE` style from the ListView on initialization
- Intercepts `WM_STYLECHANGING` to block Explorer from re-enabling auto-arrange
- Intercepts `WM_COMMAND` for sort commands (Sort by Name/Date/Size/Type) and align-to-grid
- `CompactVisibleIcons()`: After hiding icons, removes gaps in the icon grid by repositioning visible icons contiguously
- Debounced compaction via timers to avoid redundant repositioning during bulk operations
- Intercepts `WM_INITMENUPOPUP` to fake auto-arrange checkmark state in Explorer's context menu
- Shows a dismissable popup ("Disable auto-sort?") when user manually moves icons while our auto-arrange is active

**Drop Target Wrapping:**
- Wraps Explorer's `IDropTarget` on the desktop ListView
- Suppresses `DragEnter`/`DragOver` on hidden icons to prevent visual artifacts (highlights, tooltips)
- Properly manages `DragEnter`/`DragLeave` transitions when dragging between hidden and visible icons

**In-Process Communication (via HookBridge):**
- App worker thread writes hidden icon names via `HookBridge::UpdateHiddenIcons()`
- Hook reads via `HookBridge::GetVersion()` / `HookBridge::GetHiddenIconNames()`
- Version counter changes on every update → Hook only rebuilds list when needed (zero-cost cache checks)
- Protected by CRITICAL_SECTION; most paint cycles just read a volatile DWORD (no lock)

**Logging:**
- Always-on logging to `%APPDATA%/DexCorral/CorralHook.log` at this stage of development

### 4. Desktop Icon Manipulation (`DesktopIcons.cpp`)

Cross-process access to Explorer's desktop ListView.

**Strategy:** Cannot directly access Explorer's memory, so:
1. Find desktop ListView handle via window hierarchy traversal
2. Use `VirtualAllocEx` to allocate memory in Explorer process
3. Use `WriteProcessMemory`/`ReadProcessMemory` for cross-process data transfer
4. Use `SendMessage` with remote pointers to execute ListView commands

**Operations:**
- **Get Icon Count**: `LVM_GETITEMCOUNT`
- **Get Item Text**: `LVM_GETITEMTEXTW` with remote buffer
- **Get Item Position**: `LVM_GETITEMPOSITION` (reading)
- **Set Item Position**: `LVM_SETITEMPOSITION` (hiding/restoring)
- **Hit Test**: `LVM_HITTEST` to check if point is on icon
- **Get Selected Count**: `LVM_GETSELECTEDCOUNT`

**Window Hierarchy:**
```
Progman (or WorkerW)
  ↓
SHELLDLL_DefView
  ↓
SysListView32 (desktop icon ListView)
```

### 5. Global Mouse Hook (`MouseHook.cpp`)

Low-level mouse event monitoring for corral interactions.

**Hook Type:** `WH_MOUSE_LL` (system-wide, low-level)

**Features:**
- Captures mouse button down/up events globally
- Captures mouse move for drag operations
- **Mouse Wheel Routing**: Routes `WM_MOUSEWHEEL` from global hook to window under cursor (fixes issue with tool windows not receiving wheel events naturally)
- Callback-based design for decoupling

**Mouse Wheel Workaround:**
Tool windows (`WS_EX_TOOLWINDOW`) don't naturally receive mouse wheel events. The hook intercepts `WM_MOUSEWHEEL` at the system level, identifies the window under the cursor, and routes the message directly via `SendMessage`.

### 6. Tray Icon (`TrayIcon.cpp`)

System tray presence and quick access menu.

**Features:**
- Displays icon in system notification area
- Left-click: Show/focus main window
- Right-click: Context menu (create corral, settings)
- Tooltip with hover information

### 7. Folder Watcher (`FolderWatcher.cpp`)

Monitors virtual corral folder for changes.

**Features:**
- Uses `FindFirstChangeNotification` / `WaitForMultipleObjects` for async change detection
- Posts `WM_FOLDER_CHANGED` custom message to corral window when folder contents change
- Auto-loads new files, removes deleted files
- Supports auto-monitoring of virtual folder paths

### 8. Desktop Monitor (`DesktopMonitor.cpp`)

Watches physical desktop for new files.

**Strategy:**
- Monitors desktop path (`%USERPROFILE%\Desktop`) for file creation/deletion
- Provides callbacks for application to handle desktop file changes
- Supports "catch-all" corral that auto-collects new desktop files

### 9. Monitor Manager (`MonitorManager.cpp`)

Tracks display configuration.

**Features:**
- Listens for `WM_SETTINGCHANGE` for monitor addition/removal
- Tracks DPI scaling per monitor
- Notifies application of display changes for corral repositioning

---

## Explorer Hook Integration

### Loading Strategy

DexCorralHook.dll is loaded into Explorer.exe automatically via COM shell extension registration (not manual injection):

```
DexCorral.exe --register (one-time setup, requires admin)
         ↓
1. Calls DllRegisterServer() in DexCorralHook.dll
2. Registers as IShellIconOverlayIdentifier (Explorer loads all overlay handlers on startup)
3. Also registers as desktop background context menu extension (IContextMenu)
         ↓
Explorer restart (or next login)
         ↓
Explorer loads DexCorralHook.dll via CoCreateInstance
         ↓
DLL_PROCESS_ATTACH initializes hook + starts app worker thread
```

### Hook Activation

```c
1. DLL_PROCESS_ATTACH (loaded by Explorer via COM)
   ↓
2. HookBridge::Initialize() — creates CRITICAL_SECTION for thread-safe icon list
   ↓
3. InitializeCorralHook(): Find desktop ListView, subclass SHELLDLL_DefView + SysListView32,
   capture and disable Explorer's auto-arrange, wrap Explorer's IDropTarget, register popup class
   ↓
4. Start worker thread → RunApp() (corral windows, tray icon, config)
   ↓
5. On each custom draw message: check version counter, hide marked icons, compact visible icons
```

### In-Process Communication

Since both the app worker thread and Explorer hook run inside the same process (`explorer.exe`), they share data via `HookBridge` — a global `std::vector<std::wstring>` protected by a `CRITICAL_SECTION`:

- **Initialization**: `HookBridge::Initialize()` creates the critical section (called from `DLL_PROCESS_ATTACH`); `HookBridge::Cleanup()` destroys it (called from `DLL_PROCESS_DETACH`)
- **Writer** (app worker thread): `HookBridge::UpdateHiddenIcons()` — locks, copies names into global vector, increments version counter
- **Reader** (Explorer UI thread): `HookBridge::GetVersion()` — reads volatile DWORD (no lock). Only calls `GetHiddenIconNames()` (which takes the lock) when version changes.
- Most paint cycles have zero synchronization cost (version matches → cached list is valid)

---

## Win32 API Reference

### Window Management

| API | Purpose |
|-----|---------|
| `CreateWindowExW` | Create layered corral window with `WS_EX_LAYERED` and `WS_EX_TOOLWINDOW` |
| `DestroyWindow` | Destroy corral window on close |
| `RegisterClassExW` | Register custom window class for corrals |
| `FindWindowW` | Find Progman window (desktop parent) |
| `FindWindowExW` | Find SHELLDLL_DefView and SysListView32 children |
| `EnumWindows` | Enumerate all windows to find desktop ListView in WorkerW |
| `ShowWindow` | Show/hide corral windows |
| `MoveWindow` | Reposition window on drag/snap |
| `GetWindowRect` | Get corral window bounds |
| `SetWindowPos` | Set window position with Z-order (e.g., `SendToBottom`) |
| `GetWindowThreadProcessId` | Get process ID of desktop ListView |
| `WindowFromPoint` | Find window under mouse cursor (for mouse wheel routing) |
| `GetModuleHandleW` | Get application instance handle |

### Message Processing

| API | Purpose |
|-----|---------|
| `SendMessageW` | Send messages to windows (LVM_* for ListView) |
| `PostMessageW` | Post asynchronous messages | 
| `DefWindowProcW` | Default window procedure |
| `CallWindowProcW` | Call original window procedure in subclass |
| `DispatchMessageW` | Dispatch message from queue to window proc |
| `GetMessageW` | Retrieve next message from queue |
| `TranslateMessage` | Translate virtual key codes to WM_CHAR |
| `CreateMessageQueueW` | Create per-thread message queue |

### Rendering and Graphics

| API | Purpose |
|-----|---------|
| `GetDC` | Get device context for screen/window |
| `ReleaseDC` | Release device context |
| `CreateCompatibleDC` | Create memory device context for drawing |
| `DeleteDC` | Delete memory device context |
| `CreateDIBSection` | Create 32-bit DIB section for per-pixel alpha |
| `SelectObject` | Select bitmap/brush/pen into device context |
| `UpdateLayeredWindow` | Update layered window with final DIB (alpha blending) |
| `CreateSolidBrush` | Create solid color brush |
| `GetStockObject` | Get standard system objects (pens, brushes) |
| `PatBlt` | Fast fill rectangle with pattern |
| `TextOutW` | Draw text (title bar, labels, tab names) |
| `SetTextColor` | Set text color for drawing |
| `SetBkMode` | Set background mode (TRANSPARENT, OPAQUE) |
| `DrawEdge` | Draw beveled edge for 3D effect |

### Icon and File Operations

| API | Purpose |
|-----|---------|
| `SHGetFileInfoW` | Get file icon, type, attributes from shell |
| `ExtractIconExW` | Extract icon from file (alternative to SHGetFileInfo) |
| `ShellExecuteW` | Execute/open file (double-click handler) |
| `SHFileOperationW` | Delete file with recycle bin or permanent delete |
| `GetFileAttributesW` | Check if file exists, is directory, etc. |
| `CreateDirectoryW` | Create folder for config storage |

### Drag and Drop

| API | Purpose |
|-----|---------|
| `RegisterDragDrop` | Register window as OLE drop target |
| `RevokeDragDrop` | Unregister OLE drop target |
| `OleInitialize` | Initialize OLE/COM (required for IDropTarget and drag-drop) |
| `OleUninitialize` | Uninitialize OLE/COM |
| `IDropTarget::DragEnter` | Called when drag enters window (accept/reject) |
| `IDropTarget::DragOver` | Called while dragging over window |
| `IDropTarget::Drop` | Called when drop occurs |
| `OleGetClipboard` | Get clipboard data (IDataObject) |

### Process and Memory

| API | Purpose |
|-----|---------|
| `OpenProcess` | Open explorer.exe for cross-process memory access |
| `CloseHandle` | Close process handle |
| `VirtualAllocEx` | Allocate memory in explorer.exe |
| `VirtualFreeEx` | Free remote memory |
| `ReadProcessMemory` | Read data from explorer.exe memory |
| `WriteProcessMemory` | Write data to explorer.exe memory |
| `GetCurrentProcessId` | Get DexCorral process ID |

### Synchronization and Events

| API | Purpose |
|-----|---------|
| `InitializeCriticalSection` | Create lock for hidden icon list |
| `EnterCriticalSection` / `LeaveCriticalSection` | Thread-safe access to hidden icon list |
| `CreateEventW` | Create named event (debug flag) |
| `OpenEventW` | Open named event for debug detection |
| `CloseHandle` | Close event handles |

### Registry and Configuration

| API | Purpose |
|-----|---------|
| `RegOpenKeyExW` | Open registry key (shortcut arrows) |
| `RegQueryValueExW` | Query registry value |
| `RegCloseKey` | Close registry key |
| `SHGetKnownFolderPath` | Get desktop, appdata, public desktop paths |

### Hooks and Callbacks

| API | Purpose |
|-----|---------|
| `SetWindowsHookExW` | Install global mouse hook (`WH_MOUSE_LL`) |
| `UnhookWindowsHookEx` | Remove global mouse hook |
| `CallNextHookEx` | Pass message to next hook in chain |
| `FindFirstChangeNotificationW` | Start monitoring folder for changes |
| `FindNextChangeNotificationW` | Wait for next folder change |
| `FindCloseChangeNotificationW` | Stop folder monitoring |
| `SetWindowsHookExW` (subclass variant) | DLL injection hooks into explorer.exe |
| `SetWindowSubclass` | Subclass window with protected subclass chain |

### Tray Icon and Notifications

| API | Purpose |
|-----|---------|
| `Shell_NotifyIconW` | Add/modify/delete tray icon |
| `MessageBoxW` | Show message dialog |
| `ShellExecuteW` | Open URL, folder, etc. |

### ListView Control Messages (Sent to Explorer)

| Message | Purpose |
|---------|---------|
| `LVM_GETITEMCOUNT` | Get number of desktop icons |
| `LVM_GETITEMTEXTW` | Get display name of icon at index |
| `LVM_GETITEMPOSITION` | Get pixel position of icon (for reading) |
| `LVM_SETITEMPOSITION` / `LVM_SETITEMPOSITION32` | Set pixel position of icon (for hiding/restoring/compaction) |
| `LVM_HITTEST` | Test if point hits an icon |
| `LVM_GETSELECTEDCOUNT` | Get number of selected icons |
| `LVM_GETITEM` / `LVM_SETITEM` | Get/set item data structures |
| `LVM_SORTITEMS` / `LVM_SORTITEMSEX` | Sort icons (intercepted for compaction) |
| `LVM_ARRANGE` | Arrange/align icons to grid (intercepted for compaction) |

### Custom Draw (NM_CUSTOMDRAW)

| Return Code | Purpose |
|------------|---------|
| `CDRF_DODEFAULT` | Draw normally |
| `CDRF_SKIPDEFAULT` | Skip default drawing (invisible) |
| `CDDS_PREPAINT` | Called before list item painting |
| `CDDS_ITEMPREPAINT` | Called before each item painting |
| `CDDS_ITEMPOSTPAINT` | Called after each item painting |

---

## Key Design Patterns

### 1. Per-Pixel Alpha Rendering

**Problem:** Standard window transparency uses window-level alpha. Per-pixel transparency with selective blending requires custom rendering.

**Solution:**
1. Create 32-bit DIB section (ARGB format)
2. Fill pixels with pre-multiplied colors
3. Draw with alpha=0 initially (GDI limitation)
4. Loop through pixels and restore alpha for non-transparent regions
5. Use `UpdateLayeredWindow` to composite final result with desktop

### 2. Cross-Process Memory Access

**Problem:** Cannot directly access Explorer's ListView items or memory.

**Solution:**
1. Allocate temporary buffer in Explorer via `VirtualAllocEx`
2. Use `SendMessage` with remote pointer to run ListView commands
3. Read results back via `ReadProcessMemory`
4. Free remote buffer

### 3. Zero-Cost Cache Invalidation

**Problem:** Hook DLL is called on every paint → checking for hidden icons on every draw could be expensive.

**Solution:**
1. HookBridge stores a volatile version counter alongside the hidden icon list
2. Hook caches last seen version locally
3. Only acquires lock and copies list when version changes
4. Most draws just compare two DWORDs (zero synchronization cost)

### 4. Invisible Icon Positioning

**Problem:** Hidden icons must remain invisible but positioned somewhere valid for drag-drop to work correctly.

**Solution:** Position hidden icons under the corral window center (valid screen coordinates but occluded by corral).

### 5. Tool Window Mouse Wheel Routing

**Problem:** Tool windows (`WS_EX_TOOLWINDOW`) don't naturally receive mouse wheel messages.

**Solution:** Global mouse hook intercepts `WM_MOUSEWHEEL`, finds window under cursor, and routes message directly via `SendMessage`.

### 6. Auto-Arrange Takeover

**Problem:** Explorer's auto-arrange repositions all icons including hidden ones, creating gaps and fighting with corral positioning.

**Solution:**
1. Strip `LVS_AUTOARRANGE` from ListView on hook initialization
2. Intercept `WM_STYLECHANGING` to block Explorer from re-enabling it
3. Intercept sort commands (`WM_COMMAND` 31492–31495) and temporarily allow `LVS_AUTOARRANGE` for Explorer's sort, then strip it again
4. After any sort or hide operation, run `CompactVisibleIcons()` to fill gaps left by hidden icons
5. Debounce compaction via timers to batch rapid changes

### 7. Drop-on-Icon Forwarding

**Problem:** Dropping a file onto an icon inside a corral should behave like dropping on a desktop icon (e.g., opening a file with an application).

**Solution:**
1. `DragOver()` detects whether the cursor is over an icon or empty space, returning `DROPEFFECT_COPY` vs `DROPEFFECT_LINK`
2. On `Drop()`, if over an icon: resolve the icon's shell path, get its `IDropTarget` via `SHGetDesktopFolder`→`GetUIObjectOf`, and forward the `IDataObject` to that target
3. On `Drop()`, if over empty space: add dropped files to the corral tab as usual

### 8. Explorer Drop Target Wrapping

**Problem:** Dragging files over hidden desktop icons still triggers Explorer's visual feedback (icon highlights, tooltips).

**Solution:** Wrap Explorer's `IDropTarget` on the desktop ListView:
1. Suppress `DragEnter`/`DragOver` calls to the original target when cursor is over a hidden icon
2. Properly manage `DragEnter`/`DragLeave` transitions when dragging between hidden and visible areas
3. Ensure the original target's state is correct before forwarding `Drop()`

---

## Threading Model

All code runs inside Explorer.exe (DexCorralHook.dll):

- **Explorer's main thread**: Hook subclass procs (NM_CUSTOMDRAW, input filtering) run on Explorer's UI thread
- **App worker thread**: Corral windows, tray icon, config, and message loop run on a dedicated worker thread created in DLL_PROCESS_ATTACH
- **Mouse hook callback**: Called on system hook thread, posts messages to worker thread
- **Folder watcher**: Separate thread monitoring file system changes, posts to worker thread window

HookBridge mediates between hook (Explorer thread) and app (worker thread) using a CRITICAL_SECTION + version counter for near-zero-cost cache checks.

---

## Performance Considerations

1. **Version Counter**: Enables hook to skip lock acquisition on every paint (just reads a volatile DWORD)
2. **Deferred Icon Loading**: Icons loaded lazily when corral becomes visible
3. **DIB Reuse**: Memory DC and DIB section created once, reused for updates
4. **Selective Invalidation**: Only affected regions trigger repaints
5. **ListView Message Caching**: Icon positions cached in App class to avoid frequent remote queries
6. **Debounced Compaction**: Icon grid compaction uses timers (200ms debounce) to batch rapid changes and avoid redundant repositioning

---

## Security Considerations

1. **COM Shell Extension**: Registered via standard COM registration; Explorer loads the DLL as a trusted shell extension
2. **Thread Safety**: CRITICAL_SECTION protects hidden icon list between app worker thread and Explorer UI thread
3. **Cross-Process Memory**: Uses proper handle and access right flags
4. **File Operations**: Standard shell file APIs used (no direct file manipulation)
5. **Registry Access**: Read-only for icon configuration

---

## Error Handling Strategy

- **Graceful Degradation**: If hook initialization fails, corrals still render (icons won't be hidden)
- **Fallback Paths**: Multiple methods to find desktop ListView (Progman vs WorkerW)
- **Config Resilience**: `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` ensures missing JSON fields get default values instead of throwing, so new config fields are automatically handled
