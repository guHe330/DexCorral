## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ${SETUP_SIZE} |
| `Portable_DexCorral_${VERSION}.zip` | Portable package (no installer) | ${PORTABLE_SIZE} |

> **Requires Windows 11** (build 22000 or newer). Windows 10 is end of life and is not supported.

> **Note:** Both packages are currently unsigned. Windows SmartScreen may show a warning on first run. Click "More info" then "Run anyway" to proceed.

### Installation Options

#### Option 1: Installer (Recommended)
**For Windows 11 users**

1. Download `DexCorral_${VERSION}_Setup.exe`
2. Run the installer (requires Administrator)
3. The installer registers the shell extension and restarts Explorer automatically

#### Option 2: Portable Package
**For advanced users or non-administrator installs**

1. Download `Portable_DexCorral_${VERSION}.zip`
2. Extract to any folder
3. Run `DexCorral.exe --register` (as Administrator)
4. Restart Explorer or log out/in

**Package Contents:**
- `DexCorral.exe` - Registration tool
- `DexCorralHook.dll` - Explorer shell extension
- `LICENSE` - GPL-3.0 License
- `readme.txt` - Quick start guide

### What's New

- DexCorral speaks German. The installer asks for your language and the whole interface follows it; portable users can set `"Language": "de"` in `%APPDATA%\DexCorral\config.json`
- The Corral header (title bar and tabs) now has its own opacity slider in the Appearance dialog, separate from the background. Fade a Corral into your wallpaper and the header fades with it, instead of staying solid on top of an invisible Corral
- Inactive tabs adjust themselves: whatever header opacity you pick, they stay dimmer and darker than the active tab, so you can always tell which tab you're on. Nothing extra to set
- The header slider stops just short of invisible on purpose — the header is what you grab to move a Corral, roll it up, or open its menu, so a faint edge always remains. A rolled-up Corral is kept a little more visible still, and unrolling brings back exactly the setting you chose
- The Corral border has its own opacity slider too, all the way down to none — a Corral can now sit on the desktop as a completely frameless shape
- Hovering a Corral now brings its header and border back to full strength along with the icons, so a faded Corral is fully readable while you work in it and settles back when you move away. The fade-out is a little slower than before, so moving between two Corrals no longer makes them flash
- Every opacity setting now sits together in one place: the Appearance dialog's Opacity group holds background, border, header and icon opacity as one set of sliders
- Fixed: clicking a tab near its top edge did nothing. The top few pixels of every tab were reserved for resizing the Corral, so clicks that landed there were swallowed instead of switching tabs. The whole tab now responds, and the corners still resize
- Fixed: a tab's reorder grip could be grabbed where no grip was shown, so a click near the left edge of a tab could start moving it unintentionally. Only the tab actually showing its grip can be dragged by it
- Fixed: resizing a Corral often stopped half-way. Drag an edge too quickly, or let it snap to a neighbour, and the pointer slipped outside the Corral — which is where Windows stops telling a background window about the mouse. Resizing and moving now follow the pointer wherever it goes
- Fixed: resizing a Corral diagonally was practically impossible. The corner grab areas were smaller than the grip drawn in the bottom-right corner; they now match it, and the top corners are twice as wide as before
- Security: the update check now verifies that the release address it got from GitHub really points at the DexCorral repository before opening it, and falls back to the releases page if it doesn't
- Security: the release build pins every GitHub Action to an exact revision, and a new CI check keeps these safeguards from being undone by a later change
- Fixed: a Corral could stay in resize mode after you let go of the mouse button — moving the mouse afterwards kept resizing it. Releasing the button now always ends the resize, wherever the pointer happens to be, and the size you released at is saved

<!-- Older changes (v1.0.21) -->

- DexCorral has its own icon now — on the application, the tray, the Corral windows, and in the installer and Add/Remove Programs, instead of Windows' generic default
- Fixed: Corrals no longer jump in front of other applications when you switch windows or click an icon inside a Corral
- Fixed: files that had disappeared from a virtual Corral's folder left blank placeholder icons behind; they are now cleared out automatically
- Fixed: "Remove from Corral" stayed available for entries whose file can no longer be found, so a stuck icon can always be cleared
- Fixed: internet shortcuts showed their .url extension in the Corral when Windows could not resolve the item
- Fixed: dragging an icon over a scrolled Corral highlighted the wrong drop position

<!-- Older changes (v1.0.20) -->

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

See [open issues](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
