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
* Quick-Hide: Double-click an empty spot on the desktop to hide all icons and
  Corrals at once, and again to restore them. Individual Corrals can be
  exempted so they stay visible.
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

2. Register the shell extension (one-time setup). Pick who it is for:

     DexCorral.exe --register --scope=user
       Just your account. No Administrator rights needed.

     DexCorral.exe --register --scope=machine
       Every account on this PC. Run from a command prompt opened as
       Administrator.

   Without --scope, machine scope is used if this folder is under
   Program Files, and user scope otherwise.

   Do not register both ways on one PC: they share a registration and the
   per-user one silently takes precedence for that account.

   Requires Windows 11 (build 22000 or newer); on an older version this
   refuses with a message. Add --force to register anyway -- unsupported,
   untested, and not eligible for bug reports.

3. Restart Explorer or log out/in for the shell extension to load.

To uninstall, run DexCorral.exe --unregister with the same --scope you
registered with (as Administrator for --scope=machine), restart Explorer, then
delete this folder. Deleting the folder on its own leaves the shell extension
registered and pointing at a DLL that is gone, so run --unregister first.

Coming from 1.0.27 or earlier? Those versions registered without any notion of
scope. Run DexCorral.exe --cleanup-legacy once as Administrator to clear what
they left behind.

To also remove your settings:

  Remove-Item "$env:APPDATA\DexCorral" -Recurse -Force
  Remove-Item "HKCU:\Software\DexCorral" -Recurse -Force

DexCorral writes nothing else outside its own folder. The registry entries are
the shell extension's COM registration (removed by --unregister), which goes to
HKCU with --scope=user and HKLM with --scope=machine, and, under
HKCU\Software\DexCorral, your language choice and two safe-mode counters. The
full list is in the User Manual, "What DexCorral writes outside its own folder".

LANGUAGE: The UI is available in English (default) and German. Right-click the
tray icon and choose Language to switch; the change is immediate and is saved
for next time. The installer asks at install time, but the portable package has
no installer, so the tray menu is where you set it.

NOTE: The binaries are currently unsigned, so Windows SmartScreen warns and
reports the publisher as unknown. Click "More info" then "Run anyway" to
proceed. To verify this download instead of trusting it, the Releases page
lists a SHA-256 for every file:

    Get-FileHash Portable_DexCorral_<version>.zip -Algorithm SHA256

Each release is also covered by a GitHub build provenance attestation, which
ties the file to the workflow run and commit that built it:

    gh attestation verify Portable_DexCorral_<version>.zip --repo guHe330/DexCorral

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

Third-party: nlohmann/json v3.12.0 (MIT License)
https://github.com/nlohmann/json

================================================================================
