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

- Compatibility fix with third-party NM_CUSTOMDRAW hooks: DexCorral now passes through to the original chain for non-corral icons
- Updated default appearance settings for new corrals (color, font, opacity, tint, spacing)
- First-launch corral now correctly inherits default appearance settings from AppConfig

### Known Issues

See [open issues](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+is%3Aopen+label%3Abug) for known bugs.
