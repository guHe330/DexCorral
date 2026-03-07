# Changelog

All notable changes to DexCorral will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

---

## [1.0.16] - 2026-03-07

### Added
- Startup injection via `DexCorral.exe --startup`: injects DexCorralHook.dll into Explorer via WH_GETMESSAGE hook, replacing Explorer restart
- `DesktopFilter` window: blocks mouse and OLE drop interaction at hidden icon positions on the desktop
- `WakeHookProc` DLL export for startup hook injection
- `HookBridge::SetAppMessageWindow` / `GetAppMessageWindow` for hook-to-app notifications
- Tray icon retry timer when shell notification area is not ready at early startup
- Auto-start via `HKCU\...\Run` registry key (set by installer)
- Hook posts `WM_APP+100` repark notification to app after sort/compaction

### Fixed
- Win+D (Show Desktop) no longer hides corral windows (Progman ownership + WM_WINDOWPOSCHANGING guard)
- Mouse wheel routing scoped to DexCorral windows only — no longer swallows scroll in other applications
- About dialog: corrected website URL (was .app, now .com) and GitHub username typo
- License in README corrected from MIT to GPLv3

### Changed
- Installer no longer kills/restarts Explorer; uses `--startup` injection instead
- `main.cpp` refactored: `--debug` flag removed, `--startup` flag added, cleaner arg parsing
- `TrayIcon::Show()` now returns bool indicating success; tracks visibility state

---

## [1.0.15] - 2026-02-21

### Fixed
- Compatibility with third-party NM_CUSTOMDRAW hooks: pass-through to original chain in `CDDS_PREPAINT` and for non-corral icons in `CDDS_ITEMPREPAINT`
- First-launch corral now inherits default appearance settings (color, font, opacity, tint, spacing) from `AppConfig` defaults

### Changed
- Updated built-in defaults for new corrals: semi-transparent blue tint, compact spacing, slimmer title bar, Segoe UI Semibold header font

---

## [1.0.14] - 2026-02-21

### Added
- Unit test suite (Google Test via CMake FetchContent): config JSON round-trips, layout math, string utilities
- `IconUtils` module: `IsSpecialIconEntry`, `GetSpecialIconClsid`, `StripLnkExtension` extracted as pure testable functions
- `LayoutMath` module: grid and details layout calculations extracted as pure testable functions
- `INTEGRATION_TESTS.md`: manual test checklist for all Win32-dependent behaviour
- `build.ps1 -SkipTests` switch; tests now run automatically after each successful build

---

## [1.0.13] - 2026-02-20

### Added
- Unified two-pass text rendering: shadow pass + foreground pass for readable labels on any wallpaper
- Per-tab background colors shown in the title bar
- Resize snap: alignment snapping to all corrals on the same monitor; filtered to same-monitor corrals only

---

## [1.0.12] - 2026-02-20

### Fixed
- Explorer restart sequence in Inno Setup installer

### Removed
- MSIX package (replaced by Inno Setup installer)

---

## [1.0.11] - 2026-02-19

### Added
- Split apply-to-all into two separate options (appearance vs layout)

### Fixed
- Tray icon not restored after Explorer restart
- Hook DLL retry on startup if initial injection fails
- Icon label clipping when scrolled
- Font size now stored and applied as points (not pixels)

---

## [1.0.9] - 2026-02-17

### Added
- Inno Setup installer with `--silent` flag; replaces MSIX

### Fixed
- Scrollbar incorrectly appearing when all icons fit without scrolling

---

## [1.0.8] - 2026-02-17

### Added
- About dialog
- GPL-3.0 license
- Configurable icon spacing sliders (horizontal and vertical gap)
- Better scrollbar rendering
- "Apply opacity to all corrals" option

### Changed
- All app logic moved from EXE into `DexCorralHook.dll` (monolith shell extension)
- Shell extension source folder renamed to `ShellExtension/`

---

## [1.0.6] - 2026-02-09

### Fixed
- Desktop icon position tracking (internal)

---

## [1.0.5] - 2026-02-07

### Added
- Icon opacity and tint color / tint strength controls
- Special shell items (Recycle Bin, etc.) can be added to a corral
- Cloud sync status column in details view (OneDrive integration)
- Icon sorting on the desktop when corral-owned icons are hidden

### Fixed
- Icons with non-ASCII filenames failed to load

---

## [1.0.4] - 2026-02-02

### Added
- Tab system: multiple icon groups per corral window
- Mouse-wheel and trackpad scrolling

### Fixed
- Various tab and resize edge cases

---

## [1.0.3] - 2026-02-01

### Fixed
- UTF-8 filename handling in file paths

---

## [1.0.2] - 2026-02-01

### Fixed
- Icon rename interaction
- Scrollbar click-to-jump

---

## [1.0.1] - 2026-01-31

Initial alpha release.

### Added
- Corral windows with per-pixel alpha transparency and layered rendering
- Drag-and-drop support for adding files, shortcuts, and shell items to a corral
- Drop-on-icon: forward drops to an icon's shell target (e.g. open with an exe)
- Icon hover effects with alpha-blended highlights
- Explorer hook (COM shell extension) for hiding corral-owned desktop icons
- Auto-arrange management: compacts visible desktop icons to fill gaps left by hidden ones
- Input filtering: hidden icons are invisible to hit testing, rubber-band selection, and keyboard navigation
- Catch-all corral for automatically capturing new desktop files
- Virtual corrals backed by any folder path (with live folder watching)
- Roll-up / hover-expand interaction for compact title-bar-only view
- Custom narrow/expanding scrollbar
- In-place icon rename via double-click on the label
- Context menu with delete, properties, and view mode selection
- Four view modes: small, medium, large icons, and details list
- Per-corral title bar height, font face, font color
- Multi-monitor support with per-monitor corral positioning and resolution scaling
- Snap-to-edge, snap-to-grid, and snap-to-corral during drag and resize
- System tray icon with context menu for creating corrals and global settings
- Desktop icon visibility toggle and shortcut arrow overlay toggle
- JSON configuration persisted to `%APPDATA%/DexCorral/config.json` with forward/backward compatibility
- Portable ZIP package with manual registration via `DexCorral.exe --register`
