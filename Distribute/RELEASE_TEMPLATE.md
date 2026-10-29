## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

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
```

Check a file in PowerShell:

```powershell
Get-FileHash DexCorral_${VERSION}_Setup.exe -Algorithm SHA256
```

Both artifacts also carry a GitHub build provenance attestation, which ties them to the exact workflow run and commit that produced them. With the [GitHub CLI](https://cli.github.com/):

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
3. Run `DexCorral.exe --register --scope=user`, or `--scope=machine` from an Administrator prompt
4. Restart Explorer or log out/in

The zip contains:
- `DexCorral.exe`: registration tool
- `DexCorralHook.dll`: Explorer shell extension
- `LICENSE`: GPL-3.0 license
- `readme.txt`: quick start guide

### What's New

<!-- Per-release, not cumulative: list only the changes since the last tag.
     Emptied after each release ships. -->

- Opening a file from another program (for example Ctrl+clicking a file link in a terminal) no longer adds extra DexCorral icons to the tray or pauses DexCorral as if Explorer had crashed.

The complete list is in the [changelog](https://github.com/guHe330/DexCorral/blob/main/docs/CHANGELOG.md).

### Known issues

- Two files with the exact same filename on the user desktop and the Public desktop can't be told apart; DexCorral assumes the one on the user desktop.
- If Stardock Fences is started while DexCorral is already running, DexCorral can only tell you; it steps aside at the next sign-in.
- Folders on network drives cannot be used for virtual Corrals.

See [open issues](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
