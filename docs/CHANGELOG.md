# Changelog

All notable changes to DexCorral will be documented in this file. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Entries describe what changed from a user's point of view, and why when that is not obvious. Implementation detail belongs in the commit history.

---

## [Unreleased]

## [1.0.27] - 2026-09-02

### Security
- Releases now publish a SHA-256 for the installer and the portable zip, and both artifacts carry a GitHub build provenance attestation tying them to the workflow run and commit that built them. The binaries are still unsigned, so SmartScreen still warns and reports an unknown publisher; this at least makes a download checkable instead of something you have to take on trust. Verify with `Get-FileHash`, or with `gh attestation verify <file> --repo guHe330/DexCorral`.

### Changed
- The unsigned-binaries note in the README, the User Manual and the portable readme now points at those checks instead of just telling you to click through the SmartScreen warning.
- README restructured: features are grouped with the screenshots that show them, and quick-hide (double-click the desktop to hide every icon and Corral, with per-Corral opt-out) is documented there and in the portable readme for the first time.
- Documentation throughout dropped em dashes and hard-wrapped paragraphs.

## [1.0.25] - 2026-09-02

### Added
- **Language** submenu in the tray menu switches the interface language while the app is running, with no restart, no editing `config.json`. It overrides the installer's choice, and for the portable package it is the only way to change the language, since nothing writes one at install time. The choice also applies to `DexCorral.exe`'s own registration dialogs. Names already given to Corrals and tabs keep their wording, being your text rather than the app's.
- The User Manual now documents everything DexCorral writes outside its own folder: every registry key, why it exists, and whether the uninstaller removes it. The uninstall notes and the portable package's readme point at it, and the portable instructions now cover the registry rather than stopping at "delete the folder".

### Fixed
- Uninstalling no longer leaves `HKCU\Software\DexCorral` behind. The installer deleted the language value it had written, but the safe-mode counters are written later, from inside Explorer, so the key survived with them still in it. The whole key now goes.

## [1.0.24] - 2026-08-31

### Added
- **Lock Position** in a Corral's context menu freezes it where it is: no mouse move, no mouse resize, and a small padlock in the title bar to say so. Tabs, roll-up and the per-monitor position memory all keep working. Aimed at the Corral that lives where you drop files. An accidental title-bar drag also shoves the desktop icons underneath it around, with no undo.
- Keyboard support in Corrals, matching Explorer: arrow keys move the selection, Home/End jump to the first/last icon, Esc clears it, and type-ahead jumps to a name. Delete, Enter, Alt+Enter and Ctrl+C/X/V run the shell's own verbs, so confirmations, the Recycle Bin and Explorer's undo all apply. F5 reloads, the Menu key opens the context menu, Backspace goes up a folder.
- Multi-select in Corrals: Ctrl+click, Shift+click, Ctrl+A, Shift+arrows, and a drag-out selection rectangle. Commands act on the whole selection in one shell call, so a multi-file delete is a single undo step.
- German: the whole interface is available in German. The installer asks which language you want and the app follows that choice. Anything a translation misses falls back to English rather than showing blank.
- Text opacity in the Appearance dialog: **Header label** fades a tab's title, **Icon label** fades the icon captions. Both go all the way to invisible, and both fade back in when you point at the Corral, so text set to 0% is hidden at rest rather than gone for good.
- GitHub issue forms for bug reports, feature requests, and multi-monitor test reports.
- Contributing guide gains a "Testing is a contribution" section naming the untested territory: multiple monitors, mixed DPI, unusual display hardware, and large desktops.

### Changed
- All four ways of creating a Corral now place it the same way: the desktop right-click uses the point you clicked (it used to ignore it and tile from the top-right), a Corral's own menu and the tray now avoid overlapping existing Corrals. Placement seeks the nearest free spot, capped so a crowded desktop puts the Corral where you asked rather than across the monitor. Desktop icons are deliberately not avoided; they are pushed out from under a Corral as before.
- Every opacity slider now sits together in the Appearance dialog's **Opacity** section, each one directly under what it affects.
- DexCorral now requires Windows 11 (build 22000 or newer). Windows 10 is end of life and there is no machine to test it on, so the installer and `--register` refuse to run below that build. `--register --force` overrides the check, unsupported and untested.
- Downloads are named after their version (`Portable_DexCorral_<version>.zip`), so packages from different releases stay distinguishable once downloaded.
- Release notes show the real download sizes instead of a hand-maintained figure that had drifted from reality.
- Vendored nlohmann/json bumped from 3.11.3 to 3.12.0; the SBOM records the new version and checksum.
- Documentation trimmed: the README is a short overview that points at the manual instead of repeating it, and changelog entries are now brief user-facing lines rather than symbol-level detail.

### Fixed
- New Corrals no longer appear behind the ones already on the desktop, and the Corral you interact with is raised above its neighbours instead of being sunk below them. Corrals still sit below ordinary windows as a group; they now have a sensible order *within* that group, so a click on an overlapped Corral acts on the one you clicked.
- New Corrals are DPI-scaled. The default 300x200 was raw pixels while everything drawn inside scaled with the monitor, so a new Corral came out undersized on high-DPI displays.
- Hover-expanding a rolled-up Corral now waits for a short dwell, and only one Corral may be hover-expanded at a time. An exposed sliver of a neighbouring Corral no longer opens itself while you travel to another.
- Upgrading no longer fails with "cannot replace `DexCorralHook.dll`". The installer used to restart Explorer *before* copying the new files, so a fresh Explorer could load the old extension again and lock the file mid-copy. It now moves the old DLL aside first (Windows allows renaming a file that is still loaded), copies onto the freed name, and restarts Explorer afterwards. The moved-aside copy is deleted in the same pass, once Explorer has restarted and released it; anything still held falls back to deletion at the next reboot.
- Resizing a corral no longer stalls part-way. A corral sits behind everything else, and Windows stops sending a background window mouse messages the moment the pointer leaves it, so any drag that outran the frame, or that snapped an edge out from under the pointer, silently froze. Resizing, moving and details-column dragging now follow the pointer wherever it goes.
- Diagonal resizing works. The corner grab zones were a 6×6 pixel target, smaller than the grip drawn in the bottom-right corner; the bottom corners now match the grip and the top corners are twice as wide.
- A black header font is no longer invisible. Header text is very slightly softer as a result, and now renders correctly over any wallpaper.

### Security
- The update check only ever opens a DexCorral release page. It reads the release address from GitHub's API and previously handed whatever came back straight to Windows to open; it is now checked against the project's own repository first, and anything else falls back to the releases page. A tampered API response can no longer make DexCorral open an arbitrary link or local path.
- The release build pins every GitHub Action it uses to an exact revision, so a change made upstream cannot alter the build that produces a release without that change being reviewed first.
- A new CI check guards all three of the above on every push, so a later change cannot quietly undo them.
- Every release now ships a machine-readable bill of materials (`DexCorral_<version>_sbom.cdx.json`, CycloneDX) listing what DexCorral is built from, with a checksum for each component, so you can check what is inside a build without taking our word for it. The build verifies that the bundled JSON library still matches its recorded checksum and that the document describes the version actually being released, and a weekly job watches for new upstream releases.
- The test framework is now fetched by content hash rather than by tag, so a build cannot silently pick up a different version than the one it was checked against.

## [1.0.22] - 2026-08-27

### Added
- Header opacity is now a per-corral setting, with its own slider in the Appearance dialog. It stops short of fully transparent because the header is a corral's only grab handle: at zero it would be an invisible window that still swallows the mouse.
- Border opacity is now a per-corral setting. At zero the corral is a frameless shape on the wallpaper; the resize grip stays visible.
- Inactive tabs are derived from the header opacity instead of being configured separately, so they stay dimmer than the active tab at every setting, with no second slider to keep in sync.
- Hovering a corral now fades its header and border to full strength alongside the icons, so a near-invisible corral presents a solid grab handle the moment you reach for it.
- Both new opacity settings are covered by "use as default for new corrals", "apply changes to all corrals", and "copy full style to all corrals".

### Changed
- The hover fade is no longer symmetric: fading in stays at 200 ms, fading out is 400 ms. Crossing between adjacent corrals used to make the chrome flash out and back in.
- A rolled-up corral keeps a minimum header visibility, since rolled up the header is the entire corral. The configured value is untouched and returns on unroll.
- The Appearance dialog's Opacity group now holds every opacity setting as labelled rows: background, border, header, and icons.

### Fixed
- Clicking a tab often did nothing: the top few pixels of every tab started an invisible resize instead of activating the tab. Corners still resize from the top.
- A tab's reorder grip could be grabbed on tabs that were not showing one.
- A corral could get stuck in resize or drag mode when the mouse button was released outside it. The released size and position are saved; drop side effects such as merging into another corral are skipped, since the release point is unknown by then.
- Documentation and installer links pointed at a repository that does not exist.

## [1.0.21] - 2026-08-22

### Added
- DexCorral's own icon is used for the application, the tray, corral windows, and the installer, instead of the generic Windows default.

### Changed
- Desktop rename and delete notifications, and desktop context-menu commands, are processed on DexCorral's own thread.

### Fixed
- Virtual corral entries whose files have disappeared are pruned instead of rendering as blank ghost icons.
- "Remove from Corral" stays available for entries the shell can no longer resolve.
- `.url` shortcuts get their extension stripped in fallback display names, as `.lnk` already did.
- Corrals no longer pop above other applications when a different window is activated or an icon inside a corral is clicked.
- Dragging an icon over a scrolled corral highlighted the wrong slot.

## [1.0.20] - 2026-06-23

### Added
- Tabs can be reordered by dragging the grip handle on their left edge, revealed by hovering a tab when a corral has more than one.

## [1.0.19] - 2026-06-23

### Added
- Virtual corrals are browsable like an Explorer pane: double-click a sub-folder to navigate into it, and a "folder up" button returns. It never navigates above the linked folder, and the current sub-path survives restarts.
- Details view gains a sortable column header row (Name, Type, Size, Date modified), mirrored by a "Sort By" context submenu. Folders always sort before files, and the choice persists.
- Details columns are resizable by dragging the header separators; widths are remembered per tab.
- Virtual corrals survive their folder being renamed, moved, or deleted: the view navigates up to the nearest existing folder, or offers a relink if the linked folder itself is gone.
- Optional update check, off by default. When enabled, DexCorral asks GitHub once a day whether a newer release exists and shows a tray balloon; clicking it opens the release page. Nothing is downloaded automatically.
- Quick-hide: double-click an empty spot on the desktop to fade out all icons and corrals at once, and again to bring them back. Corrals can opt out individually, and the tray menu toggles the same state.
- Desktop "Sort by" is carried out by DexCorral itself, so Explorer never repositions corral-owned icons.
- Corral-owned icons cannot be moved by Explorer or third-party tools: only DexCorral moves them.
- Crash containment: if anything goes wrong inside the Explorer hook, it releases the hidden icons and goes inert for the session instead of taking Explorer down with it.
- Safe mode: three consecutive sessions dying at startup start DexCorral with the hook disabled and a tray balloon, then retry normally on the next session.

### Changed
- Detaching a tab places the new corral in free space next to the one it came from instead of on top of it.
- New corrals open in free space instead of overlapping existing ones.
- New corrals default to 100% icon spacing.
- The catch-all can be turned off entirely; having no catch-all at all is now allowed, and startup no longer forces one.
- Auto-arrange no longer leaves empty grid slots where hidden icons used to be, and works even when nothing is hidden.
- Hidden icons are matched by their full path rather than their display name, so a name collision can no longer hide the wrong icon.
- With debug logging off (the default), DexCorral writes no log files at all.
- Unhooking leaves other tools' subclasses intact: if something installed after DexCorral, its hook stays in place but inert rather than breaking the other tool.
- The version in `Version.h` is the single source of truth, and the release workflow verifies the pushed tag matches it.
- Release builds are pinned to a runner that still ships Visual Studio 2022.
- Docs: the user manual was rewritten for the current architecture, and a stale architecture document was removed.

### Fixed
- Corral-owned icons could act as invisible drop targets, most visibly below a rolled-up corral. Explorer registers its desktop drop target lazily, so DexCorral's wrapper often never got installed; it is now installed and re-verified for the life of the session.
- Two desktop items with the same display name no longer both get hidden when only one is in a corral.
- Corral labels respect Explorer's "hide extensions for known file types" setting, and renaming inside a corral re-appends the hidden extension instead of changing the file type.
- Files created via the desktop "New" menu are no longer yanked into the catch-all mid-rename.
- Dragging one of two same-named icons no longer moves both, and a free name-twin of a corral-owned icon is no longer skipped when icons are pushed out of the way.
- Renaming a corral-owned file no longer makes it reappear on the desktop, leave a visible duplicate, or flicker during the rename.
- Hidden and system files such as `desktop.ini` are no longer adopted into the catch-all as ghost icons, and empty entries can no longer render as a stray "Desktop" icon.

### Removed
- The "Start with Windows" tray toggle. It was misleading: Explorer loads DexCorral's shell extension at every login regardless, so turning it off never actually stopped DexCorral.

### Known Limitations
- Corral membership is stored as a bare filename, so two files with the same name on the user and Public desktop cannot be told apart (the user-desktop one wins). Fixing it needs paths in the config plus a migration.

## [1.0.16] - 2026-03-07

### Added
- DexCorral starts inside the running Explorer via `DexCorral.exe --startup`, so installing no longer restarts Explorer.
- Auto-start at login, set up by the installer.
- Mouse clicks and drops are blocked at hidden icon positions on the desktop.
- The tray icon retries when the notification area is not ready yet at login.

### Changed
- The installer no longer kills and restarts Explorer.
- Command line: `--debug` removed, `--startup` added.

### Fixed
- Win+D (Show Desktop) no longer hides corral windows.
- Mouse wheel scrolling is scoped to DexCorral windows and no longer swallowed from other applications.
- About dialog: corrected website URL and GitHub username.
- License in the README corrected from MIT to GPLv3.

## [1.0.15] - 2026-02-21

### Fixed
- Compatibility with third-party tools that also hook desktop icon drawing.
- The first-launch corral inherits the default appearance settings instead of starting bare.

### Changed
- New defaults for new corrals: semi-transparent blue tint, compact spacing, slimmer title bar, Segoe UI Semibold header font.

## [1.0.14] - 2026-02-21

### Added
- Unit test suite covering config round-trips, layout math, and string handling. Tests run automatically after each successful build; `build.ps1 -SkipTests` skips them.

## [1.0.13] - 2026-02-20

### Added
- Two-pass label rendering (shadow plus foreground) keeps icon labels readable on any wallpaper.
- Per-tab background colors are shown in the title bar.
- Resizing snaps to other corrals on the same monitor.

## [1.0.12] - 2026-02-20

### Fixed
- Explorer restart sequence in the installer.

### Removed
- MSIX package, replaced by the Inno Setup installer.

## [1.0.11] - 2026-02-19

### Added
- Apply-to-all is split into two options: appearance and layout.

### Fixed
- Tray icon not restored after an Explorer restart.
- The hook retries if the initial injection at startup fails.
- Icon labels were clipped when scrolled.
- Font size is stored and applied in points rather than pixels.

## [1.0.9] - 2026-02-17

### Added
- Inno Setup installer, with a silent install flag; replaces MSIX.

### Fixed
- Scrollbar appeared when all icons already fit.

## [1.0.8] - 2026-02-17

### Added
- About dialog.
- GPL-3.0 license.
- Icon spacing sliders, horizontal and vertical.
- "Apply opacity to all corrals".
- Better scrollbar rendering.

### Changed
- All application logic moved into the shell extension DLL.

## [1.0.6] - 2026-02-09

### Fixed
- Desktop icon position tracking.

## [1.0.5] - 2026-02-07

### Added
- Icon opacity, tint color, and tint strength.
- Special shell items such as the Recycle Bin can be added to a corral.
- Cloud sync status column in details view, for OneDrive files.
- Desktop icon sorting while corral-owned icons are hidden.

### Fixed
- Icons with non-ASCII filenames failed to load.

## [1.0.4] - 2026-02-02

### Added
- Tabs: multiple icon groups per corral.
- Mouse-wheel and trackpad scrolling.

### Fixed
- Various tab and resize edge cases.

## [1.0.3] - 2026-02-01

### Fixed
- UTF-8 filename handling in file paths.

## [1.0.2] - 2026-02-01

### Fixed
- Icon rename interaction.
- Scrollbar click-to-jump.

## [1.0.1] - 2026-01-31

Initial alpha release.

### Added
- Corral windows with per-pixel alpha transparency.
- Drag and drop of files, shortcuts, and shell items into a corral.
- Drop-on-icon: dropping a file onto an icon invokes its target.
- Icon hover highlights.
- Explorer shell extension that hides corral-owned desktop icons and keeps them out of hit testing, rubber-band selection, and keyboard navigation.
- Auto-arrange management: visible desktop icons are compacted to fill the gaps left by hidden ones.
- Catch-all corral for new desktop files.
- Virtual corrals backed by any folder, kept in sync as the folder changes.
- Roll-up to a title-bar-only view, with hover-expand.
- In-place icon rename.
- Context menu with delete, properties, and view mode.
- Four view modes: small, medium, and large icons, and a details list.
- Per-corral title bar height, font face, and font color.
- Multi-monitor support with per-monitor positioning and per-resolution scaling.
- Snap to screen edges, to the grid, and to other corrals while dragging and resizing.
- Tray icon for creating corrals and global settings.
- Desktop icon visibility and shortcut arrow overlay toggles.
- JSON configuration in `%APPDATA%\DexCorral`, forward and backward compatible.
- Portable ZIP package with manual registration.
