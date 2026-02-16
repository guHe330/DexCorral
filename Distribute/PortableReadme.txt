================================================================================
                              DexCorral (Alpha)
================================================================================

DexCorral is a high-performance, native C++ desktop organization tool for
Windows. It helps you keep your desktop clean by grouping icons into shaded,
customizable areas called "Corrals".

NOTE: This is an alpha release. Expect breaking changes, missing features,
and rough edges.

================================================================================
KEY INFORMATION
================================================================================

Status:         Alpha
Version:        C++ Native (Win32)
Binary Size:    ~750KB (exe + dll)
Platform:       Windows 10 / 11
License:        MIT

================================================================================
FEATURES
================================================================================

Desktop Organization
--------------------
* Custom Corrals: Create multiple shaded areas on your desktop to group
  related shortcuts and files.
* Icon Anchoring: Icons are positioned precisely behind Corral windows,
  ensuring they stay organized.
* Auto-Arrange Management: Takes over Explorer's auto-arrange, compacts
  visible icons to remove gaps left by hidden ones.
* Catch-All System: Automatically capture new desktop items into a designated
  Corral to prevent clutter.
* Persistence: Your Corral layout and icon assignments are saved and restored
  automatically.

User Interface
--------------
* Layered Transparency: Uses Windows layered windows for smooth semi-transparent
  color overlays.
* Icon Hover Effects: Visual hover highlights when mousing over icons.
* Drop-on-Icon: Drop files onto icons inside corrals (e.g., drop a file onto
  an executable to open it).
* Roll-up Interaction: Double-click title bars to collapse Corrals and save
  screen real estate.
* Tray Integration: Manage global settings and create new Corrals from the
  system tray.

Performance and Efficiency
--------------------------
* Native Code: Written in C++ using Win32 API for minimal resource usage and
  zero runtime overhead.
* Small Footprint: Total binary size approx. 750KB with no external
  dependencies required.
* Fast Startup: Instant application launch without JIT compilation or managed
  runtime delays.

================================================================================
INSTALLATION (PORTABLE)
================================================================================

1. Extract all files from this archive to any folder.

2. Run DexCorral.exe --register (as Administrator, one-time setup).

3. Restart Explorer or log out/in for the shell extension to load.

To uninstall, run: DexCorral.exe --unregister

NOTE: The binaries are currently unsigned. Windows SmartScreen may show a
warning when you first run the application. Click "More info" then "Run
anyway" to proceed.

An MSIX installer is also available on the Releases page for a more
integrated install experience.

================================================================================
PACKAGE CONTENTS
================================================================================

* DexCorral.exe       - Registration tool
* DexCorralHook.dll   - Explorer shell extension (all app logic)
* LICENSE             - MIT License
* readme.txt          - This file

================================================================================
MORE INFORMATION
================================================================================

* GitHub:        https://github.com/guHe330/DexCorralCpp
* Releases:      https://github.com/guHe330/DexCorralCpp/releases
* Issues/Bugs:   https://github.com/guHe330/DexCorralCpp/issues

================================================================================
LICENSE
================================================================================

This project is licensed under the MIT License - see the LICENSE file for
details.

Third-party: nlohmann/json v3.11.3 (MIT License)
https://github.com/nlohmann/json

================================================================================
