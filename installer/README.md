# DexCorral Installer

This directory contains the Inno Setup installer configuration for DexCorral.

## Inno Setup (Windows 10/11)

**Location:** [`innosetup/`](innosetup/)

Traditional Windows installer using Inno Setup 6+.

**Features:**
- Clean install/uninstall
- Automatic shell extension registration
- Explorer restart handled automatically
- Requires admin privileges (for COM registration)

## Building

Version can be overridden from the command line:

```powershell
iscc /DMyAppVersion=1.2.3 innosetup\DexCorral.iss
```

Output goes to `innosetup/output/`.

## Distribution

For public releases:
1. Build with proper version number
2. Sign with a trusted code signing certificate
3. Test installation on clean Windows installation
4. Distribute via GitHub Releases
