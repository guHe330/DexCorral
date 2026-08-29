================================================================================
                              DexCorral (Alpha)
================================================================================

DexCorral is a high-performance, native C++ desktop organization tool for
Windows. It helps you keep your desktop clean by grouping icons into shaded,
customizable areas called "Corrals". Corrals can also act as virtual
folders -- point one at a directory and its contents appear directly on
your desktop, neatly contained within the Corral.

NOTE: This is an alpha release. Expect breaking changes, missing features,
and rough edges.

================================================================================
KEY INFORMATION
================================================================================

Status:         Alpha
Version:        C++ Native (Win32)
Binary Size:    ~1 MB (exe + dll)
Platform:       Windows 11
License:        GPL-3.0

================================================================================
FEATURES
================================================================================

Desktop Organization
--------------------
* Custom Corrals: Create multiple shaded areas on your desktop to group
  related shortcuts and files.
* Tabs: Each Corral can hold multiple tabs, each with its own icon group,
  color, view mode, and header font.
* Virtual Folders: Point a Corral at any directory and its contents appear
  on your desktop, kept in sync as the folder changes.
* Real Desktop Icons: A lightweight Explorer shell extension hides
  corral-owned icons from the desktop and protects them from auto-arrange,
  sorting, and rubber-band selection -- they stay real icons with full
  Explorer behavior.
* Auto-Arrange Management: Takes over Explorer's auto-arrange, compacts
  visible icons to remove gaps left by hidden ones.
* Catch-All System: Automatically capture new desktop items into a designated
  Corral to prevent clutter.
* Persistence: Your Corral layout and icon assignments are saved and restored
  automatically.

User Interface
--------------
* Fully Adjustable Opacity: Corrals range from fully opaque to completely
  transparent, letting your desktop wallpaper or live backgrounds show
  through while icons stay organized on top.
* Layered Transparency: Uses Windows layered windows for smooth semi-transparent
  color overlays.
* Icon Hover Effects: Visual hover highlights when mousing over icons.
* Drop-on-Icon: Drop files onto icons inside corrals (e.g., drop a file onto
  an executable to open it).
* Roll-up Interaction: Double-click title bars to collapse Corrals and save
  screen real estate.
* View Modes: Small, medium, or large icon grids, or a details list with
  name, type, size, date, and cloud sync status.
* Multi-Monitor Aware: Corral positions are remembered per monitor and per
  resolution.
* Tray Integration: Manage global settings and create new Corrals from the
  system tray.

Performance and Efficiency
--------------------------
* Native Code: Written in C++ using Win32 API for minimal resource usage and
  zero runtime overhead.
* Small Footprint: Total binary size approx. 1 MB with no external
  dependencies required.
* Fast Startup: Instant application launch without JIT compilation or managed
  runtime delays.

================================================================================
INSTALLATION (PORTABLE)
================================================================================

1. Extract all files from this archive to any folder.

2. Run DexCorral.exe --register (as Administrator, one-time setup).
   Requires Windows 11 (build 22000 or newer); on an older version this
   refuses with a message. Add --force to register anyway -- unsupported,
   untested, and not eligible for bug reports.

3. Restart Explorer or log out/in for the shell extension to load.

To uninstall, run: DexCorral.exe --unregister

LANGUAGE: The UI is available in English (default) and German. The installer
asks for the language; for the portable package, set "Language": "de" (or
"en") in %APPDATA%\DexCorral\config.json.

NOTE: The binaries are currently unsigned. Windows SmartScreen may show a
warning when you first run the application. Click "More info" then "Run
anyway" to proceed.

An Inno Setup installer is also available on the Releases page for a more
integrated install experience.

================================================================================
PACKAGE CONTENTS
================================================================================

* DexCorral.exe       - Registration tool
* DexCorralHook.dll   - Explorer shell extension (all app logic)
* LICENSE             - GPL-3.0 License
* readme.txt          - This file

================================================================================
MORE INFORMATION
================================================================================

* GitHub:        https://github.com/guHe330/DexCorral
* Releases:      https://github.com/guHe330/DexCorral/releases
* Issues/Bugs:   https://github.com/guHe330/DexCorral/issues

================================================================================
LICENSE
================================================================================

This project is licensed under the GPL-3.0 License - see the LICENSE file for
details.

Third-party: nlohmann/json v3.11.3 (MIT License)
https://github.com/nlohmann/json

================================================================================
