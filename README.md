# DexCorral

DexCorral is a high-performance, native C++ desktop organization tool for Windows. It helps you keep your desktop clean by grouping icons into shaded, customizable areas called "Corrals".

## Key Information

*   **Version**: C++ Native (Win32 / GDI+)
*   **Executable Size**: ~200KB
*   **Platform**: Windows 10 / 11
*   **License**: MIT

## Features

### Desktop Organization
*   **Custom Corrals**: Create multiple shaded areas on your desktop to group related shortcuts and files.
*   **Icon Anchoring**: Icons are positioned precisely behind Corral windows, ensuring they stay organized.
*   **Catch-All System**: Automatically capture new desktop items into a designated Corral to prevent clutter.
*   **Persistence**: Your Corral layout and icon assignments are saved and restored automatically.

### User Interface
*   **Wallpaper Integration**: Corrals render the underlying portion of your desktop wallpaper for a seamless look.
*   **Layered Transparency**: Uses Windows layered windows for smooth semi-transparent color overlays.
*   **Roll-up Interaction**: Double-click title bars to collapse Corrals and save screen real estate.
*   **Tray Integration**: Manage global settings and create new Corrals from the system tray.

### Performance and Efficiency
*   **Native Code**: Written in C++ using Win32 API for minimal resource usage and zero runtime overhead.
*   **Small Footprint**: Extremely small executable size (approx. 200KB) with no external dependencies required.
*   **Fast Startup**: Instant application launch without JIT compilation or managed runtime delays.

## Installation

DexCorral is available for download on GitHub.

1.  Navigate to the [Releases](https://github.com/guHe330/DexCorralCpp/releases) page.
2.  Download the latest `DexCorral.exe` and `DexCorral.Watchdog.exe`.
3.  Place both executables in the same folder and run `DexCorral.exe`.

## Documentation

Detailed information is available in the following documents:

*   [User Manual](usermanual.md): Learn how to use all features of DexCorral.
*   [Build Guide](buildguide.md): Instructions for compiling the project from source.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
