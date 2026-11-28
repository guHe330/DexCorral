# Build Environment Check Script for DexCorral
# Run this to verify all required tools are installed

Write-Host "`n=== DexCorral Build Environment Check ===" -ForegroundColor Cyan
Write-Host "Checking required tools for building DexCorral...`n" -ForegroundColor Gray

$allGood = $true

# Check CMake (standalone on PATH, or bundled with Visual Studio)
Write-Host "[1/4] Checking CMake..." -ForegroundColor Yellow
$cmakeExe = $null
$cmakeSource = ""
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

# 1. Try cmake on PATH first
try {
    $cmakeOnPath = cmake --version 2>&1 | Select-Object -First 1
    if ($cmakeOnPath -match "cmake version") {
        $cmakeExe = "cmake"
        $cmakeSource = "standalone"
    }
} catch { }

# 2. If not on PATH, look inside the Visual Studio installation
if (-not $cmakeExe) {
    if (Test-Path $vsWhere) {
        $vsInstallPath = & $vsWhere -latest -products * -property installationPath 2>$null
        if ($vsInstallPath) {
            $vsCmake = Join-Path $vsInstallPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $vsCmake) {
                $cmakeExe = $vsCmake
                $cmakeSource = "Visual Studio"
            }
        }
    }
}

if ($cmakeExe) {
    $cmakeVersionOutput = & $cmakeExe --version 2>&1 | Select-Object -First 1
    if ($cmakeVersionOutput -match "cmake version (\d+\.\d+\.\d+)") {
        $cmakeVersion = $matches[1]
        $versionParts = $cmakeVersion.Split('.')
        $major = [int]$versionParts[0]
        $minor = [int]$versionParts[1]

        if ($major -gt 3 -or ($major -eq 3 -and $minor -ge 15)) {
            Write-Host "      [OK] CMake $cmakeVersion via $cmakeSource (minimum 3.15 required)" -ForegroundColor Green
        } else {
            Write-Host "      [X] CMake $cmakeVersion is too old (minimum 3.15 required)" -ForegroundColor Red
            Write-Host "        Install: winget install Kitware.CMake" -ForegroundColor Gray
            $allGood = $false
        }
    }
} else {
    Write-Host "      [X] CMake not found (checked PATH and Visual Studio installation)" -ForegroundColor Red
    Write-Host "        Install via VS Installer: add 'C++ CMake tools for Windows' component" -ForegroundColor Gray
    Write-Host "        Or standalone: winget install Kitware.CMake" -ForegroundColor Gray
    $allGood = $false
}

# Check Visual Studio
Write-Host "`n[2/4] Checking Visual Studio..." -ForegroundColor Yellow
$vsFound = $false
# Same query as build.ps1, so both scripts agree on Build Tools, VS 2026 and
# installs outside the default folder.
if (Test-Path $vsWhere) {
    $vsName = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName 2>$null
    if ($vsName) {
        Write-Host "      [OK] $vsName with C++ tools" -ForegroundColor Green
        $vsFound = $true
    }
}

if (-not $vsFound) {
    Write-Host "      [X] Visual Studio or Build Tools with C++ tools not found" -ForegroundColor Red
    Write-Host "        Download: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Gray
    Write-Host "        Required: Desktop development with C++ workload" -ForegroundColor Gray
    $allGood = $false
}

# Check Git (optional but recommended)
Write-Host "`n[3/4] Checking Git (optional)..." -ForegroundColor Yellow
try {
    $gitVersion = git --version 2>&1
    Write-Host "      [OK] $gitVersion" -ForegroundColor Green
} catch {
    Write-Host "      [!] Git not found (optional)" -ForegroundColor DarkYellow
    Write-Host "        Install: winget install Git.Git" -ForegroundColor Gray
}

# Check Windows SDK (usually comes with Visual Studio)
Write-Host "`n[4/4] Checking Windows SDK..." -ForegroundColor Yellow
$sdkPath = "C:\Program Files (x86)\Windows Kits\10"
if (Test-Path $sdkPath) {
    $sdkVersions = Get-ChildItem "$sdkPath\Include" -Directory -ErrorAction SilentlyContinue |
                   Where-Object { $_.Name -match "^\d+\.\d+\.\d+\.\d+$" } |
                   Select-Object -Last 1
    if ($sdkVersions) {
        Write-Host "      [OK] Windows SDK $($sdkVersions.Name)" -ForegroundColor Green
    } else {
        Write-Host "      [!] Windows SDK found but version unclear" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "      [!] Windows SDK not found at standard location" -ForegroundColor DarkYellow
    Write-Host "        (Usually installed with Visual Studio)" -ForegroundColor Gray
}

# Check PowerShell version
Write-Host "`n[Info] PowerShell Version: $($PSVersionTable.PSVersion)" -ForegroundColor Gray

# Summary
Write-Host "`n========================================" -ForegroundColor Cyan
if ($allGood) {
    Write-Host "[OK] All required tools are installed!" -ForegroundColor Green
    Write-Host "`nYou can now build DexCorral:" -ForegroundColor White
    Write-Host "  .\build.ps1" -ForegroundColor Cyan
    Write-Host "`nOr manually:" -ForegroundColor White
    Write-Host "  mkdir build" -ForegroundColor Gray
    Write-Host "  cd build" -ForegroundColor Gray
    Write-Host "  cmake .. -G ""Visual Studio 17 2022"" -A x64" -ForegroundColor Gray
    Write-Host "  cmake --build . --config Release" -ForegroundColor Gray
} else {
    Write-Host "[X] Some required tools are missing" -ForegroundColor Red
    Write-Host "`nPlease install missing tools and run this script again." -ForegroundColor Yellow
    Write-Host "`nQuick install commands:" -ForegroundColor White
    Write-Host "  winget install Kitware.CMake" -ForegroundColor Gray
    Write-Host "  winget install Microsoft.VisualStudio.2022.Community --override ""--add Microsoft.VisualStudio.Workload.NativeDesktop""" -ForegroundColor Gray
}
Write-Host "========================================`n" -ForegroundColor Cyan
