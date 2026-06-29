## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ~750KB |
| `Portable_DexCorral.zip` | Portable package (no installer) | ~750KB |

> **Note:** Both packages are currently unsigned. Windows SmartScreen may show a warning on first run. Click "More info" then "Run anyway" to proceed.

### Installation Options

#### Option 1: Installer (Recommended)
**For Windows 10/11 users**

1. Download `DexCorral_${VERSION}_Setup.exe`
2. Run the installer (requires Administrator)
3. The installer registers the shell extension and restarts Explorer automatically

#### Option 2: Portable Package
**For advanced users or non-administrator installs**

1. Download `Portable_DexCorral.zip`
2. Extract to any folder
3. Run `DexCorral.exe --register` (as Administrator)
4. Restart Explorer or log out/in

**Package Contents:**
- `DexCorral.exe` - Registration tool
- `DexCorralHook.dll` - Explorer shell extension
- `LICENSE` - GPL-3.0 License
- `readme.txt` - Quick start guide

### What's New

- Tabs can now be reordered by dragging: hover a tab to reveal a small grip handle on its left edge, then drag it left or right to move the tab. The new order is saved automatically

<!-- Older changes (v1.0.19) -->

- The catch-all corral can now be turned off completely — right-click a catch-all tab and choose "Catch-All" again to disable it. As before, only one corral can be catch-all at a time; now you can also choose to have none, in which case new desktop files are left where they land until you re-enable a catch-all
- Virtual corrals can now be browsed like a mini Explorer (Details view): double-click a sub-folder to open it inline, and use the "up" button in the title bar to go back (it appears only once you've gone below the linked folder). Where you navigated is remembered between restarts
- Details view now has clickable column headers (Name, Type, Size, Date modified) — click to sort, click again to reverse; folders always come first. You can also pick the sort from the right-click "Sort By" menu, and the choice is remembered
- Details columns are resizable — drag the lines between headers; widths are remembered per tab
- If a virtual corral's folder (or a sub-folder you're in) gets renamed, moved, or deleted, the corral steps back up to the nearest existing folder instead of breaking — or shows "Folder unavailable, right-click to relink" if the whole folder is gone
- New corrals (desktop context menu and tray menu, file and virtual) now open in free space starting from the top-right corner instead of overlapping existing corrals
- Detaching a tab now drops the new corral into free space right next to the one it came from, instead of landing on top of it
- New corrals now default to 100% horizontal and vertical icon spacing
- Optional update check (off by default): turn on "Check for Updates Automatically" in the tray menu and DexCorral will let you know via a tray notification when a new version is available (click it to open the download page); "Check for Updates Now" checks on demand. No automatic downloads
- Quick-hide: double-click an empty spot on the desktop to hide everything — native icons and all corrals (with a fade) — and double-click again to bring it all back. Per-corral "Exclude from Quick-Hide" keeps chosen corrals visible; also available as "Quick-Hide Everything" in the tray menu
- Desktop sorting is now owned by DexCorral: "Sort by" never moves corral-owned icons and no longer leaves empty grid slots where hidden icons used to be
- Corral-owned icons are immune to Explorer repositioning (auto-arrange, align-to-grid, third-party tools) — only DexCorral can move them
- Desktop sort/compaction repaints in a single pass (no flicker of intermediate layouts)
- Icons are now identified by their real shell identity (full path), not display name — same-named items can no longer be hidden by mistake
- Faster desktop painting and hit-testing: hidden-icon lookups are cached per item instead of re-resolved on every paint
- Corral labels respect "Hide extensions for known file types"; toggling the setting no longer duplicates icons
- Creating a file via right-click > New keeps the inline rename: the catch-all corral waits until you've finished typing the name
- DexCorral's auto-arrange keeps the desktop compacted when files are added or removed
- Startup injection: new `--startup` flag injects DexCorralHook.dll into Explorer via WH_GETMESSAGE hook — no Explorer restart needed on install or login
- Removed the "Start with Windows" tray toggle — DexCorral is loaded by Explorer at every login automatically (it's a shell extension), so the toggle did nothing useful and could mislead. Startup is now always on
- Win+D immunity: corral windows are now owned by Progman and block SWP_HIDEWINDOW, so Show Desktop no longer hides them
- Desktop filter window blocks mouse/drop interaction at hidden icon positions
- Tray icon retry: handles shell notification area not being ready at early Explorer startup
- Mouse wheel fix: WM_MOUSEWHEEL re-routing now scoped to DexCorral windows only, no longer swallowing scroll in other apps
- Hook-to-app repark notification: hidden icons are repositioned under their corrals after sort/compaction
- Crash containment: every hook entry point Explorer calls is exception-guarded — on an unexpected error the hook deactivates itself and releases all icons instead of taking Explorer down
- Safe mode: if Explorer keeps dying right after the hook starts, DexCorral starts with the hook disabled and shows a tray notice; the next start tries again normally
- Debug log files are now fully opt-in (DebugLogging config flag) — drag-and-drop no longer writes a log file on every mouse move
- Desktop context menu commands (Sort by, Auto arrange, Align to grid) are recognized by menu caption, not just hard-coded IDs, for resilience across Windows builds
- Uninstalling/disabling no longer breaks other desktop tools that hooked the desktop after DexCorral
- Installer simplified: no Explorer restart during install, auto-start via Run registry key
- Fixed: hidden corral-owned icons could silently accept drag-drops (e.g. below a rolled-up corral) — the drop blocker now reliably installs even when Explorer registers its drop target late, and re-installs itself if another tool takes over the drop target
- Fixed About dialog URL and GitHub link
- Documentation overhaul: user manual rewritten to cover tabs, virtual corrals, the appearance dialog, and the installer flow; uninstall guide and README brought up to date

### Known Issues

- Two files with the exact same filename on the user desktop and the Public desktop can't be told apart; DexCorral assumes the one on the user desktop.

See [open issues](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
