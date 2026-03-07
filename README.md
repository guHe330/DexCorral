# DexCorral

DexCorral is a native C++ desktop organization tool for Windows.
It helps you keep your desktop clean by grouping icons into shaded, customizable areas called "Corrals".
Corrals can also act as virtual folders — point one at a directory and its contents appear directly on your desktop, neatly contained within the Corral.

> **A personal project, shared openly.** DexCorral is built and maintained by a single developer as a passion project. It's not backed by a company or a team, just one person who wanted a better way to organize a cluttered desktop. I'm sharing it because I think others might find it useful too. Development follows my own priorities and pace, but feedback and bug reports are always welcome.

> **Privacy & Trust.** DexCorral is ad-free, collects no telemetry or user data, and never communicates with any server. Everything the application needs lives in two places on your PC: `Program Files\DexCorral` and `%AppData%\DexCorral`. That's it — nothing leaves your machine.

> **Alpha** — This project is in early development. Expect breaking changes, missing features, and rough edges. See [Known Issues & Planned Features](https://github.com/guHe330/DexCorralCpp/issues).

## Key Information

*   **Status**: Alpha (read-only repo until beta)
*   **Version**: C++ Native (Win32)
*   **Executable Size**: ~750KB (exe + dll)
*   **Platform**: Windows 11
*   **License**: GPLv3

## Features

### Desktop Organization
*   **Custom Corrals**: Create multiple shaded areas ("Corrals") on your desktop to group related shortcuts and files.
*   **Icon Anchoring**: Icons are positioned precisely behind Corral windows, ensuring they stay organized.
*   **Auto-Arrange Management**: Takes over Explorer's auto-arrange, compacts visible icons to remove gaps left by hidden ones.
*   **Catch-All System**: Automatically capture new desktop items into a designated Corral to prevent clutter.
*   **Persistence**: Your Corral layout and icon assignments are saved and restored automatically.

### User Interface
*   **Fully Adjustable Opacity**: Corrals range from completely transparent to fully opaque — at full opacity the Corral fill disappears entirely, letting your desktop wallpaper, live backgrounds, or animated scenes show through unobstructed while icons remain perfectly organized on top.
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

DexCorral is available for download on the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.

### Installer (recommended)

Download and run the provided installer. Because the binaries are currently unsigned, Windows SmartScreen will likely block it on first run — click **More info** then **Run anyway** to proceed. Some antivirus software may also flag the files as suspicious; this is normal for unsigned software from small developers. Code signing will be added at a later stage.

### Manual Installation

1.  Download the latest release containing `DexCorral.exe` and `DexCorralHook.dll`.
2.  Place both files in the same folder (e.g. `C:\Program Files\DexCorral\`).
3.  Open a command prompt **as Administrator** and run `DexCorral.exe --register` (one-time setup).
4.  Restart Explorer or log out/in for the shell extension to load.

## Roadmap & Issues

*   [Planned Features](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Aenhancement) - Upcoming features and improvements
*   [Known Bugs](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Abug) - Open bug reports

## Testing

Unit tests (config JSON round-trips, layout math, string utilities) run automatically at the end of `build.ps1`. To skip them during quick iteration: `build.ps1 -SkipTests`. To run manually: `DexCorral/build/DexCorralTests.exe`.

For Win32-dependent behaviour that can't be unit tested (Explorer hook, drag-drop, DPI scaling, etc.) see [INTEGRATION_TESTS.md](INTEGRATION_TESTS.md).

## Documentation

*   [Changelog](docs/CHANGELOG.md): Release history and notable changes.
*   [Architecture](docs/ARCHITECTURE.md): In-depth technical architecture and design patterns.
*   [User Manual](docs/USER_MANUAL.md): Learn how to use all features of DexCorral.
*   [Build Guide](docs/BUILD_GUIDE.md): Instructions for compiling the project from source.

Per-release notes are also available on the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.

## Third-Party Libraries

| Library | Version | License | Usage |
|---------|---------|---------|-------|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | JSON config serialization/deserialization |

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE) file for details.
