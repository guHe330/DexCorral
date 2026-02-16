# DexCorral

> **Alpha** - This project is in early development. Expect breaking changes, missing features, and rough edges. See [Known Issues & Planned Features](https://github.com/guHe330/DexCorralCpp/issues).

DexCorral is a high-performance, native C++ desktop organization tool for Windows. It helps you keep your desktop clean by grouping icons into shaded, customizable areas called "Corrals".

## Key Information

*   **Status**: Alpha (read-only repo until beta)
*   **Version**: C++ Native (Win32)
*   **Executable Size**: ~750KB (exe + dll)
*   **Platform**: Windows 10 / 11
*   **License**: MIT

## Features

### Desktop Organization
*   **Custom Corrals**: Create multiple shaded areas ("Corrals") on your desktop to group related shortcuts and files.
*   **Icon Anchoring**: Icons are positioned precisely behind Corral windows, ensuring they stay organized.
*   **Auto-Arrange Management**: Takes over Explorer's auto-arrange, compacts visible icons to remove gaps left by hidden ones.
*   **Catch-All System**: Automatically capture new desktop items into a designated Corral to prevent clutter.
*   **Persistence**: Your Corral layout and icon assignments are saved and restored automatically.

### User Interface
*   **Layered Transparency**: Uses Windows layered windows for smooth semi-transparent color overlays.
*   **Icon Hover Effects**: Visual hover highlights when mousing over icons in corrals.
*   **Drop-on-Icon**: Drop files onto icons inside corrals (e.g., drop a file onto an executable to open it).
*   **Roll-up Interaction**: Double-click title bars to collapse Corrals and save screen real estate.
*   **Tray Integration**: Manage global settings and create new Corrals from the system tray.

### Performance and Efficiency
*   **Native Code**: Written in C++ using Win32 API for minimal resource usage and zero runtime overhead.
*   **Small Footprint**: Extremely small binary size (approx. 750KB) with no external dependencies required.
*   **Fast Startup**: Instant application launch without JIT compilation or managed runtime delays.

## Installation

DexCorral is available for download on GitHub.

1.  Navigate to the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.
2.  Download the latest release containing `DexCorral.exe` and `DexCorralHook.dll`.
3.  Place both files in the same folder and run `DexCorral.exe --register` (one-time setup, requires admin).
4.  Restart Explorer or log out/in for the shell extension to load.

> **Note:** The binaries and MSIX installer are currently unsigned. Windows SmartScreen may show a warning when you first run the application. Click "More info" then "Run anyway" to proceed. Code signing will be added at a later time.

## Roadmap & Issues

*   [Planned Features](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Aenhancement) - Upcoming features and improvements
*   [Known Bugs](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Abug) - Open bug reports

## Documentation

*   [Changelog](docs/CHANGELOG.md): Release history and notable changes.
*   [Architecture](docs/ARCHITECTURE.md): In-depth technical architecture and design patterns.
*   [User Manual](docs/USER_MANUAL.md): Learn how to use all features of DexCorral.
*   [Build Guide](docs/BUILD_GUIDE.md): Instructions for compiling the project from source.

Per-release notes are also available on the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.

## Future Plans

*   **Custom icons**: The application and tray currently use default Windows icons. Custom branding icons for the exe, DLL, tray, and MSIX package are planned.
*   **Lighter config parser**: Replace [nlohmann/json](https://github.com/nlohmann/json) with a lighter-weight config parser. The current single-header library is ~25K lines for what amounts to basic load/save of a flat config structure. A minimal custom parser or a smaller library would reduce compile times and binary size.

## Third-Party Libraries

| Library | Version | License | Usage |
|---------|---------|---------|-------|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | JSON config serialization/deserialization |

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
