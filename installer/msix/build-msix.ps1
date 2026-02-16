#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build MSIX installer package for DexCorral

.DESCRIPTION
    This script packages DexCorral into an MSIX installer.
    Requires Windows SDK (MakeAppx.exe and SignTool.exe)

.PARAMETER Version
    Version number for the package (default: 1.0.0.0)

.PARAMETER Clean
    Remove existing package before building

.PARAMETER Sign
    Sign the package with a test certificate

.EXAMPLE
    .\build-msix.ps1 -Version 1.0.0.0 -Sign
#>

param(
    [string]$Version = "1.0.0.0",
    [switch]$Clean,
    [switch]$Sign
)

$ErrorActionPreference = "Stop"

# Paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)
$BuildDir = Join-Path $ProjectRoot "DexCorral\build"
$MsixDir = $ScriptDir
$StagingDir = Join-Path $MsixDir "staging"
$OutputDir = Join-Path $MsixDir "output"

Write-Host "Building DexCorral MSIX Installer v$Version" -ForegroundColor Cyan

# Clean if requested
if ($Clean -and (Test-Path $StagingDir)) {
    Write-Host "Cleaning staging directory..." -ForegroundColor Yellow
    Remove-Item -Path $StagingDir -Recurse -Force
}

if ($Clean -and (Test-Path $OutputDir)) {
    Write-Host "Cleaning output directory..." -ForegroundColor Yellow
    Remove-Item -Path $OutputDir -Recurse -Force
}

# Create staging directory
Write-Host "Creating staging directory..." -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir "Assets") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir "Scripts") | Out-Null

# Check if build artifacts exist
if (-not (Test-Path (Join-Path $BuildDir "DexCorral.exe"))) {
    Write-Host "Build artifacts not found. Building project..." -ForegroundColor Yellow
    Push-Location (Join-Path $ProjectRoot "DexCorralCpp")
    try {
        & powershell -File "build.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
    } finally {
        Pop-Location
    }
}

# Copy executables and DLLs
Write-Host "Copying binaries..." -ForegroundColor Green
Copy-Item -Path (Join-Path $BuildDir "DexCorral.exe") -Destination $StagingDir
Copy-Item -Path (Join-Path $BuildDir "DexCorral.Watchdog.exe") -Destination $StagingDir
Copy-Item -Path (Join-Path $BuildDir "DexCorralHook.dll") -Destination $StagingDir

# Copy manifest
Write-Host "Copying manifest..." -ForegroundColor Green
$ManifestPath = Join-Path $MsixDir "AppxManifest.xml"
$StagingManifestPath = Join-Path $StagingDir "AppxManifest.xml"

# Use XML manipulation instead of regex to safely update version
[xml]$Manifest = Get-Content $ManifestPath
$Manifest.Package.Identity.Version = $Version
$Manifest.Save($StagingManifestPath)

# Copy scripts
Write-Host "Copying installation scripts..." -ForegroundColor Green
Copy-Item -Path (Join-Path $MsixDir "Scripts\install.bat") -Destination (Join-Path $StagingDir "Scripts")
Copy-Item -Path (Join-Path $MsixDir "Scripts\uninstall.bat") -Destination (Join-Path $StagingDir "Scripts")

# Create placeholder assets if they don't exist
Write-Host "Setting up assets..." -ForegroundColor Green
$AssetSizes = @(
    @{Name="Square44x44Logo.png"; Width=44; Height=44},
    @{Name="Square150x150Logo.png"; Width=150; Height=150},
    @{Name="Wide310x150Logo.png"; Width=310; Height=150},
    @{Name="StoreLogo.png"; Width=50; Height=50}
)

foreach ($Asset in $AssetSizes) {
    $AssetPath = Join-Path $MsixDir "Assets\$($Asset.Name)"
    $StagingAssetPath = Join-Path $StagingDir "Assets\$($Asset.Name)"

    if (Test-Path $AssetPath) {
        Copy-Item -Path $AssetPath -Destination $StagingAssetPath
    } else {
        Write-Host "  Creating placeholder $($Asset.Name)..." -ForegroundColor DarkGray
        # Create a simple colored rectangle as placeholder
        # This requires System.Drawing which may not be available on all systems
        # In production, these should be replaced with actual icons

        # For now, just create an empty file - the user will need to provide actual icons
        New-Item -ItemType File -Force -Path $StagingAssetPath | Out-Null
    }
}

# Find Windows SDK tools
Write-Host "Locating Windows SDK tools..." -ForegroundColor Green
$WindowsKits = "C:\Program Files (x86)\Windows Kits\10\bin"
if (Test-Path $WindowsKits) {
    $SdkVersions = Get-ChildItem $WindowsKits -Directory | Sort-Object Name -Descending
    $SdkBinPath = $null
    foreach ($Ver in $SdkVersions) {
        $TestPath = Join-Path $Ver.FullName "x64\makeappx.exe"
        if (Test-Path $TestPath) {
            $SdkBinPath = Join-Path $Ver.FullName "x64"
            break
        }
    }

    if (-not $SdkBinPath) {
        throw "Windows SDK tools not found. Install Windows SDK from Visual Studio Installer."
    }

    $MakeAppx = Join-Path $SdkBinPath "makeappx.exe"
    $SignTool = Join-Path $SdkBinPath "signtool.exe"
} else {
    throw "Windows SDK not found at $WindowsKits"
}

# Build MSIX package
$OutputMsix = Join-Path $OutputDir "DexCorral_$Version.msix"
Write-Host "Creating MSIX package..." -ForegroundColor Green
Write-Host "  Output: $OutputMsix" -ForegroundColor DarkGray

& $MakeAppx pack /d $StagingDir /p $OutputMsix /o
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create MSIX package"
}

# Sign the package if requested
if ($Sign) {
    Write-Host "Signing package..." -ForegroundColor Green

    # Check if test certificate exists
    $CertPath = Join-Path $MsixDir "DexCorral_TestCert.pfx"
    if (-not (Test-Path $CertPath)) {
        Write-Host "  Creating test certificate..." -ForegroundColor Yellow

        # Create a self-signed certificate for testing
        $Cert = New-SelfSignedCertificate -Type Custom -Subject "CN=DexCorral" `
            -KeyUsage DigitalSignature -FriendlyName "DexCorral Test Certificate" `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

        $Password = ConvertTo-SecureString -String "DexCorralTest" -Force -AsPlainText
        Export-PfxCertificate -Cert $Cert -FilePath $CertPath -Password $Password | Out-Null

        Write-Host "  Test certificate created: $CertPath" -ForegroundColor DarkGray
        Write-Host "  Password: DexCorralTest" -ForegroundColor DarkGray
    }

    & $SignTool sign /fd SHA256 /a /f $CertPath /p "DexCorralTest" $OutputMsix
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Warning: Failed to sign package" -ForegroundColor Yellow
    } else {
        Write-Host "  Package signed successfully" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "MSIX package created successfully!" -ForegroundColor Green
Write-Host "  Location: $OutputMsix" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installation Instructions:" -ForegroundColor Yellow
Write-Host "  1. Double-click the .msix file to install" -ForegroundColor Gray
Write-Host "  2. Or use PowerShell: Add-AppxPackage -Path '$OutputMsix'" -ForegroundColor Gray
Write-Host ""
Write-Host "Note: You may need to install the test certificate to Trusted Root" -ForegroundColor DarkGray
Write-Host "      if the package is signed with a self-signed certificate." -ForegroundColor DarkGray
Write-Host ""

# Cleanup option
Write-Host "Keep staging directory for inspection? (Y/N): " -NoNewline -ForegroundColor Yellow
# Auto-cleanup in automated mode
if ($env:CI -eq "true") {
    Write-Host "No (CI mode)" -ForegroundColor Gray
    Remove-Item -Path $StagingDir -Recurse -Force
} else {
    Write-Host "(Keeping for now - delete manually if not needed)" -ForegroundColor Gray
}
