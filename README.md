# DexCorral

DexCorral is a native C++ desktop organization tool for Windows.
It helps you keep your desktop clean by grouping icons into shaded, customizable areas called "Corrals".
Corrals can also act as virtual folders — point one at a directory and its contents appear directly on your desktop, neatly contained within the Corral.

> **A personal project, shared openly.** DexCorral is built and maintained by a single developer as a passion project. It's not backed by a company or a team, just one person who wanted a better way to organize a cluttered desktop. I'm sharing it because I think others might find it useful too. Development follows my own priorities and pace, but feedback and bug reports are always welcome.

> **Privacy & Trust.** DexCorral is ad-free and collects no telemetry or user data. Everything the application needs lives in two places on your PC: `Program Files\DexCorral` and `%AppData%\DexCorral`. The only network access is the **optional update check** (off by default): when you enable it, DexCorral asks GitHub for the latest published version number and nothing else — no personal data is ever sent, and it never downloads or installs anything on its own.

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
*   **Tabs**: Each Corral can hold multiple tabs, each with its own icon group, color, view mode, and header font.
*   **Virtual Folders**: Point a Corral at any directory and its contents appear on your desktop, kept in sync as the folder changes.
*   **Real Desktop Icons**: A lightweight Explorer shell extension hides corral-owned icons from the desktop and protects them from auto-arrange, sorting, and rubber-band selection — they stay real icons with full Explorer behavior.
*   **Auto-Arrange Management**: Takes over Explorer's auto-arrange, compacts visible icons to remove gaps left by hidden ones.
*   **Catch-All System**: Automatically capture new desktop items into a designated Corral to prevent clutter.
*   **Persistence**: Your Corral layout and icon assignments are saved and restored automatically.

### User Interface
*   **Fully Adjustable Opacity**: Corrals range from fully opaque to completely transparent — at full transparency the Corral fill disappears entirely, letting your desktop wallpaper, live backgrounds, or animated scenes show through unobstructed while icons remain perfectly organized on top.
*   **Layered Transparency**: Uses Windows layered windows for smooth semi-transparent color overlays.
*   **Icon Hover Effects**: Visual hover highlights when mousing over icons in corrals.
*   **Drop-on-Icon**: Drop files onto icons inside corrals (e.g., drop a file onto an executable to open it).
*   **Roll-up Interaction**: Double-click title bars to collapse Corrals and save screen real estate.
*   **View Modes**: Small, medium, or large icon grids, or a details list with name, type, size, date, and cloud sync status.
*   **Multi-Monitor Aware**: Corral positions are remembered per monitor and per resolution.
*   **Tray Integration**: Manage global settings and create new Corrals from the system tray.

### Performance and Efficiency
*   **Native Code**: Written in C++ using Win32 API for minimal resource usage and zero runtime overhead.
*   **Small Footprint**: Extremely small binary size (approx. 750KB) with no external dependencies required.
*   **Fast Startup**: Instant application launch without JIT compilation or managed runtime delays.

## Installation

DexCorral is available for download on the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.

### Installer (recommended)

Download and run the provided installer. It registers the shell extension and starts DexCorral inside the running Explorer — no Explorer restart or logout needed — and sets up automatic start at login. Uninstalling asks whether to keep your configuration for a later reinstall.

Because the binaries are currently unsigned, Windows SmartScreen will likely block the installer on first run — click **More info** then **Run anyway** to proceed. Some antivirus software may also flag the files as suspicious; this is normal for unsigned software from small developers. Code signing will be added at a later stage.

### Manual Installation

1.  Download the latest release containing `DexCorral.exe` and `DexCorralHook.dll`.
2.  Place both files in the same folder (e.g. `C:\Program Files\DexCorral\`).
3.  Open a command prompt **as Administrator** and run `DexCorral.exe --register` (one-time setup).
4.  Run `DexCorral.exe --startup` to start DexCorral in the current session, or restart Explorer / log out and back in.

## Roadmap & Issues

*   [Planned Features](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Aenhancement) - Upcoming features and improvements
*   [Known Bugs](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Abug) - Open bug reports

## Testing

Unit tests (config JSON round-trips, layout math, string utilities) run automatically at the end of `build.ps1`. To skip them during quick iteration: `build.ps1 -SkipTests`. To run manually: `DexCorral/build/DexCorralTests.exe`.

For Win32-dependent behaviour that can't be unit tested (Explorer hook, drag-drop, DPI scaling, etc.) see [INTEGRATION_TESTS.md](docs/INTEGRATION_TESTS.md).

## Documentation

*   [Changelog](docs/CHANGELOG.md): Release history and notable changes.
*   [User Manual](docs/USER_MANUAL.md): Learn how to use all features of DexCorral.
*   [Build Guide](docs/BUILD_GUIDE.md): Instructions for compiling the project from source.

Per-release notes are also available on the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.

## Third-Party Libraries

| Library | Version | License | Usage |
|---------|---------|---------|-------|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | JSON config serialization/deserialization |

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE) file for details.
