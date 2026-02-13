# Build MSIX package for DexCorral
#
# Prerequisites:
#   - Windows SDK installed (for makeappx.exe and signtool.exe)
#   - Visual Studio 2022 with Desktop C++ workload
#   - Certificate for signing (or create test cert with New-SelfSignedCertificate)
#
# Usage:
#   .\build-msix.ps1                    # Build and package
#   .\build-msix.ps1 -Sign              # Build, package, and sign
#   .\build-msix.ps1 -Sign -Install     # Build, package, sign, and install

param(
    [switch]$Sign,
    [switch]$Install,
    [switch]$Clean,
    [string]$CertThumbprint = "",  # Optional: specify certificate thumbprint
    [string]$Version = "1.0.0.0"
)

$ErrorActionPreference = "Stop"

# Paths
$RepoRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $RepoRoot "DexCorralCpp\build"
$MsixDir = $PSScriptRoot
$StagingDir = Join-Path $MsixDir "staging"
$AssetsDir = Join-Path $MsixDir "Assets"
$OutputDir = Join-Path $MsixDir "output"
$MsixFile = Join-Path $OutputDir "DexCorral_${Version}_x64.msix"

# SDK tools (adjust path if needed)
$WindowsSDKPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"
$MakeAppx = Join-Path $WindowsSDKPath "makeappx.exe"
$SignTool = Join-Path $WindowsSDKPath "signtool.exe"

Write-Host "=== DexCorral MSIX Build ===" -ForegroundColor Cyan

# Clean staging directory
if ($Clean -and (Test-Path $StagingDir)) {
    Write-Host "Cleaning staging directory..." -ForegroundColor Yellow
    Remove-Item $StagingDir -Recurse -Force
}

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null

# Step 1: Build the application
Write-Host "`n[1/5] Building DexCorral..." -ForegroundColor Green
Push-Location (Join-Path $RepoRoot "DexCorralCpp")
try {
    & powershell -File build.ps1
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
} finally {
    Pop-Location
}

# Step 2: Copy binaries to staging
Write-Host "`n[2/5] Staging files..." -ForegroundColor Green

$FilesToCopy = @(
    "DexCorral.exe",
    "DexCorral.Watchdog.exe",
    "DexCorralHook.dll"
)

foreach ($file in $FilesToCopy) {
    $source = Join-Path $BuildDir $file
    $dest = Join-Path $StagingDir $file
    if (Test-Path $source) {
        Copy-Item $source $dest -Force
        Write-Host "  Copied: $file" -ForegroundColor Gray
    } else {
        Write-Error "Missing required file: $file"
    }
}

# Copy manifest
Copy-Item (Join-Path $MsixDir "AppxManifest.xml") $StagingDir -Force

# Update version in manifest
$manifestPath = Join-Path $StagingDir "AppxManifest.xml"
$manifest = [xml](Get-Content $manifestPath)
$manifest.Package.Identity.Version = $Version
$manifest.Save($manifestPath)
Write-Host "  Updated manifest version to $Version" -ForegroundColor Gray

# Copy assets
$StagingAssetsDir = Join-Path $StagingDir "Assets"
New-Item -ItemType Directory -Force -Path $StagingAssetsDir | Out-Null
if (Test-Path $AssetsDir) {
    Copy-Item "$AssetsDir\*" $StagingAssetsDir -Force
    Write-Host "  Copied assets" -ForegroundColor Gray
} else {
    Write-Host "  Warning: Assets directory not found. Using placeholder assets." -ForegroundColor Yellow
    # Create placeholder assets
    & {
        $placeholders = @(
            "Square44x44Logo.png",
            "Square150x150Logo.png",
            "Wide310x150Logo.png",
            "StoreLogo.png"
        )
        foreach ($placeholder in $placeholders) {
            "[Placeholder: $placeholder]" | Out-File (Join-Path $StagingAssetsDir $placeholder)
        }
    }
}

# Copy install helper (if exists)
$InstallHelperSrc = Join-Path $MsixDir "install-helper\install-helper.exe"
if (Test-Path $InstallHelperSrc) {
    Copy-Item $InstallHelperSrc $StagingDir -Force
    Write-Host "  Copied install helper" -ForegroundColor Gray
}

# Step 3: Create MSIX package
Write-Host "`n[3/5] Creating MSIX package..." -ForegroundColor Green

if (-not (Test-Path $MakeAppx)) {
    Write-Error "makeappx.exe not found at: $MakeAppx`nPlease install Windows SDK or update the path."
}

& $MakeAppx pack /d $StagingDir /p $MsixFile /o
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create MSIX package"
}

Write-Host "  Package created: $MsixFile" -ForegroundColor Gray

# Step 4: Sign the package (optional)
if ($Sign) {
    Write-Host "`n[4/5] Signing MSIX package..." -ForegroundColor Green

    if (-not (Test-Path $SignTool)) {
        Write-Error "signtool.exe not found at: $SignTool`nPlease install Windows SDK or update the path."
    }

    # Find or create certificate
    if (-not $CertThumbprint) {
        Write-Host "  Looking for DexCorral signing certificate..." -ForegroundColor Gray
        $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*DexCorral*" } | Select-Object -First 1

        if (-not $cert) {
            Write-Host "  Creating self-signed certificate for testing..." -ForegroundColor Yellow
            $cert = New-SelfSignedCertificate `
                -Type Custom `
                -Subject "CN=DexCorral Publisher" `
                -KeyUsage DigitalSignature `
                -FriendlyName "DexCorral Code Signing Certificate" `
                -CertStoreLocation "Cert:\CurrentUser\My" `
                -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

            Write-Host "  Certificate created: $($cert.Thumbprint)" -ForegroundColor Gray
            Write-Host "  NOTE: For distribution, you need a trusted certificate from a CA" -ForegroundColor Yellow
        }

        $CertThumbprint = $cert.Thumbprint
    }

    & $SignTool sign /fd SHA256 /sha1 $CertThumbprint /f $MsixFile
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to sign package. You may need to install the certificate or use a different one."
    } else {
        Write-Host "  Package signed successfully" -ForegroundColor Gray
    }
} else {
    Write-Host "`n[4/5] Skipping signing (use -Sign to sign)" -ForegroundColor Yellow
}

# Step 5: Install (optional)
if ($Install) {
    Write-Host "`n[5/5] Installing MSIX package..." -ForegroundColor Green

    # Stop running instances first
    Stop-Process -Name DexCorral -Force -ErrorAction SilentlyContinue
    Stop-Process -Name "DexCorral.Watchdog" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1

    Add-AppxPackage -Path $MsixFile -ForceUpdateFromAnyVersion

    if ($?) {
        Write-Host "  Installation successful!" -ForegroundColor Gray
    } else {
        Write-Error "Installation failed"
    }
} else {
    Write-Host "`n[5/5] Skipping installation (use -Install to install)" -ForegroundColor Yellow
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Cyan
Write-Host "Package: $MsixFile" -ForegroundColor White
Write-Host "`nTo install: Add-AppxPackage -Path '$MsixFile'" -ForegroundColor Gray
Write-Host "To uninstall: Get-AppxPackage DexCorral | Remove-AppxPackage" -ForegroundColor Gray
