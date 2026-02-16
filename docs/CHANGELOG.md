# Changelog

All notable changes to DexCorral will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.0.1] - Unreleased

Initial alpha release.

### Added
- Corral windows with per-pixel alpha transparency and layered rendering
- Tab system for organizing icons into multiple groups per corral
- Drag-and-drop support for adding files, shortcuts, and shell items
- Drop-on-icon: forward drops to an icon's shell target (e.g., open with an exe)
- Icon hover effects with alpha-blended highlights
- Explorer hook (COM shell extension) for hiding corral-owned desktop icons
- Auto-arrange management: takes over Explorer's auto-arrange and compacts visible icons
- Drop target wrapping to suppress visual artifacts on hidden icons
- Input filtering: hidden icons are invisible to hit testing, selection, and keyboard navigation
- Catch-all corral for automatically capturing new desktop files
- Virtual corrals backed by any folder path
- Roll-up/hover-expand interaction for compact title-bar-only view
- Custom scrollbar rendering (PowerShell-style narrow/expand)
- In-place icon rename via double-click on label
- Context menu with delete, properties, and view mode selection
- Four view modes: small, medium, large icons, and details
- Configurable icon spacing, opacity, tint color, and tint strength
- Per-corral title bar height, font, and font color
- Multi-monitor support with per-monitor corral positioning
- Snap to edges, grid, and other corrals during drag and resize
- System tray icon with context menu for creating corrals and settings
- Desktop icon visibility toggle and shortcut arrow toggle
- JSON configuration persisted to `%APPDATA%/DexCorral/config.json`
- MSIX installer package
- Portable ZIP package with manual registration
