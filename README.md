# DexCorral

DexCorral is a native C++ desktop organization tool for Windows. It helps you keep your desktop clean by grouping icons into shaded, customizable areas called "Corrals". 

Opacity runs from fully opaque to completely invisible, so wallpapers, live backgrounds, and animated scenes keep showing through.

Corrals can also act as virtual folders, point one at a directory and its contents appear directly on your desktop, contained within the Corral.

<img src="docs/assets/screenshots/desktop-after.png" alt="A Windows desktop organised into five corrals: System, Projects, Games, Programs, and a tabbed Work/Other corral" width="640">


> **DexCorral is a personal project** built for my own desktop, shared in case it is useful to yours. Development follows my own priorities and pace; see [Project Scope](#project-scope). Bug reports are genuinely appreciated.

> **Privacy and Trust.** DexCorral is ad-free and collects no telemetry or user data. Everything the application needs lives in two places on your PC: `Program Files\DexCorral` and `%AppData%\DexCorral`. The only network access is the **optional update check** (off by default).

> **Alpha quality.** DexCorral still has rough edges, expect bugs, missing features, and the occasional breaking change. See [Known Issues](https://github.com/guHe330/DexCorral/issues).

## Key Information

*   **Status**: Alpha, in active development
*   **Version**: C++ Native (Win32)
*   **Executable Size**: ~1 MB (exe + dll)
*   **Platform**: Windows 11 only (Windows 10 is end of life and unsupported)
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
*   **Fully Adjustable Opacity**: Corrals range from fully opaque to completely transparent — fill, header and border each have their own slider, so at the low end a Corral all but disappears and only the organized icons remain over your wallpaper, live background, or animated scene.
*   **Self-Adjusting Tabs**: Inactive tabs are derived from the header opacity automatically, staying dimmer than the active tab at every setting — one slider, no pairs to keep in sync.
*   **Hover Reveal**: Mousing over a Corral fades its header, border and icons up to full strength and gently fades them back when you leave.
*   **Layered Transparency**: Uses Windows layered windows for smooth semi-transparent color overlays.
*   **Icon Hover Effects**: Visual hover highlights when mousing over icons in corrals.
*   **Drop-on-Icon**: Drop files onto icons inside corrals (e.g., drop a file onto an executable to open it).
*   **Roll-up Interaction**: Double-click title bars to collapse Corrals and save screen real estate.
*   **View Modes**: Small, medium, or large icon grids, or a details list with name, type, size, date, and cloud sync status.
*   **Multi-Monitor Aware**: Corral positions are remembered per monitor and per resolution.
*   **Tray Integration**: Manage global settings and create new Corrals from the system tray.

### Performance and Efficiency
*   **Native Code**: Written in C++ using Win32 API for minimal resource usage and zero runtime overhead.
*   **Small Footprint**: Extremely small binary size (approx. 1 MB) with no external dependencies required.
*   **Fast Startup**: Instant application launch without JIT compilation or managed runtime delays.

## Screenshots

Corrals are shaded, resizable areas that hold your icons. Drag a file in and it stays put.

### Tabs

Each corral holds multiple tabs, so one area on the desktop can carry several groups of icons.

<img src="docs/assets/screenshots/tabs.gif" alt="Switching between tabs in a corral" width="500">

### Opacity on hover

Corrals can fade out until you need them, so they stay out of the way of the wallpaper.

<img src="docs/assets/screenshots/opacity-on-hover.gif" alt="A corral fading in as the mouse moves over it" width="500">

### Live appearance preview

Background colour, opacity, header height and font, icon tint and spacing — every change previews on the real corral while you drag the slider.

<img src="docs/assets/screenshots/appearance-live-preview.gif" alt="Adjusting sliders in the Appearance dialog while the corral updates live" width="250">

## Installation

DexCorral is available for download on the [Releases](https://github.com/guHe330/DexCorral/releases) page.

> **Windows 11 required.** Windows 10 reached end of life and DexCorral is neither tested nor supported on it. The installer refuses to run below Windows 11 (build 22000), and so does `DexCorral.exe --register` in the portable package.

### Installer (recommended)

Download and run the provided installer. It registers the shell extension and starts DexCorral inside the running Explorer — no Explorer restart or logout needed — and sets up automatic start at login. Uninstalling asks whether to keep your configuration for a later reinstall.

Because the binaries are currently unsigned, Windows SmartScreen will likely block the installer on first run — click **More info** then **Run anyway** to proceed. Some antivirus software may also flag the files as suspicious; this is normal for unsigned software from small developers. Code signing will be added at a later stage.

### Manual Installation

1.  Download the latest release containing `DexCorral.exe` and `DexCorralHook.dll`.
2.  Place both files in the same folder (e.g. `C:\Program Files\DexCorral\`).
3.  Open a command prompt **as Administrator** and run `DexCorral.exe --register` (one-time setup). On an unsupported Windows version this refuses with a message; `--register --force` registers anyway, at your own risk and without support.
4.  Run `DexCorral.exe --startup` to start DexCorral in the current session, or restart Explorer / log out and back in.

## Issues

*   [Open Bugs](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+label%3Abug) - Known problems
*   [Feature Requests](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+label%3Aenhancement) - Ideas under consideration, subject to [Project Scope](#project-scope)

## Testing

Unit tests (config JSON round-trips, layout math, string utilities) run automatically at the end of `build.ps1`. To skip them during quick iteration: `build.ps1 -SkipTests`. To run manually: `DexCorral/build/DexCorralTests.exe`.

Win32-dependent behaviour (Explorer hook, drag-drop, DPI scaling, etc.) is not covered by the unit tests and has to be verified by hand.

## Documentation

*   [Changelog](docs/CHANGELOG.md): Release history and notable changes.
*   [User Manual](docs/USER_MANUAL.md): Learn how to use all features of DexCorral.
*   [Build Guide](docs/BUILD_GUIDE.md): Instructions for compiling the project from source.

Per-release notes are also available on the [Releases](https://github.com/guHe330/DexCorral/releases) page.

## Third-Party Libraries

| Library | Version | License | Usage |
|---------|---------|---------|-------|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | JSON config serialization/deserialization |

## Project Scope

DexCorral is a tool I built for my own desktop, and I develop it to fit my own workflow. That is
the entire design brief.

Anything that does not serve that vision gets declined - theming, animations, per-corral
backgrounds, and the like - no matter how well it is written. That is not a judgement of the idea
or of your code; it simply is not what this project is. Please open an issue before you start
writing, so neither of us spends an evening on something that was never going to be merged.

If you want a DexCorral that works differently, fork it. That is what the GPL is for, and the
invitation is sincere.

## Contributing

Bug reports, feature suggestions, and pull requests are welcome - see [CONTRIBUTING.md](docs/CONTRIBUTING.md).
Expect a review within a week or so; this is a side project.

Contributors sign a one-time [Contributor License Agreement](docs/CLA.md) confirming their work is
licensed under the GPLv3. A bot handles it automatically on your first pull request.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE) file for details.
