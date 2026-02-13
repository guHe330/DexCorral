# DexCorral Installers

This directory contains packaging configurations for DexCorral distribution.

## MSIX Package (Windows 10/11)

**Location:** [`msix/`](msix/)

Modern Windows app package format for Windows 10 and later.

**Quick Start:**
```powershell
cd msix
.\build-msix.ps1 -Sign
```

See [msix/README.md](msix/README.md) for detailed instructions.

**Features:**
- ✅ Clean install/uninstall
- ✅ Auto-start on login
- ✅ Digital signature support
- ✅ Microsoft Store compatible
- ✅ Includes all components (exe files + DexCorralHook.dll)

## Future Installers

Additional installer formats can be added here:

- **MSI (Windows Installer)** - Traditional enterprise deployment
- **Portable ZIP** - No installation required
- **Chocolatey Package** - Windows package manager
- **WinGet Manifest** - Windows Package Manager

## Current Status

| Format | Status | Location | Notes |
|--------|--------|----------|-------|
| MSIX | ✅ Ready | `msix/` | Requires Windows SDK to build |
| MSI | ⏳ Planned | - | Future addition |
| Portable | ⏳ Planned | - | Simple ZIP archive |

## Building

Each installer type has its own build script in its subdirectory. See the respective README files for instructions.

## Distribution

For public releases:
1. Build with proper version number
2. Sign with a trusted code signing certificate
3. Test installation on clean Windows installation
4. Distribute via GitHub Releases, Microsoft Store, or your preferred method
