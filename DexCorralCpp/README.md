# DexCorral - C++ Native Implementation

This is a native C++ implementation of DexCorral using Win32 API and GDI+.

## Features

- **Desktop Icon Management**: Position and organize desktop icons behind corral windows
- **Wallpaper Rendering**: Each corral renders the corresponding portion of your desktop wallpaper as its background
- **Mouse Hooks**: Global mouse hooks for creating corrals by dragging on empty desktop areas
- **System Tray Integration**: Minimize to system tray with right-click menu
- **Configuration Persistence**: JSON-based configuration stored in AppData
- **Layered Windows**: Semi-transparent corrals with color overlay

## Architecture

### Core Components

- **App.cpp**: Main application logic, mouse hook handlers, corral management
- **CorralWindow.cpp**: Individual corral window implementation with wallpaper rendering
- **DesktopIcons.cpp**: Desktop icon manipulation using ListView messages
- **WallpaperManager.cpp**: Wallpaper loading and region rendering using GDI+
- **MouseHook.cpp**: Low-level mouse hook for desktop interactions
- **TrayIcon.cpp**: System tray icon management
- **Config.cpp**: JSON configuration loading/saving using nlohmann/json

### How It Works

1. **Icon Positioning**: Unlike hiding icons off-screen, icons are positioned AT the corral location BEHIND the window
2. **Wallpaper Background**: Each corral renders the portion of desktop wallpaper at its screen coordinates
3. **Layered Windows**: Uses `WS_EX_LAYERED` for transparency and `AlphaBlend` for color overlay
4. **P/Invoke to Native**: Direct Win32 API calls instead of C# P/Invoke - no marshalling overhead

### Desktop-Overlay Architecture

This implementation follows a desktop-overlay approach:
- Icons remain at their corral position when app exits
- No continuous monitoring needed - natural Windows behavior
- Wallpaper rendered per corral (not pass-through to desktop)
- Icons survive Explorer.exe restarts

## Building

### Prerequisites

- Visual Studio 2022 or later with C++ desktop development workload
- CMake 3.15 or later
- Windows 10 SDK

### Build Instructions

1. **Using PowerShell Build Script:**
   ```powershell
   .\build.ps1
   ```

2. **Manual CMake Build:**
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   cmake --build . --config Release
   ```

3. **Output:**
   - Executable: `build\DexCorral.exe`
   - Watchdog: `build\DexCorral.Watchdog.exe`

## Configuration

Configuration is stored as JSON in:
```
%APPDATA%\DexCorral\config.json
```

### Config Structure

```json
{
  "Corrals": [
    {
      "Left": 100.0,
      "Top": 100.0,
      "Width": 300.0,
      "Height": 200.0,
      "Title": "My Corral",
      "ColorHex": "#99000000",
      "IsRolledUp": false,
      "Files": ["file1.lnk", "file2.txt"]
    }
  ]
}
```

## Usage

1. **Run the Application:**
   ```
   DexCorral.exe
   ```
   (The watchdog `DexCorral.Watchdog.exe` will be started automatically)

2. **Create a Corral:**
   - Drag on empty desktop area (> 50 pixels)
   - Or right-click tray icon → "Create New Corral (Center)"

3. **Add Files:**
   - Drag files from desktop onto corral window
   - Files are moved to corral position behind window

4. **Customize Corral:**
   - Right-click corral → "Rename"
   - Right-click corral → "Change Color"

5. **Remove from Corral:**
   - Right-click file in corral → "Remove from Corral"
   - Icon becomes visible on desktop

6. **Delete Corral:**
   - Right-click corral → "Delete Corral"
   - Files remain on desktop

## Differences from C# Version

### Advantages

- **Performance**: Native Win32 code with no managed runtime overhead
- **Size**: Smaller executable size (< 1 MB vs 60+ MB for .NET version)
- **Startup**: Faster startup time (no JIT compilation)
- **Dependencies**: No .NET runtime required

### Implementation Differences

- **Rendering**: Uses GDI+ instead of WPF ImageBrush
- **Windowing**: Direct Win32 window creation instead of WPF Window
- **JSON**: Uses nlohmann/json library (header-only)
- **File Operations**: Direct Win32 API instead of System.IO

### Current Limitations

- No file list view in corral windows (icons positioned but not displayed in UI)
- No color picker dialog (would need to implement Win32 color chooser)
- No file watcher for desktop changes
- No drag-drop from corral to desktop (would need IDropSource implementation)

## Technical Details

### Icon Positioning

Uses ListView messages to Explorer's desktop SysListView32:
- `LVM_GETITEMCOUNT`: Get number of desktop icons
- `LVM_GETITEMTEXTW`: Read icon filename
- `LVM_SETITEMPOSITION`: Move icon to coordinates
- `LVM_GETITEMPOSITION`: Read current icon position

### Wallpaper Rendering

1. Get wallpaper path via `SystemParametersInfo(SPI_GETDESKWALLPAPER)`
2. Load image using GDI+ `Image` class
3. Calculate source rectangle based on screen position
4. Render using `Graphics::DrawImage` with viewport transformation

### Memory Management

- Uses `std::unique_ptr` for RAII
- Proper cleanup of GDI+ objects
- VirtualAllocEx/VirtualFreeEx for inter-process memory

## Future Enhancements

- [ ] File list view in corral windows (custom drawing or ListView control)
- [ ] Color picker dialog using `ChooseColor` Win32 API
- [ ] Drag-drop from corral to desktop (IDropSource/IDropTarget COM)
- [ ] File system watcher for desktop changes
- [ ] Multi-monitor support with per-monitor wallpaper
- [ ] Direct2D rendering for better performance
- [ ] Icon thumbnail rendering in corrals

## License

Same as parent DexCorral project.
