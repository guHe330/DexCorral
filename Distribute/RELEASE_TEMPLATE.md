## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ${SETUP_SIZE} |
| `Portable_DexCorral_${VERSION}.zip` | Portable package (no installer) | ${PORTABLE_SIZE} |
| `DexCorral_${VERSION}_sbom.cdx.json` | Software bill of materials (CycloneDX) | ${SBOM_SIZE} |

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

- DexCorral now requires Windows 11 (build 22000 or newer). Windows 10 has reached end of life and there is no machine here to test it on, so the installer refuses to run below that build. Nothing changes if you are already on Windows 11
- A Corral can be locked in place: right-click it and choose **Lock Position**. It can no longer be moved or resized with the mouse, and a small padlock in the title bar shows why. Tabs still switch, double-click still rolls it up, and it still finds its saved place on each monitor. Worth doing for the Corral that sits where you drop files — an accidental drag also pushes the desktop icons under it out of the way, and that cannot be undone
- Corrals now respond to the keyboard like Explorer does. Arrow keys move between icons, Home/End jump to the first/last, Esc clears the selection, and typing a few letters jumps to the icon whose name starts with them
- Delete, Enter, Alt+Enter and Ctrl+C/X/V now work on Corral icons. They run the shell's own commands, so you get the usual confirmation, the Recycle Bin, and Ctrl+Z undo in Explorer. Shift+Delete still deletes permanently
- Several icons can be selected at once: Ctrl+click to add or remove one, Shift+click for a range, Ctrl+A for all, Shift+arrows to extend, or drag a selection rectangle over empty space in the Corral
- F5 reloads a Corral's contents, the Menu key opens the selected icon's context menu, and Backspace goes up one folder in a folder Corral
- DexCorral speaks German. Pick the language from the tray icon's menu under **Language** and the whole interface switches straight away — no restart, nothing to edit. The installer still asks at install time; the tray menu overrides that choice afterwards, and is how portable users set it at all
- New Corrals appear where you ask for them. Right-click the desktop and the Corral turns up where you clicked; ask for one from a Corral's own menu and it lands beside it; from the tray it follows the mouse. In every case it slides to the nearest spot that doesn't cover another Corral — and if the desktop is too crowded for that to be close by, it simply appears where you asked rather than jumping across the screen
- New Corrals are the right size on high-DPI screens. The starting size was fixed pixels, so on a 4K display at 150% a new Corral came out a third too small for everything drawn inside it. It now scales with the monitor it appears on
- A new Corral is no longer created underneath the ones already on your desktop, and the Corral you are using stays on top of its neighbours. Clicking an icon in a Corral that overlaps another one now acts on the Corral you clicked, not the one that happened to be created first
- Rolled-up Corrals wait a moment before springing open on hover, and only one opens at a time — so brushing past a stack of them on the way somewhere else no longer opens each in turn
- Two new sliders control text opacity: **Header label** fades a tab's title, **Icon label** fades the captions under the icons. Both go all the way to invisible, and both fade back in when you point at the Corral — so text set to 0% is hidden at rest rather than lost. Every opacity slider now sits together in the Appearance dialog's Opacity section, each one directly under what it affects
- Fixed: resizing a Corral often stopped half-way. Drag an edge too quickly, or let it snap to a neighbour, and the pointer slipped outside the Corral — which is where Windows stops telling a background window about the mouse. Resizing and moving now follow the pointer wherever it goes
- Fixed: resizing a Corral diagonally was practically impossible. The corner grab areas were smaller than the grip drawn in the bottom-right corner; they now match it, and the top corners are twice as wide as before
- Fixed: a black header font was invisible. Header text now renders correctly over any wallpaper, at the cost of being very slightly softer
- Upgrading over an existing install is reliable. The occasional "cannot replace DexCorralHook.dll" failure is gone: the installer now moves the old extension aside before copying and restarts Explorer afterwards, instead of racing a freshly started Explorer for the file
- Security: the update check now verifies that the release address it got from GitHub really points at the DexCorral repository before opening it, and falls back to the releases page if it doesn't
- Security: the release build pins every GitHub Action to an exact revision, and a new CI check keeps these safeguards from being undone by a later change
- Security: every release now ships a bill of materials (`DexCorral_${VERSION}_sbom.cdx.json`) listing every component DexCorral is built from, each with a checksum the build verifies, plus a weekly check for new versions of those components
- The bundled JSON library (nlohmann/json) is updated to 3.12.0, with the bill of materials and its checksum updated to match

<!-- Older changes (v1.0.22) -->

- The Corral header (title bar and tabs) now has its own opacity slider in the Appearance dialog, separate from the background. Fade a Corral into your wallpaper and the header fades with it, instead of staying solid on top of an invisible Corral
- Inactive tabs adjust themselves: whatever header opacity you pick, they stay dimmer and darker than the active tab, so you can always tell which tab you're on. Nothing extra to set
- The header slider stops just short of invisible on purpose — the header is what you grab to move a Corral, roll it up, or open its menu, so a faint edge always remains. A rolled-up Corral is kept a little more visible still, and unrolling brings back exactly the setting you chose
- The Corral border has its own opacity slider too, all the way down to none — a Corral can now sit on the desktop as a completely frameless shape
- Hovering a Corral now brings its header and border back to full strength along with the icons, so a faded Corral is fully readable while you work in it and settles back when you move away. The fade-out is a little slower than before, so moving between two Corrals no longer makes them flash
- Every opacity setting now sits together in one place: the Appearance dialog's Opacity group holds background, border, header and icon opacity as one set of sliders
- The User Manual now lists everything DexCorral puts in the registry, why each entry is there, and what the uninstaller takes back out. Uninstalling also cleans up properly now — a settings key used to be left behind, and the portable instructions stopped at "delete the folder" without mentioning what that leaves registered
- Fixed: clicking a tab near its top edge did nothing. The top few pixels of every tab were reserved for resizing the Corral, so clicks that landed there were swallowed instead of switching tabs. The whole tab now responds, and the corners still resize
- Fixed: a tab's reorder grip could be grabbed where no grip was shown, so a click near the left edge of a tab could start moving it unintentionally. Only the tab actually showing its grip can be dragged by it
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
