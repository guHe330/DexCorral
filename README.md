<p align="center">
  <img alt="Desktop organized with DexCorral" src="docs/assets/screenshots/desktop-after.png" width="640" />
  <h2 align="center">DexCorral</h2>
</p>

[![Build and Release](https://github.com/guHe330/DexCorral/actions/workflows/build-release.yml/badge.svg)](https://github.com/guHe330/DexCorral/actions/workflows/build-release.yml)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
[![Latest release](https://img.shields.io/github/v/release/guHe330/DexCorral?include_prereleases)](https://github.com/guHe330/DexCorral/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2011-0078d4.svg)](#requirements)

## Contents

- [About](#about)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Documentation](#documentation)
- [Issues](#issues)
- [Project Scope](#project-scope)
- [License](#license)

## About

A lightweight native Windows desktop organizer. DexCorral groups desktop icons into customizable areas called Corrals, with tabs, real per-pixel transparency, virtual folders, and multi-monitor support.

It runs as a small DLL inside Explorer instead of faking a desktop window, so Corrals draw as part of the shell and stay truly transparent over animated wallpapers such as [Lively](https://github.com/rocksdanister/lively).

> **Alpha software.** DexCorral is a personal project developed around my own workflow. Expect bugs and occasional breaking changes.

> **Private by design.** No ads, telemetry, or user tracking. Network access is limited to an optional update check, disabled by default.

## Features

### Quick-hide the whole desktop

Double-click an empty spot on the desktop and every icon and Corral fades out at once. Double-click again and everything comes back exactly as it was.

- Individual Corrals can be exempted with **Exclude from Quick-Hide**, so the ones you always want in view stay put.
- The tray menu's **Quick-Hide Everything** toggles the same state.
- It is temporary: restarting DexCorral always restores your normal desktop.

### Tabs

<img src="docs/assets/screenshots/tabs.gif" alt="Switching between tabs" width="500">

- Multiple tabs per Corral, each with its own icons and view settings.
- Virtual folders backed by real directories.

### Opacity on hover

<img src="docs/assets/screenshots/opacity-on-hover.gif" alt="Corral fading in on hover" width="500">

- Per-pixel alpha over any wallpaper, animated ones included.
- Background, header, border, and icon opacity are adjustable separately.
- Hover reveal and roll-up keep the desktop clear until you need it.

### Live appearance preview

<img src="docs/assets/screenshots/appearance-live-preview.gif" alt="Live appearance configuration" width="250">

- Appearance changes apply as you drag, with no dialog round-trip.
- Settings can be applied to one Corral or to all of them.

### More Features

- Organize desktop icons into movable, resizable Corrals.
- Per-Corral position lock against accidental moves.
- Small, medium, large, and details views.
- Catch-all Corral for new desktop items.
- Multi-monitor and resolution-aware layouts.
- Native C++ / Win32, ~1 MB executable + DLL, no runtime dependencies.

## Requirements

- Windows 11 (build 22000 or newer)
- Windows 10 is unsupported

## Installation

Download the latest version from [Releases](https://github.com/guHe330/DexCorral/releases).

### Installer

The installer registers the Explorer shell extension, configures startup, and launches DexCorral without requiring an Explorer restart.

The binaries are currently unsigned, so Windows SmartScreen may display a warning.

### Portable / Manual

Place `DexCorral.exe` and `DexCorralHook.dll` in the same directory, then run as Administrator:

```powershell
DexCorral.exe --register
DexCorral.exe --startup
```

Full installation and uninstall instructions are in the [User Manual](docs/USER_MANUAL.md#installation).

## Documentation

- [User Manual](docs/USER_MANUAL.md)
- [Changelog](docs/CHANGELOG.md)
- [Build Guide](docs/BUILD_GUIDE.md)
- [Contributing](docs/CONTRIBUTING.md)

## Issues

Bug reports are appreciated.

- [Open bugs](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+label%3Abug)
- [Feature requests](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+label%3Aenhancement)

## Project Scope

DexCorral is built primarily for my own desktop and workflow. Feature requests and pull requests are welcome, but changes that do not fit that direction may be declined.

Please open an issue before implementing larger changes.

Forks are welcome; that's what the GPL is for.

## Third-Party Libraries

- [nlohmann/json](https://github.com/nlohmann/json) 3.12.0, [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT)

## License

DexCorral is licensed under the [GPLv3](LICENSE).
