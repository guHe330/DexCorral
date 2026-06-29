# DexCorral Installer

This directory contains the Inno Setup installer configuration for DexCorral.

## Inno Setup (Windows 11)

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

