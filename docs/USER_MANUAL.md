# DexCorral User Manual

DexCorral organizes your Windows desktop icons into shaded, customizable areas called **Corrals**. Icons stay real desktop icons: DexCorral integrates with Explorer through a shell extension, so drag-and-drop, context menus, and file operations keep working exactly as they always have.

## Installation

DexCorral requires **Windows 11** (build 22000 or newer). Windows 10 is end of life and is neither tested nor supported; both the installer and `DexCorral.exe --register` refuse to run on it.

### Installer (recommended)

1. Download `DexCorral_<version>_Setup.exe` from the [Releases](https://github.com/guHe330/DexCorral/releases) page.
2. Run it and choose how to install (see [Install for all users, or just me](#install-for-all-users-or-just-me) below). (SmartScreen may warn; see [Troubleshooting](#troubleshooting).)
3. The installer registers the shell extension and starts DexCorral inside the running Explorer, with no Explorer restart and no logout needed. A default Corral appears on your desktop immediately.

DexCorral loads automatically at every login. Its shell extension is loaded by Explorer on startup, so there is nothing to configure.

### Install for all users, or just me

Setup's first page asks which one you want. Both give you the same DexCorral; they differ in who gets it and what it takes to install.

- **Install for all users**: goes to `C:\Program Files\DexCorral` and registers in `HKLM`. Needs Administrator rights, so Windows shows a UAC prompt. Every account on the PC gets DexCorral, and an administrator updates it once for everybody. Choose this on a shared or family PC.
- **Install for me only**: goes to `%LOCALAPPDATA%\Programs\DexCorral` and registers in `HKCU`. No Administrator rights and no UAC prompt, which makes it the option on a work PC where you do not have admin. Only your account gets DexCorral, and you update your own copy. Other users on the PC are unaffected.

**The two cannot be installed side by side.** They register the same shell extension, and a per-user install would quietly take precedence over a machine-wide one for your account, leaving you on a different version than everyone else with nothing on screen to say so. Setup checks for the other kind before it writes anything: installing just for yourself over an existing all-users install offers to remove that one first, and installing for all users warns you about any per-user installs it can see (only the owner of one can remove it).

If you are not sure, pick **all users**. It has one extra load path into Explorer, so it recovers on its own if Explorer crashes mid-session, where a per-user install waits until your next login or your next right-click on the desktop.

### Portable package

1. Download `Portable_DexCorral_<version>.zip` and extract it to any folder.
2. Open a command prompt in that folder and run the one-time registration:
   - Just for you: `DexCorral.exe --register --scope=user`, no Administrator rights needed.
   - For all users: `DexCorral.exe --register --scope=machine`, from a command prompt opened **as Administrator**.

   Without `--scope`, DexCorral picks machine scope if the folder is under `Program Files` and user scope otherwise. On an unsupported Windows version this stops with a message instead of registering. Adding `--force` registers anyway, unsupported and untested.
3. Run `DexCorral.exe --startup` to start DexCorral in the current session, or restart Explorer / log out and back in.

Run `DexCorral.exe` with no arguments for the full list of options.

### Uninstalling

Uninstall from **Settings > Apps > Installed apps**, or via the Start Menu group's **Uninstall DexCorral** entry. The uninstaller asks whether to **keep your configuration** (corral layouts and appearance settings). Choose *Yes* if you plan to reinstall later. It then unregisters the shell extension and restarts Explorer to fully unload.

The uninstaller removes only what its own install mode created. Uninstalling a per-user install never touches an all-users install, and uninstalling an all-users install removes the shared registration and files but leaves every account's own settings in `%APPDATA%\DexCorral` alone, including yours if you said *Yes* above.

After uninstalling, icons that were inside Corrals reappear as normal desktop icons; you may want to rearrange them (right-click desktop > **Sort by > Name**).

The uninstaller removes every registry entry DexCorral created. See [What DexCorral writes outside its own folder](#what-dexcorral-writes-outside-its-own-folder) for the full list.

Removed the portable package by deleting its folder? That leaves the same entries behind. Run `DexCorral.exe --unregister` first, with the same `--scope` you registered with (as Administrator for `--scope=machine`); to also clear the per-user settings, delete `HKCU\Software\DexCorral` and `%APPDATA%\DexCorral`.

Upgrading from 1.0.27 or earlier? Those versions registered without any notion of scope. The installer clears the old entries for you; for a portable install, run `DexCorral.exe --cleanup-legacy` once as Administrator.

## Core Concepts

### Corrals
A Corral is a shaded, semi-transparent window holding a group of icons. Icons assigned to it are hidden from the regular desktop and drawn inside the Corral instead.

### Tabs
Each Corral can hold multiple **tabs**, each with its own title, icon list, background color, view mode, and header font: several groups (e.g. "Work", "Games", "Downloads") in one place on the desktop.

**Reordering tabs:** hover a tab to reveal a grip handle ("⠿") on its left edge, then drag it left or right.

**Moving icons between tabs:** drag the icons onto the tab you want them in, in the same Corral or in another one, or right-click an icon and pick the tab from **Move to Tab**. When you drag something in from the desktop or an Explorer window, resting on a tab for half a second brings that tab forward, and dropping straight onto a tab works too.
### Catch-All
One tab can be the **Catch-All**: new files and shortcuts landing on the desktop are captured into it automatically.

### Virtual Corrals
A virtual tab mirrors any folder on your PC. Point it at `Downloads` or a project directory and its files appear inside the Corral, kept in sync as the folder changes. It is a live view of that folder, so dropping files onto it does what dropping them into the folder's Explorer window would: they are moved or copied there, with the same cursor and the same Ctrl / Shift modifiers.

## Managing Corrals

### Creating
- Right-click the DexCorral **tray icon** (or the background of any Corral) and choose **Create New Corral**. DexCorral asks for its name straight away; press Esc to keep "New Corral" and name it later with **Rename Tab**.
- Choose **New Virtual Corral** instead to pick a folder and create a Corral mirroring it.

### Moving and Resizing
- Drag the **title bar** to move a Corral.
- Drag any **edge or corner** to resize. While moving or resizing, Corrals snap to screen edges and align to other Corrals on the same monitor.
- Corrals are immune to **Win+D / Show Desktop**: they stay visible with your desktop.

### Lock Position
Right-click a Corral and choose **Lock Position** to freeze it where it is. A locked Corral cannot be moved or resized with the mouse, and a small padlock appears in its title bar. Everything else keeps working: tabs still switch, double-clicking the title bar still rolls it up, and positions are still restored per monitor and resolution. Choose **Lock Position** again to unlock.

Worth doing for a Corral that sits where you drop files all day. An accidental drag does not just move the Corral, it pushes the desktop icons underneath it out of the way, and there is no undo for that.

### Roll-Up
Double-click the title bar to **roll up** a Corral so only the title bar remains visible. Double-click again to expand it.

A rolled-up Corral also opens by itself while the mouse rests on it, and closes again when the mouse leaves. If one sits where your mouse often passes and keeps popping open, right-click it and uncheck **Expand on Hover**; it then only opens on a double-click.
### Multi-Monitor
Corral positions are remembered **per monitor and per resolution**. Moving a Corral to another monitor or changing display resolution restores the layout you used there last.

### Customizing
Right-click a Corral's title bar or background:

- **Add Tab**: add a new tab to this Corral and name it.
- **Detach Tab**: move the current tab out into its own Corral window (shown when the Corral has more than one tab).
- **Rename Tab**: change the current tab's title.
- **Appearance...**: open the appearance dialog (see below).
- **Change Folder...**: for virtual tabs, point the tab at a different folder.
- **View**: switch between **Small / Medium / Large Icons** and **Details** (a list with name, type, size, modified date, and cloud sync status for OneDrive files).
- **Sort By**: on a normal tab, sorts the icons once by **Name** or **Type**; you can still drag them into any order afterwards, and new icons are added at the end. On a virtual tab it sets the standing sort column and direction, in every view.
- **Catch-All (receives new files)**: make this tab the catch-all for new desktop items.
- **Add Special Icon**: add special shell items such as the Recycle Bin to the Corral.
- **Show Desktop Icons**: toggle visibility of all native desktop icons.
- **Lock Position**: freeze this Corral so it cannot be moved or resized by mouse (see above).
- **Expand on Hover**: whether this Corral, when rolled up, opens under a resting mouse (see [Roll-Up](#roll-up)).
- **Create New Corral / New Virtual Corral**: same as the tray menu.
- **Close Tab / Delete Corral**: remove the current tab, or the whole Corral if it's the last tab. Contained icons return to the desktop.

### Appearance Dialog
**Appearance...** gives live-preview control over:

- **Background color**: the Corral fill.
- **Opacity**: one slider each for the background fill, border, header, header label, icons and icon labels. The border's resize grip stays visible however far down you take it.
- **Header**: title bar height, font face/size, and font color (font settings are per tab).
- **Icons**: tint color and tint strength.
- **Icon spacing**: horizontal and vertical spacing (50–200%).

Three checkboxes at the bottom decide where the style goes when you press OK:

- **Save this style as the default for new corrals**: Corrals you create from now on start with this look.
- **Apply only what I changed to all corrals**: the settings you touched in this session, and only those, are copied to every Corral. Change just the border opacity and every Corral gets that border, keeping its own colours and fonts.
- **Make all corrals look like this one**: every setting in the dialog is copied to every Corral.

**Reset to Defaults** sets every control back to the look a new Corral would get. It previews live like any other change, and Cancel undoes it.

#### Header opacity and tabs
Header opacity fades the title bar and tab strip independently of the Corral fill.

- Inactive tabs are derived from the header opacity automatically, always dimmer than the active tab, no second slider to keep in sync.
- The slider stops short of fully transparent, because the header is the Corral's grab handle. Rolled-up Corrals are kept a little more visible than the setting; unrolling restores it exactly.
- Hovering a Corral fades its header, border, icons and text up to full strength, and back when you leave.

#### Text opacity
The tab title and the icon labels fade on their own sliders, so text can be dialled back without touching what is behind it, or turned off entirely for a Corral that is just icons.

- **Header label** fades the active tab's title. It is a per-tab setting, like the font face and color, even though the slider sits with the per-Corral ones.
- **Icon label** fades every icon caption in the Corral.
- Both go all the way to invisible. Nothing is lost: the header still drags, rolls up and right-clicks with no title on it, and icons stay visible and clickable with no captions.
- Both fade in on hover along with the icons, border and header, on the same timing. Text set to 0% is therefore not gone: it appears when you move the mouse over the Corral and fades back out when you leave.

## Working with Icons

### Adding
- Drag any icon from the desktop and drop it onto a Corral.
- Drag a file or folder from an Explorer window onto a Corral: DexCorral puts a shortcut to it on the desktop and adds that shortcut to the Corral. The original stays where it is.
- If the cursor shows the no-drop sign, the Corral cannot take what you are dragging.
- New desktop items land automatically in the Catch-All tab, if one is set.

### Using
- **Double-click** an icon to open it.
- **Drop a file onto an icon** inside a Corral to invoke its target, e.g. drop a document onto an application's icon to open it with that app.
- **Double-click an icon's label** to rename it in place.
- **Select several icons** with Ctrl+click, Shift+click, Ctrl+A or by dragging a rectangle over them. Dragging one of the selected icons carries all of them: to another Corral, back out to the desktop, or to a new place in the same Corral.
- **Scroll** with the mouse wheel or trackpad when a tab holds more icons than fit; a slim scrollbar appears on hover.

### Keyboard
Click a Corral to give it the keyboard. It then behaves like an Explorer window:

| Keys | Action |
|---|---|
| Arrow keys, Home, End | Move the selection; with Shift, extend it |
| PageUp, PageDown | Scroll one screenful |
| Ctrl+Tab, Ctrl+Shift+Tab (or Ctrl+PgDn, Ctrl+PgUp) | Next / previous tab |
| Typing a name | Jump to the icon that starts with it |
| Enter, Alt+Enter | Open, Properties |
| F2, Delete, Ctrl+C / X / V | Rename, delete, copy / cut / paste |
| Ctrl+A, Esc | Select all, clear the selection |
| F5 | Reload |
| Menu key | Context menu |
| Backspace | Up one folder (virtual tabs) |
### Right-Click Menu
Right-clicking an icon shows the standard Windows context menu (Open, Cut, Copy, Delete, Properties, ...) plus two DexCorral entries: **Move to Tab**, when the Corral has other tabs, and **Remove from Corral**, which returns the icon to the regular desktop without deleting anything. Both act on every selected icon if the one you clicked is part of a selection. Virtual tabs show neither: their icons are the files of a folder.

## Quick-Hide

Double-click an empty spot on the desktop to hide everything at once: all native desktop icons and all corrals (they fade out). Double-click the desktop again to bring everything back exactly as it was.

- Corrals can opt out: right-click a corral and check **Exclude from Quick-Hide** to keep it visible while everything else hides.
- Double-clicking the **tray icon** does the same, and so does the tray menu's **Quick-Hide Everything** entry.
- Quick-hide is temporary: restarting DexCorral always restores your normal desktop.
- For a keyboard shortcut, switch on **Quick-Hide Hotkey** in the tray menu. It is off by default, and its combination is Ctrl+Alt+H. To use another one, set `QuickHideHotkey` in `config.json` to something like `"Ctrl+Shift+F9"` or `"Win+Alt+D"`: at least one of Ctrl, Alt, Shift, Win, plus one letter, digit or function key. If the combination is already taken, DexCorral says so and leaves the hotkey off.
## Tray Icon

Double-clicking the tray icon toggles Quick-Hide. Its right-click menu offers:

- **About**: the version, the license in one sentence, and buttons that open the website, the GitHub project and the full license text.
- **User Manual**: opens this manual on GitHub in your browser.
- **Create New Corral** / **New Virtual Corral**.
- **Show Desktop Icons**: toggle all native desktop icons.
- **Quick-Hide Everything**: hide/show icons and corrals at once (same as double-clicking the desktop).
- **Quick-Hide Hotkey**: switch a global keyboard shortcut for the same toggle on or off (see [Quick-Hide](#quick-hide)).
- **Check for Updates Automatically**: off by default. When on, DexCorral asks the GitHub Releases API once every 24 hours whether a newer version exists and shows a tray balloon if so. It never downloads or installs anything. See [Privacy](PRIVACY.md#the-update-check) for exactly what the request sends.
- **Check for Updates Now**: run that check once, right now, without switching the automatic one on.
- **Language**: switch the interface between English and German. The change takes effect immediately, no restart. The installer asks once at install time; this overrides that, and for the portable package it is the only place the language is set.
- **Open Config Folder**: opens `%APPDATA%\DexCorral` in Explorer, where `config.json`, its backup and any debug logs live.

## Desktop Integration

Because DexCorral hooks Explorer's desktop directly:

- Icons inside Corrals are invisible to desktop hit-testing, rubber-band selection, and keyboard navigation, so you can't accidentally select or disturb them.
- Desktop **Sort by** and auto-arrange never move Corral-owned icons, and visible icons are compacted so sorting leaves no gaps where hidden icons used to be.
- Corral-owned icons are immune to repositioning by Explorer or third-party tools: only DexCorral moves them.

## Configuration

All settings (corral layouts, tabs, colors, fonts, assigned files) are stored in a single JSON file:

```
%APPDATA%\DexCorral\config.json
```

The format is forward- and backward-compatible: fields missing from an older config simply get default values. The file is written automatically; if you edit it by hand, do so while DexCorral is not running.

Two more files can appear next to it:

- `config.bak` is a copy of the last `config.json` that loaded without errors, refreshed each time DexCorral starts.
- `config.corrupt.json` is written when `config.json` cannot be read, for example after a hand edit with a stray comma. DexCorral then loads `config.bak` instead, keeps your unreadable file under this name so you can repair it, and shows a tray notification saying so. With no usable backup it starts with a fresh layout.

Deleting `config.json` still means "start over": the backup is not restored in that case.

### What DexCorral writes outside its own folder

Everything you configure lives in `config.json` above. The registry is used only where Windows requires it: to register the shell extension, to start at login, and for a few settings that must be readable before the config file is.

Which hive these live in follows the install mode you chose. `<root>` below is `HKLM` for an all-users install and `HKCU` for a per-user one. Two keys are all-users only, because Windows reads them from `HKLM` and nowhere else.

| Key | Value | Why | Removed by the uninstaller |
|---|---|---|---|
| `<root>\Software\Microsoft\Windows\CurrentVersion\Run` | `DexCorral` | Starts DexCorral at login. Written by the installer only. | Yes |
| `<root>\Software\DexCorral` | `Language` | The interface language, so it is known before `config.json` exists and by `DexCorral.exe`'s own dialogs, which never read the config. Written by the installer and by the tray menu's **Language** submenu. | Yes |
| `<root>\Software\DexCorral` | `InstallDir`, `InstallScope`, `Version` | Records where and how DexCorral is installed, so Setup can tell you if you are about to install the two modes on top of each other. | Yes |
| `HKCU\Software\DexCorral` | `HookStartPending`, `HookFailureCount` | Safe-mode counters. If Explorer dies three times in a row while DexCorral is loading, the next session starts without the hook rather than repeating the crash. Written from inside Explorer, where the config file is not available, so always per-user. | Yes |
| `<root>\Software\Classes\CLSID\{7A3B9E42-…}` | (several) | Standard COM registration for `DexCorralHook.dll`. | Yes |
| `<root>\Software\Classes\Directory\Background\ShellEx\ContextMenuHandlers\DexCorral` | (default) | The desktop right-click menu entry. | Yes |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler` | `{7A3B9E42-…}` | How Explorer loads the extension at startup. All-users installs only. | Yes |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ DexCorral` | (default) | Second startup load path. No overlay is ever drawn. All-users installs only. | Yes |

The `Software\Classes` and `HKLM` entries are the ones `DexCorral.exe --register` creates and `--unregister` removes. They are the reason an all-users registration needs Administrator rights; a per-user registration writes only to `HKCU` and needs none. Nothing else on the system is touched.

Early versions had a **Hide shortcut arrows** option, removed since. If you switched it on back then and want the arrows back, delete the value `29` under `SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Icons` in `HKLM` or `HKCU`, whichever holds it, and restart Explorer.

Versions up to 1.0.27 wrote the COM registration through `HKEY_CLASSES_ROOT` instead, with no per-user option. `DexCorral.exe --cleanup-legacy`, run once as Administrator, removes what those versions left behind; the installer does it for you.

## Known Limitations

- **Identical filenames on the user and Public desktop**: corral membership is stored as a bare filename, so DexCorral assumes the one on the user desktop. The same *display* name with different filenames (a folder `test` next to `test.txt`) is handled correctly.

## Troubleshooting

- **SmartScreen / antivirus warnings**: the binaries are currently unsigned, so Windows reports the publisher as unknown. Code signing is planned. In the meantime every release publishes SHA-256 checksums and a GitHub build provenance attestation, so you can verify a download rather than taking it on trust: see the release page for both, and `gh attestation verify <file> --repo guHe330/DexCorral` to check the attestation.
- **Corrals don't appear after install**: right-click the desktop once to wake the shell, or log out and back in (the Start-with-Windows entry re-injects DexCorral at login).
- **Desktop icons misbehave**: restarting Explorer resets the hook: `Stop-Process -Name explorer -Force; Start-Process explorer.exe` in PowerShell, or via Task Manager.
- **Bug reports**: please file issues at the [GitHub issue tracker](https://github.com/guHe330/DexCorral/issues).
