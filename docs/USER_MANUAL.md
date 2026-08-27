# DexCorral User Manual

DexCorral organizes your Windows desktop icons into shaded, customizable areas called **Corrals**. Icons stay real desktop icons — DexCorral integrates with Explorer through a shell extension, so drag-and-drop, context menus, and file operations keep working exactly as they always have.

## Installation

DexCorral requires **Windows 11** (build 22000 or newer). Windows 10 is end of life and is neither tested nor supported; both the installer and `DexCorral.exe --register` refuse to run on it.

### Installer (recommended)

1. Download `DexCorral_<version>_Setup.exe` from the [Releases](https://github.com/guHe330/DexCorral/releases) page.
2. Run it (Administrator rights are required for shell extension registration). Because the binaries are currently unsigned, Windows SmartScreen may warn on first run — click **More info**, then **Run anyway**.
3. The installer registers the shell extension and starts DexCorral inside the running Explorer — no Explorer restart and no logout needed. A default Corral appears on your desktop immediately.

DexCorral loads automatically at every login — its shell extension is loaded by Explorer on startup, so there is nothing to configure.

### Portable package

1. Download `Portable_DexCorral_<version>.zip` and extract it to any folder.
2. Open a command prompt **as Administrator** in that folder and run `DexCorral.exe --register` (one-time setup). On an unsupported Windows version this stops with a message instead of registering — adding `--force` (`DexCorral.exe --register --force`) registers anyway, unsupported and untested.
3. Run `DexCorral.exe --startup` to start DexCorral in the current session, or restart Explorer / log out and back in.

### Uninstalling

Uninstall from **Settings > Apps > Installed apps**, or via the Start Menu group's **Uninstall DexCorral** entry. The uninstaller asks whether to **keep your configuration** (corral layouts and appearance settings) — choose *Yes* if you plan to reinstall later. It then unregisters the shell extension and restarts Explorer to fully unload.

After uninstalling, icons that were inside Corrals reappear as normal desktop icons; you may want to rearrange them (right-click desktop > **Sort by > Name**).

## Core Concepts

### Corrals
A Corral is a shaded, semi-transparent window that lives on your desktop and contains a group of icons. Icons assigned to a Corral are hidden from the regular desktop and drawn inside the Corral window instead. They remain real desktop items — opening, renaming, deleting, and dragging all work as usual.

### Tabs
Each Corral can hold multiple **tabs**, each with its own title, icon list, background color, view mode, and header font. Click a tab to switch to it. Tabs let one Corral hold several groups (e.g. "Work", "Games", "Downloads") without taking more screen space.

**Reordering tabs:** hover over a tab to reveal a small grip handle (a dotted "⠿" mark) on its left edge, then drag the grip left or right to move the tab to a new position. The new order is saved automatically.

### Catch-All
One tab can be designated the **Catch-All**: any new file or shortcut that lands on your desktop is automatically captured into it, keeping the rest of your desktop clean.

### Virtual Corrals
A virtual tab mirrors the contents of any folder on your PC — point it at `Downloads` or a project directory and its files appear inside the Corral, kept in sync automatically as the folder changes. Virtual tabs are a live view: you manage the files in the folder itself (drops onto a virtual tab are not accepted).

## Managing Corrals

### Creating
* Right-click the DexCorral **tray icon** (or the background of any Corral) and choose **Create New Corral**.
* Choose **New Virtual Corral** instead to pick a folder and create a Corral mirroring it.

### Moving and Resizing
* Drag the **title bar** to move a Corral.
* Drag any **edge or corner** to resize. While moving or resizing, Corrals snap to screen edges and align to other Corrals on the same monitor.
* Corrals are immune to **Win+D / Show Desktop** — they stay visible with your desktop.

### Roll-Up
Double-click the title bar to **roll up** a Corral so only the title bar remains visible. Double-click again to expand it.

### Multi-Monitor
Corral positions are remembered **per monitor and per resolution**. Moving a Corral to another monitor or changing display resolution restores the layout you used there last.

### Customizing
Right-click a Corral's title bar or background:

<img src="assets/screenshots/corral-context-menu.png" alt="The corral context menu, showing Add Tab, Rename Tab, Appearance, View, Sort By, and corral commands" width="380">

* **Add Tab** — add a new tab to this Corral.
* **Detach Tab** — move the current tab out into its own Corral window (shown when the Corral has more than one tab).
* **Rename Tab** — change the current tab's title.
* **Appearance...** — open the appearance dialog (see below).
* **Change Folder...** — for virtual tabs, point the tab at a different folder.
* **View** — switch between **Small / Medium / Large Icons** and **Details** (a list with name, type, size, modified date, and cloud sync status for OneDrive files).
* **Catch-All (receives new files)** — make this tab the catch-all for new desktop items.
* **Add Special Icon** — add special shell items such as the Recycle Bin to the Corral.
* **Show Desktop Icons** — toggle visibility of all native desktop icons.
* **Create New Corral / New Virtual Corral** — same as the tray menu.
* **Close Tab / Delete Corral** — remove the current tab, or the whole Corral if it's the last tab. Contained icons return to the desktop.

### Appearance Dialog
**Appearance...** gives live-preview control over:

<img src="assets/screenshots/appearance-dialog.png" alt="The Appearance dialog with background colour, opacity, header, icon and spacing controls" width="380">

* **Background color** and **opacity** — from fully opaque down to fully transparent, where the Corral fill disappears entirely and only the organized icons remain over your wallpaper.
* **Border opacity** — from a solid frame down to none at all, leaving the Corral as a frameless shape on your wallpaper. The resize grip in the bottom-right corner stays visible either way, so a frameless Corral can still be grabbed and resized.
* **Header** — title bar height, opacity, font face/size, and font color (font settings are per tab).
* **Icons** — opacity, tint color, and tint strength.
* **Icon spacing** — horizontal and vertical spacing (50–200%).

Checkboxes at the bottom let you save the current style as the **default for new corrals**, **apply the changes** you just made to all corrals, or **copy the full style** to all corrals.

#### Header opacity and tabs
Header opacity fades the title bar and tab strip independently of the Corral fill, so a Corral can sit almost invisibly on the wallpaper and still be there when you need it.

* **Inactive tabs adjust automatically.** They are always rendered dimmer and darker than the active tab, derived from whatever header opacity you set — there is no separate slider to keep in sync, and the active tab stays recognizable at every setting.
* **The slider stops short of fully transparent.** The header is what you drag to move a Corral, double-click to roll it up, and right-click for the menu; at zero it would be an invisible window that still catches the mouse. The minimum leaves a faint edge you can find.
* **Rolled-up Corrals stay visible.** Rolled up, the header *is* the whole Corral, so it is kept a little more visible than your setting while in that state. Unrolling restores exactly what you chose.
* **Hovering brings everything back.** Moving the mouse over a Corral fades its header, border and icons up to full strength, then fades them back when you leave — so a faded Corral is always fully readable while you are working in it.

## Working with Icons

### Adding
* Drag any icon from the desktop (or files from an Explorer window) and drop it onto a Corral.
* New desktop items land automatically in the Catch-All tab, if one is set.

### Using
* **Double-click** an icon to open it.
* **Drop a file onto an icon** inside a Corral to invoke its target — e.g. drop a document onto an application's icon to open it with that app.
* **Double-click an icon's label** to rename it in place.
* **Scroll** with the mouse wheel or trackpad when a tab holds more icons than fit; a slim scrollbar appears on hover.

### Right-Click Menu
Right-clicking an icon shows the standard Windows context menu (Open, Cut, Copy, Delete, Properties, ...) plus **Remove from Corral**, which returns the icon to the regular desktop without deleting anything.

## Quick-Hide

Double-click an empty spot on the desktop to hide everything at once: all native desktop icons and all corrals (they fade out). Double-click the desktop again to bring everything back exactly as it was.

* Corrals can opt out: right-click a corral and check **Exclude from Quick-Hide** to keep it visible while everything else hides.
* The tray menu's **Quick-Hide Everything** entry toggles the same state.
* Quick-hide is temporary — restarting DexCorral always restores your normal desktop.

## Tray Icon

The DexCorral tray icon's right-click menu offers:

* **About** — version and license information.
* **Create New Corral** / **New Virtual Corral**.
* **Show Desktop Icons** — toggle all native desktop icons.
* **Quick-Hide Everything** — hide/show icons and corrals at once (same as double-clicking the desktop).

## Desktop Integration

Because DexCorral hooks Explorer's desktop directly:

* Icons inside Corrals are invisible to desktop hit-testing, rubber-band selection, and keyboard navigation — you can't accidentally select or disturb them.
* Desktop **Sort by** and auto-arrange never move Corral-owned icons, and visible icons are compacted so sorting leaves no gaps where hidden icons used to be.
* Corral-owned icons are immune to repositioning by Explorer or third-party tools — only DexCorral moves them.

## Configuration

All settings (corral layouts, tabs, colors, fonts, assigned files) are stored in a single JSON file:

```
%APPDATA%\DexCorral\config.json
```

The format is forward- and backward-compatible: fields missing from an older config simply get default values. The file is written automatically; if you edit it by hand, do so while DexCorral is not running.

## Known Limitations

* **Identical filenames on the user and Public desktop** — corral membership is stored as a bare filename, so two files with the exact same name on `%USERPROFILE%\Desktop` and `C:\Users\Public\Desktop` can't be told apart; DexCorral assumes the one on the user desktop. (Items with the same *display* name but different filenames — e.g. a folder `test` next to `test.txt` with hidden extensions — are fully distinguished.)

## Troubleshooting

* **SmartScreen / antivirus warnings** — the binaries are currently unsigned; this is expected for software from a small developer. Code signing is planned.
* **Corrals don't appear after install** — right-click the desktop once to wake the shell, or log out and back in (the Start-with-Windows entry re-injects DexCorral at login).
* **Desktop icons misbehave** — restarting Explorer resets the hook: `Stop-Process -Name explorer -Force; Start-Process explorer.exe` in PowerShell, or via Task Manager.
* **Bug reports** — please file issues at the [GitHub issue tracker](https://github.com/guHe330/DexCorral/issues).
