# DexCorral MSIX Installer

This directory contains the MSIX package configuration for DexCorral.

## Structure

```
installer/msix/
├── AppxManifest.xml          # Package manifest
├── priconfig.xml             # Resource configuration
├── build-msix.ps1            # Build script
├── Assets/                   # Application icons and logos
│   ├── Square44x44Logo.png
│   ├── Square150x150Logo.png
│   ├── Wide310x150Logo.png
│   └── StoreLogo.png
├── Scripts/                  # Install/uninstall scripts
│   ├── install.bat
│   └── uninstall.bat
├── staging/                  # Temporary build directory (created during build)
└── output/                   # Built MSIX packages (created during build)
```

## Building the MSIX Package

### Prerequisites

1. **Windows SDK** - Install via Visual Studio Installer under "Windows SDK"
   - Provides `MakeAppx.exe` and `SignTool.exe`
2. **Build artifacts** - Run the main build first:
   ```powershell
   cd ..\DexCorralCpp
   powershell -File build.ps1
   ```

### Build Commands

**Basic build:**
```powershell
.\build-msix.ps1
```

**Build with version number:**
```powershell
.\build-msix.ps1 -Version 1.2.0.0
```

**Clean build:**
```powershell
.\build-msix.ps1 -Clean
```

**Build and sign with test certificate:**
```powershell
.\build-msix.ps1 -Sign
```

The script will:
1. Create a staging directory
2. Copy all binaries (DexCorral.exe, Watchdog, DexCorralHook.dll)
3. Copy manifest and scripts
4. Generate placeholder assets if missing
5. Package into MSIX using MakeAppx.exe
6. Optionally sign with a self-signed test certificate
7. Output to `output/DexCorral_x.x.x.x.msix`

## Installation

### End User Installation

**Option 1: Double-click**
- Simply double-click the `.msix` file
- Windows will prompt to install

**Option 2: PowerShell**
```powershell
Add-AppxPackage -Path "output\DexCorral_1.0.0.0.msix"
```

### Developer Installation (with test certificate)

If the package is signed with a self-signed certificate, you need to trust it first:

```powershell
# Import the test certificate to Trusted Root
$cert = Get-PfxCertificate -FilePath "DexCorral_TestCert.pfx"
Import-Certificate -CertStoreLocation Cert:\LocalMachine\Root -FilePath "DexCorral_TestCert.pfx"

# Then install
Add-AppxPackage -Path "output\DexCorral_1.0.0.0.msix"
```

## Uninstallation

**Via Settings:**
1. Settings → Apps → Installed apps
2. Find "DexCorral"
3. Click "..." → Uninstall

**Via PowerShell:**
```powershell
Remove-AppxPackage -Package "DexCorral_1.0.0.0_x64__<hash>"
```

Find the package name:
```powershell
Get-AppxPackage | Where-Object Name -like "*DexCorral*"
```

## Assets

The `Assets/` directory needs PNG images for the Windows tile/icon system:

- **Square44x44Logo.png** (44×44) - Small tile, taskbar
- **Square150x150Logo.png** (150×150) - Medium tile
- **Wide310x150Logo.png** (310×150) - Wide tile
- **StoreLogo.png** (50×50) - Microsoft Store listing

Create these from your application icon at the appropriate sizes. The build script will create placeholder files if they don't exist, but you should replace them with actual branded images.

## Features

The MSIX package includes:

1. **Auto-start on login** - Configured via `windows.startupTask` extension
2. **Full trust application** - Can access desktop and inject into Explorer
3. **Clean uninstall** - Runs uninstall.bat to clean up processes and hooks
4. **All executables** - Main app, Watchdog, and DexCorralHook.dll

## Limitations

- **Explorer hook registration** - The hook DLL (DexCorralHook.dll) is injected at runtime by DexCorral.exe, not during MSIX installation. This is by design.
- **Elevation** - MSIX apps run without elevation by default. The Explorer injection happens when DexCorral.exe runs.
- **Certificate** - For production, you'll need a proper code signing certificate (not a self-signed test cert)

## Production Signing

For public distribution, obtain a code signing certificate from a trusted CA:

```powershell
.\build-msix.ps1 -Version 1.0.0.0
SignTool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a "output\DexCorral_1.0.0.0.msix"
```

Or use Azure Code Signing, Windows Store, or another trusted signing service.

## Troubleshooting

**"Windows SDK tools not found"**
- Install Windows SDK via Visual Studio Installer
- Or download standalone SDK from Microsoft

**"Failed to create MSIX package"**
- Check that all required files exist in staging directory
- Verify AppxManifest.xml is valid XML
- Ensure asset files exist (even as placeholders)

**"Package signature is invalid"**
- Trust the test certificate in Trusted Root store
- Or disable signature verification for sideloading (developer mode)

**"App crashes on launch"**
- Check that DexCorralHook.dll is included in package
- Verify all dependencies are present (use Dependency Walker)
- Check Windows Event Viewer for crash details
