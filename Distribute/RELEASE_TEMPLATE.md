## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ${SETUP_SIZE} |
| `Portable_DexCorral_${VERSION}.zip` | Portable package (no installer) | ${PORTABLE_SIZE} |
| `DexCorral_${VERSION}_sbom.cdx.json` | Software bill of materials (CycloneDX) | ${SBOM_SIZE} |

> **Requires Windows 11** (build 22000 or newer). Windows 10 is end of life and is not supported.

> **Not alongside Stardock Fences.** Setup declines to install while Fences is installed; see What's New below.

> **Note:** Both packages are currently unsigned, so Windows SmartScreen shows a warning and reports the publisher as unknown. Click "More info" then "Run anyway" to proceed. You do not have to take that on trust: verify the download first with the checksums or the build attestation below.

### Verifying your download

SHA-256:

```
DexCorral_${VERSION}_Setup.exe     ${SETUP_SHA256}
Portable_DexCorral_${VERSION}.zip  ${PORTABLE_SHA256}
```

Check a file in PowerShell:

```powershell
Get-FileHash DexCorral_${VERSION}_Setup.exe -Algorithm SHA256
```

Both artifacts also carry a GitHub build provenance attestation, which ties them to the exact workflow run and commit that produced them. With the [GitHub CLI](https://cli.github.com/):

```powershell
gh attestation verify DexCorral_${VERSION}_Setup.exe --repo guHe330/DexCorral
```

### Installation Options

#### Option 1: Installer (Recommended)
**For Windows 11 users**

1. Download `DexCorral_${VERSION}_Setup.exe`
2. Run the installer and choose how to install:
   - **All users**: `C:\Program Files\DexCorral`, needs Administrator rights, every account on the PC gets DexCorral
   - **Just me**: `%LOCALAPPDATA%\Programs\DexCorral`, no Administrator rights and no UAC prompt, your account only
3. The installer registers the shell extension and starts DexCorral in the running Explorer

The two modes cannot be installed side by side; Setup detects the other one and offers to sort it out first.

#### Option 2: Portable Package
**For advanced users or non-administrator installs**

1. Download `Portable_DexCorral_${VERSION}.zip`
2. Extract to any folder
3. Run `DexCorral.exe --register --scope=user`, or `--scope=machine` from an Administrator prompt
4. Restart Explorer or log out/in

**Package Contents:**
- `DexCorral.exe` - Registration tool
- `DexCorralHook.dll` - Explorer shell extension
- `LICENSE` - GPL-3.0 License
- `readme.txt` - Quick start guide

### What's New

<!-- Per-release, not cumulative: list only the changes since the last tag.
     Emptied after each release ships. -->

**Dragging and dropping**

- Files dragged from an Explorer window onto a Corral now land in it, as a desktop shortcut; before, nothing happened. Virtual Corrals accept drops and move or copy the files into their folder.
- Files can be dragged out of a virtual Corral, to Explorer, the desktop or an application.
- Dragging one icon of a selection carries the whole selection.
- Icons can be moved between tabs: drag them onto a tab, or use **Move to Tab** in the icon menu. A drag that rests on a tab brings it forward.

**Everyday use**

- **Sort By** for normal Corrals (name or type), and for virtual tabs in every view.
- Keyboard: Ctrl+Tab switches tabs, PageUp / PageDown scroll. An optional global hotkey for Quick-Hide can be set in `config.json` and is then shown in the tray menu.
- **Create New Corral** and **Add Tab** ask for the name right away.
- **Expand on Hover** can be switched off per Corral, for a rolled-up Corral that keeps popping open.
- Double-clicking the tray icon toggles Quick-Hide, like double-clicking the desktop.
- The **Close Tab** and **Delete Corral** confirmations say that no files are deleted.
- Appearance dialog: **Reset to Defaults**, and clearer wording for the three "apply" options. New About dialog with clickable links. Tray menu: **User Manual** and **Open Config Folder**.
- DexCorral follows the Windows "Animation effects" setting.

**Safety**

- A damaged `config.json` no longer wipes your layout: DexCorral keeps a backup, falls back to it and tells you.
- If Explorer crashes twice within two minutes while DexCorral starts, DexCorral pauses itself and stays paused until you choose **Resume DexCorral**. While paused, all icons are on the desktop and your layout is untouched. This replaces the old safe mode.
- **DexCorral does not run alongside Stardock Fences.** Both take over the desktop icon view, and with Fences running the icons DexCorral hides are drawn twice. Setup and `--register` now decline while Fences is installed, and an installed DexCorral pauses itself. DexCorral never changes anything belonging to Fences. Details in the User Manual.

**Removed**

- The **Hide shortcut arrows** option, which no menu led to any more.

The complete list is in the [changelog](https://github.com/guHe330/DexCorral/blob/main/docs/CHANGELOG.md).
### Known Issues

- Two files with the exact same filename on the user desktop and the Public desktop can't be told apart; DexCorral assumes the one on the user desktop.
- If Stardock Fences is started while DexCorral is already running, DexCorral can only tell you; it steps aside at the next sign-in.
- Folders on network drives cannot be used for virtual Corrals.

See [open issues](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
