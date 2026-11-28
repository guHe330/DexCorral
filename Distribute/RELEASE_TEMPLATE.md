## DexCorral ${VERSION}

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ${SETUP_SIZE} |
| `Portable_DexCorral_${VERSION}.zip` | Portable package (no installer) | ${PORTABLE_SIZE} |
| `DexCorral_${VERSION}_sbom.cdx.json` | Software bill of materials (CycloneDX) | ${SBOM_SIZE} |

> **Requires Windows 11** (build 22000 or newer). Windows 10 is end of life and is not supported.

> **Not alongside Stardock Fences.** Setup declines to install while Fences is installed; see "Stardock Fences" in the [User Manual](https://github.com/guHe330/DexCorral/blob/main/docs/USER_MANUAL.md#stardock-fences).

> Both packages are currently unsigned, so Windows SmartScreen shows a warning and reports the publisher as unknown. Click "More info", then "Run anyway". You can verify the download first with the checksums or the build attestation below.

### Verifying your download

SHA-256:

```
DexCorral_${VERSION}_Setup.exe     ${SETUP_SHA256}
Portable_DexCorral_${VERSION}.zip  ${PORTABLE_SHA256}
DexCorral_${VERSION}_sbom.cdx.json ${SBOM_SHA256}
```

Check a file in PowerShell:

```powershell
Get-FileHash DexCorral_${VERSION}_Setup.exe -Algorithm SHA256
```

All three files also carry a GitHub build provenance attestation, which ties them to the exact workflow run and commit that produced them. With the [GitHub CLI](https://cli.github.com/):

```powershell
gh attestation verify DexCorral_${VERSION}_Setup.exe --repo guHe330/DexCorral
```

### Installation options

#### Option 1: Installer (recommended)

1. Download `DexCorral_${VERSION}_Setup.exe`
2. Run the installer and choose how to install:
   - **All users**: `C:\Program Files\DexCorral`, needs Administrator rights, every account on the PC gets DexCorral
   - **Just me**: `%LOCALAPPDATA%\Programs\DexCorral`, no Administrator rights and no UAC prompt, your account only
3. The installer registers the shell extension and starts DexCorral in the running Explorer

The two modes cannot be installed side by side; Setup detects the other one and offers to sort it out first.

#### Option 2: Portable package

For advanced users, or to install without Administrator rights.

1. Download `Portable_DexCorral_${VERSION}.zip`
2. Extract to any folder
3. Run `DexCorral.exe --register --scope=user`, or `--scope=machine` for all users (asks for Administrator rights)
4. Just for you, DexCorral starts right away; for all users, restart Explorer or sign out and back in

The zip contains:
- `DexCorral.exe`: registration tool
- `DexCorralHook.dll`: Explorer shell extension
- `LICENSE`: GPL-3.0 license
- `readme.txt`: quick start guide

### What's New

<!-- Per-release, not cumulative: list only the changes since the last tag.
     Emptied after each release ships. -->

- **Look** in the settings window: six ready-made looks (Automatic, Classic Blue, Light Glass, Dark Glass, Graphite, Compact), shown on an example Corral before you apply one to every Corral.
- Presets of your own: **Save as...** in the Appearance dialog stores a look under a name, to load for one Corral or all. The settings window exports and imports them as a file.
- **Windows accent color** in the Appearance dialog: a tab can take its background from the accent colour of Windows and follows it when it changes.
- **Undo Last Move** in the right-click menu of every Corral takes back a move or resize, up to ten steps, together with the desktop icons it pushed out of the way.
- A new look for the dialogs and menus: light or dark like Windows, or as you choose. The Appearance dialog is split into three pages and is a good third shorter.
- Network folders: a share that goes away no longer freezes Explorer when you open something in its tab, and a share that answers is no longer asked every ten seconds, which could keep a NAS from going to sleep.
- The Corrals stay behind the open windows while one of their dialogs or questions is shown.
- A new Corral takes the nearest free place and no longer appears on top of another one.
- Many fixes for drag and drop, renaming and the start of DexCorral with a missing desktop folder or a locked `config.json`.

The complete list is in the [changelog](https://github.com/guHe330/DexCorral/blob/main/docs/CHANGELOG.md).

### Known issues

- Two files with the exact same filename on the user desktop and the Public desktop can't be told apart; DexCorral assumes the one on the user desktop.
- A network folder is read on the thread all Corrals share. If a share stops answering in the two seconds after DexCorral has checked it, or while the folder is being read, Corrals can stall once until Windows gives up on it.
- On recent Windows 11 builds the desktop right-click menu no longer lists **New DexCorral** and **New Virtual DexCorral**. Hold Shift while right-clicking the desktop, or use the tray menu or the right-click menu of any Corral.
- If Stardock Fences is started while DexCorral is already running, DexCorral can only tell you; it steps aside at the next sign-in.

See [open issues](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
