# Changelog

All notable changes to DexCorral will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added
- GitHub issue forms in `.github/ISSUE_TEMPLATE/`: **Bug report** (asks for version, Windows build, install type, display setup and an optional `config.json`), **Feature request** (states the project-scope rule up front and asks for the problem before the proposed solution), and **Multi-monitor test report** — a checklist-driven form for testers with two or more screens, since multi-monitor placement, per-resolution scaling and monitor unplug/replug handling have never been run on a real multi-display setup. `config.yml` keeps blank issues enabled and links the User Manual and contributing guide
- `docs/CONTRIBUTING.md` gains a **Testing is a contribution** section naming the untested territory (multiple monitors, mixed DPI, unusual display hardware, large desktops) and pointing at the multi-monitor form; the bug-reporting section now points at the template chooser and explains what `config.json` contains before asking anyone to attach it

### Changed
- The portable download is now named `Portable_DexCorral_<version>.zip` instead of `Portable_DexCorral.zip`, so packages from different releases stay distinguishable once downloaded (`.github/workflows/build-release.yml`). `build.ps1` names its local release archive `DexCorral_<version>.zip` for the same reason and deletes stale zips from `DexCorral/build/` first
- DexCorral now targets Windows 11 only. Windows 10 is end of life and the maintainer has no way to test on it, so every reference to it is gone from the docs, the portable readme and the release notes, and the Inno Setup installer's `MinVersion` moves from `10.0.17763` (Windows 10 1809) to `10.0.22000` (Windows 11 21H2) — the installer now refuses to run on Windows 10 instead of installing a build nobody tests there. The portable package is covered too: `DexCorral.exe --register` reads the real build number via `RtlGetVersion` (`GetVersionEx` and the `VersionHelpers` macros are manifest-capped and lie) and refuses below build 22000. A new `--force` flag overrides the check for anyone who wants to try regardless — `--unregister` and `--startup` are deliberately never gated, so a forced install stays removable and startable, and a failed version probe is treated as supported rather than blocking. The Windows 11 requirement and the `--force` escape hatch are documented in `README.md`, `docs/USER_MANUAL.md`, `docs/BUILD_GUIDE.md`, `docs/CONTRIBUTING.md`, `Distribute/PortableReadme.txt` and the release template; the bug-report form and contributing guide state that Windows 10 reports are out of scope
- The download-size column in the GitHub Release notes is now filled in from the artifacts the workflow just built, via `${SETUP_SIZE}` / `${PORTABLE_SIZE}` placeholders in `Distribute/RELEASE_TEMPLATE.md`, instead of a hand-maintained `~750KB` that had drifted from reality (the installer is ~2.3 MB). The stale ~750KB binary-size claims in `README.md` and `Distribute/PortableReadme.txt` are corrected to ~1 MB

## [1.0.22] - 2026-08-27

### Added
- Header opacity is now a per-corral setting (`CorralWindowConfig::HeaderOpacity`, default 240 — the value the active tab header was previously hard-coded to). New slider in the Appearance dialog's Header group (ID 124/125), range 20–255. The floor exists because the header is a corral's only grab handle (drag, double-click to roll up, right-click menu): at 0, a corral with a transparent background would be an invisible window that still swallows mouse input. `Config::Normalize` clamps the value on load so a hand-edited `config.json` cannot produce one either.
- Border opacity is now a per-corral setting (`CorralWindowConfig::BorderOpacity`, default 255). New slider in the Appearance dialog's Opacity group (ID 122/123), range 0–255; at 0 the corral is a frameless shape on the wallpaper. `CORRAL_BORDER_COLOR` is replaced by `CORRAL_BORDER_R/G/B` plus the configured alpha, and the border is now composited over the corral fill (`ChromeAlpha::PremultipliedOver`) instead of overwriting it, so a partly transparent border blends instead of punching a hole
- Inactive tab appearance is **derived** from the header opacity rather than configured: `inactiveAlpha = max(INACTIVE_TAB_ALPHA_MIN, headerAlpha * INACTIVE_TAB_ALPHA_FACTOR)`, clamped to never exceed the header alpha, plus a colour darkening that deepens as the header fades (50% of the tab colour at full opacity down to 25% at the floor, where the alpha gap alone is imperceptible). New pure-arithmetic header `ChromeAlpha.h` with the derivation, clamps and pixel helpers; unit-tested in `tests/test_chrome_alpha.cpp` (including exhaustive "inactive is never brighter than active" and monotonicity sweeps across all 256 alpha values)
- Hovering a corral now fades its header and border to full alongside the icons, and back to the configured values on leave — a near-invisible corral presents a solid grab handle and frame the moment you reach for it
- `AppConfig::DefaultHeaderOpacity` / `DefaultBorderOpacity`; both new settings are covered by "Use as default for new corrals", "Apply changes to all corrals" and "Copy full style to all corrals" (`App::SetDefaultAppearance`, `App::ApplyAppearanceToAllCorrals`)

### Changed
- The hover fade is no longer symmetric: fading in stays at 200 ms (`OPACITY_FADE_IN_DURATION`, the previous `OPACITY_ANIMATION_DURATION`), fading out is now 400 ms (`OPACITY_FADE_OUT_DURATION`). At 200 ms out, crossing between adjacent corrals made the chrome flash out and back in; the slower exit absorbs that. This applies to the icon fade as well — icons and chrome share one timer and one curve, and running them on different clocks would look broken
- While a corral is rolled up, the header alpha is floored at `ROLLED_UP_HEADER_ALPHA_MIN` (60): rolled up, the header is the entire corral, and a ~26px strip at the 20 floor is too thin a target to hunt for. This is a render-time clamp only — the configured value is never rewritten, so unrolling restores it exactly and the slider keeps showing what the user chose
- The Appearance dialog's **Opacity** group now holds every opacity setting as labelled rows — background, border, header and icons. "Header" keeps height, font and colour; "Icons" keeps the tint. Dialog height 308 → 337 dialog units; control IDs, ranges and handlers are unchanged.
- Unit test target additionally builds `src/Config.cpp` so `Config::Normalize` is covered

### Fixed
- Clicking a tab often did nothing. `OnLeftButtonDown` tested the resize border before tabs, and `HitTestResize` treats the top of the client area as a resize edge — but the title bar starts at y=0, so the top `RESIZE_BORDER` (6) logical px of every tab started an invisible resize instead of activating the tab. On a typical 26–28px header that was the top ~21–23% of the tab strip. New `HitTestResizeAllowingTabs` yields a plain `HTTOP` hit wherever a tab actually is, and is used by both the click path and `WM_SETCURSOR` so the cursor no longer advertises a resize where a click will switch tabs. Corners (`HTTOPLEFT`/`HTTOPRIGHT`) are untouched and still resize from the top.
- A tab's reorder grip could be grabbed where no grip was drawn: the grip is only rendered on the hovered (or dragged) tab, but `HitTestTabGrip` tested the left `TAB_GRIP_WIDTH` px of *every* tab whenever a corral had 2+ tabs, so a click near any tab's left edge silently entered reorder mode. The hit test is now limited to the tab showing the grip.
- A corral could get stuck in resize (or drag) mode: after releasing the mouse button, moving the mouse kept resizing the corral. Corrals are background windows (`MA_NOACTIVATE`, pinned to `HWND_BOTTOM`, owned by Progman), and Win32 only delivers captured mouse messages to a background window while the cursor is over it — so a button released outside the corral never reached `OnLeftButtonUp` and `isResizing` stayed set forever. Two recoveries, both routed through `EndCapturedOperationWithoutDrop`: `WM_MOUSEMOVE` ends a captured operation once the message flags and the live button state agree that the button is up (so a stuck corral heals on the very next mouse move), and a new `WM_CAPTURECHANGED` handler ends it when the capture is taken away with the button already released — relevant because DexCorral's UI runs inside Explorer's process, where another component can grab the capture at any time. Both checks require the live button state deliberately: Windows hands background windows a weaker capture it can cancel mid-drag, and ending there would abort a resize the user is still performing. Geometry is committed (`EndResize` / `SyncConfigFromWindow`, so the released size and position are saved) but the position-dependent side effects of a real drop are skipped: no merge into another corral, no icon dropped on a guessed target, since the release point is unknown by then. Applies to window drag, corral resize, details-column resize, scrollbar drag, icon drag and tab reorder.
- Docs: every GitHub link pointed at `guHe330/DexCorralCpp`, a repository that does not exist — the project is hosted at `guHe330/DexCorral`. Corrected in `README.md`, `docs/USER_MANUAL.md`, `Distribute/PortableReadme.txt`, `Distribute/RELEASE_TEMPLATE.md` (used to build the GitHub Release description) and `installer/innosetup/DexCorral.iss` (`MyAppURL`, the support link shown in Add/Remove Programs).

## [1.0.21] - 2026-08-22

### Added
- Application icon: `DexCorral.exe`, `DexCorralHook.dll`, the tray icon, and corral windows all use the DexCorral icon instead of Windows' generic default. The Inno Setup installer shows it as the wizard's small branding image and as the uninstall entry's display icon (the `Setup.exe`/`Uninstall.exe` file icon itself is left as Inno Setup's default).

### Changed
- Desktop file rename and deletion notifications, plus native desktop context-menu corral creation commands, are now marshalled to DexCorral's app thread before updating corrals or creating windows.

### Fixed
- Stale virtual-corral entries whose backing files have disappeared are pruned from the corral and saved to configuration instead of rendering as blank ghost icons.
- "Remove from Corral" remains available when a stale entry can no longer be resolved by the shell context-menu APIs.
- Fallback display-name handling now strips `.url` extensions as well as `.lnk` extensions when the shell cannot resolve an item.
- Corrals no longer pop back above other applications when a different window is activated, or when clicking an icon inside a corral. `SendToBottom()`'s `HWND_BOTTOM` placement was only ever reasserted at specific moments (creation, show, drag-end, menu-close): a new `EVENT_SYSTEM_FOREGROUND` listener (`App::WinEventProc`) now re-pins every corral to the bottom whenever another window becomes foreground, and icon selection's `SetFocus(hwnd)` call (needed for F2 rename) is immediately followed by `SendToBottom()` since `SetFocus` activates the window as a side effect that `WM_MOUSEACTIVATE`'s `MA_NOACTIVATE` doesn't cover.
- Icon-drag drop-target hit-testing accounted for the corral's current scroll offset incorrectly, so dragging an icon over a scrolled corral could highlight the wrong slot.

## [1.0.20] - 2026-06-23

### Added
- Tabs can be reordered by dragging a left-edge grip handle ("Griff"): hovering a tab reveals a 2×3 dot grip on its left edge (only when the corral has more than one tab); pressing and dragging it reorders the tab live, with the cursor crossing tab midpoints to choose the target slot. The drag uses an `IDC_SIZEALL` cursor, activates the grabbed tab, and persists the new order on release (`SaveConfig`). `ActiveTabIndex` is kept pointing at the same logical tab across the reorder. New `CorralWindow` members/helpers: `GetTabGripRect`, `HitTestTabGrip`, `MoveTab`, `OnTabDrag`, and the `hoveredTab`/`isDraggingTab`/`draggedTabIndex`/`tabDragStart` state plus `TAB_GRIP_WIDTH`

## [1.0.19] - 2026-06-23

### Added
- Virtual corrals are now browsable like an Explorer pane (Details view): double-clicking a sub-folder navigates into it inline, and a "folder up" button appears at the left of the title bar to go back (hidden at the root; never navigates above the linked folder). The current sub-path is remembered across restarts (`CorralTabConfig::CurrentSubPath`). New `CorralWindow` helpers: `GetVirtualCurrentPath`, `NavigateToSubfolder`, `NavigateUp`, `IsNavBackVisible`, `GetNavBackButtonRect`
- Details view gains a real **column header row** (Name / Type / Size / Date modified) for virtual corrals. Click a header to sort by that column; click again to flip direction (a ▲/▼ glyph marks the active column). Folders always sort before files. A "Sort By" context submenu mirrors the header. Sort column/direction persist (`DetailsSortColumn`, `DetailsSortAscending`); single source of truth for column geometry is `CorralWindow::GetDetailsColumns`
- Details columns are **resizable** by dragging the header separators (E-W cursor on hover, min width clamp). Per-tab widths persist (`DetailsColumnWidths`)
- Virtual corrals degrade gracefully when the linked folder or a navigated sub-folder is renamed/deleted/moved: the view auto-navigates up to the nearest existing ancestor, or shows a "Folder unavailable — right-click to relink" message if the root is gone (relink via the existing "Change Folder..." menu). A second `FolderWatcher` on the parent directory detects rename/delete of the folder currently being viewed
- Opt-in update check (**off by default**): when enabled, DexCorral queries the GitHub Releases API (`/repos/guHe330/DexCorral/releases/latest`) at most once per 24h on startup and shows a tray balloon if a newer version exists; clicking the balloon opens the release page (`ShellExecute`, no auto-download). New `UpdateChecker` module runs the HTTPS request on a detached worker thread (WinHTTP) and posts the result back to the app message window (`WM_APP+103`); version comparison is numeric on `{major,minor,patch}`. Tray menu gains "Check for Updates Automatically" (toggles `AppConfig::CheckForUpdates`) and "Check for Updates Now" (manual check that also reports "up to date"/"couldn't check"). New config: `CheckForUpdates`, `LastUpdateCheckEpoch`
- Quick-hide: double-clicking an empty desktop spot hides/shows everything at once — native icons (`DesktopIcons::SetIconsVisible`) plus all corral windows with a 180 ms whole-window fade (`SourceConstantAlpha` animation). Double-click detection is done manually from the `WH_MOUSE_LL` hook (cheap time/rect pairing in the callback, empty-desktop validation deferred to the app thread via `WM_APP+102`); clicks only count when they land on Explorer's desktop hierarchy and hit no icon (`App::IsPointOnEmptyDesktop`). Per-corral "Exclude from Quick-Hide" context menu flag (`CorralWindowConfig::ExcludeFromQuickHide`, applied live when toggled mid-quick-hide), tray menu "Quick-Hide Everything" entry, and `SaveConfig` persists the pre-quick-hide icon visibility so the transient state never sticks across restarts
- Hook-owned desktop sorting (Fences-style): "Sort by" commands are executed by the hook itself with shell-PIDL sort keys (name/size/type/date); Explorer never repositions icons
- Hidden-icon move immunity: position writes to corral-owned icons are swallowed unless they come from DexCorral itself (`HookBridge::BeginAppIconMove`/`EndAppIconMove`)
- Hidden icon entries now carry a canonical parsing name (full path or `::{CLSID}`) alongside the display name (`HiddenIconInfo`), groundwork for PIDL-based identity
- Desktop file add/remove re-runs compaction when DexCorral's auto-arrange is on
- Crash containment: SEH guards around every Explorer-called hook entry point — both subclass procs, all timer callbacks, and the `IDropTarget` wrapper. An escaped exception (including C++ exceptions, which MSVC layers on SEH) makes the hook go inert for the session: hidden icons are released, every entry point becomes a pass-through, and nothing propagates into Explorer
- Safe mode: a registry crash sentinel (`HKCU\Software\DexCorral`: `HookStartPending`/`HookFailureCount`) is armed before subclassing and cleared after a 60 s stability window or clean shutdown. Three consecutive sessions dying with the sentinel armed start DexCorral with the hook disabled and a tray balloon ("started in safe mode"); counters reset so the following session retries normally. New API: `IsCorralHookSafeMode()`, `TrayIcon::ShowBalloon`
- Desktop context menu command IDs (auto-arrange 28785, align-to-grid 28788, sort 31492–31495) are re-resolved at `WM_INITMENUPOPUP` by scanning menu captions, with the numeric IDs as fallback — resilient to ID changes across Windows builds (English shell UI; localized systems keep the fallbacks)

### Changed
- Detaching a tab now places the new corral in free space next to the one it came from instead of overlapping it: `App::FindNearestFreeCorralPosition` finds the closest non-overlapping center to a desired top-left within the monitor work area, ignoring the source window (passed as `exclude`)
- Release CI: pinned the build job to the `windows-2022` runner (the `windows-latest` image rolled forward to one with VS 2026 and no VS 2022, breaking the `Visual Studio 17 2022` CMake generator) and bumped `actions/checkout` to v5
- The catch-all corral can now be disabled entirely: toggling "Catch-All" on a tab that is already catch-all now turns it off (previously it could not be unset). At most one corral can still be catch-all at a time, but having none is now allowed — new desktop files simply aren't auto-collected until a catch-all is enabled again. Startup no longer force-assigns a catch-all when none exists
- New corrals now open in free space instead of overlapping existing ones: `App::FindFreeCorralPosition` tiles a 300×200 corral from the top-right corner of the primary monitor's work area (columns right-to-left, rows top-to-bottom, 16 px margins) and returns the first non-overlapping center; falls back to a top-right cascade when no free tile remains. Used by the desktop context menu ("New DexCorral"/"New Virtual DexCorral", which previously placed at the cursor) and the tray menu (previously screen-center + cascade)
- New corrals now default to 100% horizontal and vertical icon spacing (`DefaultIconSpacingXPercent`/`DefaultIconSpacingYPercent`, previously 91%/85%)
- `DexCorral/include/Version.h` is now the **single source of truth** for the version (bumped to 1.0.17): `CMakeLists.txt` parses it for the project version, both `.rc` files `#include` it for their FILEVERSION/ProductVersion fields, and the release workflow now *verifies* the pushed git tag matches `DEXCORRAL_VERSION` instead of patching CMakeLists/Version.h/.rc from the tag. Bump Version.h in the release commit; the tag must match or CI fails
- Docs: `USER_MANUAL.md` rewritten for the current architecture — tabs, virtual corrals, appearance dialog, installer/uninstaller flow, correct config path (`%APPDATA%\DexCorral`); `uninstall-guide.md` updated from MSIX to the Inno Setup installer; README feature list and install steps refreshed; `BUILD_GUIDE.md` updated (Inno Setup prerequisite, installer output path, run-your-build section); `ARCHITECTURE.md` removed (was stale)
- `LVS_AUTOARRANGE` is never re-enabled, not even temporarily during sort commands — eliminates the window where Explorer's internal reflow scattered hidden icons and left grid gaps
- `CompactVisibleIcons` batches repositioning into a single repaint via `WM_SETREDRAW`
- DexCorral's auto-arrange now also works with no hidden icons (the hook is the desktop layout engine)
- Hidden-icon matching is now PIDL-based: items are identified by their canonical shell parsing name with display-name fallback — display-name collisions can no longer hide the wrong icon. PIDLs come from the desktop's `IFolderView` (ShellWindows → top-level browser → active shell view), since the desktop ListView is owner-data on modern Windows and the legacy lParam-as-PIDL trick returns 0. Fallback verdicts are never memoized, and view acquisition is rate-limited
- Per-index hidden memo replaces the per-paint text fetch + linear list scan in all hook hot paths (custom draw, hit testing, hover, drag-drop, input filtering); invalidated on insert/delete/rename/sort and hidden-list updates
- `LogDT` (drag-drop debug log) is now gated behind the `DebugLogging` config flag like `Log()` — it used to open/write/close a file on every `DragOver` tick (mouse-move frequency during any desktop drag), always on
- **All** log files are now gated behind `DebugLogging` — including `dllmain.log` (`DllLog`, previously always-on by design) and `CorralDrop.log` (`LogCorralDrop`, previously ungated). With the flag off (the default), DexCorral writes no log files at all. To keep startup logging usable, `HookBridge::IsDebugLogging` bootstraps the flag with a lightweight token scan of `config.json` on first use — the first `DllLog` calls happen during DLL injection, before the App loads the config and calls `SetDebugLogging`
- Unhook safety: `CleanupCorralHook` only restores the original WNDPROCs if the current proc is still ours; if another tool subclassed after us, our subclass stays installed but inert instead of breaking the other tool's chain

### Fixed
- Hidden (corral-owned) icons could become invisible drag-drop targets — most visibly below a rolled-up corral, where the whole tab is parked on exposed desktop. Root cause: the hook's `IDropTarget` wrapper (which rejects drops on hidden icons) was installed once at hook init with a 15-second retry window, but Explorer registers the desktop drop target lazily — often only when the first drag starts — so the wrapper frequently never entered OLE's dispatch chain. The retry timer is now a lifetime maintenance timer: it polls fast for 15 s, then slowly forever until installed, and afterwards re-verifies every 5 s that the wrapper is still the registered target — if something re-registered behind us, the new target is re-wrapped (capped at 8 re-wraps to avoid tug-of-wars; every transition is logged via `DllLog`). Also: a third install candidate (DefView's parent, for builds that register on Progman/WorkerW), and cleanup no longer blindly restores the original drop target over a foreign registration (same policy as the WNDPROC restore)
- Auto-arrange/sort no longer leaves empty grid slots where hidden (corral-owned) icons used to be
- Two desktop items with the same display name (e.g. folder `Project` and file `Project.txt` with hidden extensions, or same-named items on the user vs Public desktop) no longer both get hidden when only one is in a corral
- Corral icon labels now respect Explorer's "Hide extensions for known file types" setting: display names come from the shell (`DesktopIcons::GetShellDisplayName`) in corral rendering, hidden-icon matching, icon position sync, and the push-out-of-way cache — toggling the setting no longer makes a corral-owned icon reappear on the desktop ("duplicate" icon)
- In-corral rename re-appends the hidden extension like Explorer does (generalized from the previous `.lnk`-only handling), so renaming with extensions hidden can't change the file type
- New files created via the desktop "New" context menu no longer get yanked into the catch-all corral mid-rename: adoption is deferred until Explorer's inline rename edit box closes (the queue follows renames and drops deleted files)
- Icon positioning is now identity-based too: parking hidden icons under corrals, drag-out-of-corral placement, and push-out-of-way all go through `DesktopIcons::PositionIconsByPath` → the hook positions items by parsing name on the Explorer UI thread (`DexCorral_PositionIconsByPath` registered message). Previously positioning matched ListView text, so with hidden extensions a name-twin (folder `test` vs file `test.txt`) was parked behind the corral along with the real member, and dragging one out moved both. Display-name requests (push-out) now match visible icons only
- Push-out-of-way is identity-based end to end: the icon cache comes from a hook snapshot service (`DexCorral_GetIconSnapshot` registered message → `DesktopIcons::GetAllIconsWithIdentity`) carrying parsing names and positions, stored as a vector so same-named icons no longer collapse into one cache entry; the corral-ownership filter (formerly `IsIconHiddenByCorral`) compares parsing names, so a free name-twin of a corral-owned icon is pushed correctly instead of being skipped
- Renaming a corral-owned file no longer makes it reappear on the desktop: both the in-corral rename and desktop-side renames now refresh the hook's hidden list and re-park the icon (its identity — path and display name — changed)
- In-corral rename updates the config entry by matching the old value instead of by index (icons and Files indices can drift when an entry fails to load), and keeps the icon's internal UTF-8 name in sync
- Catch-all adoption skips hidden/system files (`desktop.ini` etc.) — Explorer doesn't show them on the desktop, so adopting them put a ghost icon (displayed as "desktop") into the corral; adopted files are now also hidden/parked immediately instead of waiting for the next desktop event
- Defensive guards against empty file names throughout (adoption queue, corral `AddFile`, icon loading, identity collection, desktop path resolution) — an empty entry used to resolve to the desktop folder itself and render as a ghost "Desktop" icon
- Renames are now flicker-free: the old identity stays hidden as a 5-second transition alias (`App::AddTransientHiddenIcon`) alongside the new one, so there is no frame where the desktop item — which the shell updates asynchronously — matches neither identity and pops onto the desktop. Applies to both in-corral and desktop-side renames. (Note: PIDLs don't make renames stable — a filesystem item's PIDL encodes its name, so a rename changes the PIDL just like the path.)
- Renaming a corral-owned file no longer leaves a permanent visible duplicate on the desktop. Root cause: the desktop ListView is owner-data, so the defview updates items in place (renames) with no interceptable message, and the per-index memo kept a stale "not hidden" verdict computed against the pre-rename item. The hook now runs a deferred revalidation (600 ms + 2 s passes) after every hidden-list change — dropping the memo, repainting, and asking the app to re-park with fresh identities — and intercepts `LVM_SETITEMCOUNT` (the add/remove path owner-data views actually use; the classic insert/delete messages never fire on modern Windows). `HookBridge::UpdateHiddenIcons` bumps the version only when the list really changed, so the revalidation round trip can't loop and unchanged reparks no longer force full desktop repaints

### Removed
- "Start with Windows" tray menu toggle and its `App::IsAutostartEnabled`/`App::SetAutostart` helpers. The toggle was redundant and misleading: DexCorral's shell extension is registered as an icon-overlay handler, so Explorer loads the hook DLL and starts the app at every login regardless of the toggle (the installer's `--startup` Run key remains as the deterministic fast path). Disabling it never actually stopped DexCorral
- Dead code: `DesktopIcons::HideIcon`, `DesktopIcons::GetIconPosition`, the orphaned `DesktopFilter.h` (declared-only, never implemented or referenced), and the unused `ICON_HIDE_POSITION_X/Y` constants

### Known Limitations
- Corral membership is stored as a bare filename, so two files with the exact same filename on the user and Public desktop cannot be told apart (the user-desktop one wins). Documented in `USER_MANUAL.md`; fixing it requires storing paths in the config with a migration

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
