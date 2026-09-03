## DexCorral ${VERSION} (Alpha)

> This is an alpha release. Expect breaking changes and rough edges.

### Downloads

| File | Description | Size |
|------|-------------|------|
| `DexCorral_${VERSION}_Setup.exe` | Installer (recommended) | ${SETUP_SIZE} |
| `Portable_DexCorral_${VERSION}.zip` | Portable package (no installer) | ${PORTABLE_SIZE} |
| `DexCorral_${VERSION}_sbom.cdx.json` | Software bill of materials (CycloneDX) | ${SBOM_SIZE} |

> **Requires Windows 11** (build 22000 or newer). Windows 10 is end of life and is not supported.

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

- **Install for all users, or just for yourself.** The installer now asks on its first page. A per-user install goes to `%LOCALAPPDATA%\Programs\DexCorral`, registers in `HKCU` and needs no Administrator rights at all, which makes DexCorral usable on a PC where you do not have admin. An all-users install works as before.
- Fixed: on an all-users install, the "start at login" entry and the language setting were written to the profile of the account that answered the UAC prompt, not the account being installed for. On a PC with more than one user this meant DexCorral never started at login for anyone but the installer. Both now go to `HKLM` and apply to every account.
- Fixed: installing or uninstalling as an administrator could restart Explorer for every logged-on user, not just the one running Setup. It is now limited to the current session.
- The two install modes are mutually exclusive, since they share one shell extension registration. Setup detects the other kind before writing anything and offers to remove it or warns you about it.
- Registration is now explicit about where it writes: `DexCorral.exe --register --scope=user` or `--scope=machine`, and never through `HKEY_CLASSES_ROOT`. Upgrades clean up the old scopeless entries automatically; portable users can run `DexCorral.exe --cleanup-legacy` once.
- Upgrading from 1.0.27 or earlier keeps working as an all-users install; nothing to do beyond running the installer.
- README now shows Corrals running over an animated Lively wallpaper, with fresh screenshots.

### Known Issues

- Two files with the exact same filename on the user desktop and the Public desktop can't be told apart; DexCorral assumes the one on the user desktop.

See [open issues](https://github.com/guHe330/DexCorral/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
