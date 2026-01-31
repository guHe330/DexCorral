# Build Environment Check Script for DexCorral
# Run this to verify all required tools are installed

Write-Host "`n=== DexCorral Build Environment Check ===" -ForegroundColor Cyan
Write-Host "Checking required tools for building DexCorral...`n" -ForegroundColor Gray

$allGood = $true

# Check CMake
Write-Host "[1/4] Checking CMake..." -ForegroundColor Yellow
try {
    $cmakeVersionOutput = cmake --version 2>&1 | Select-Object -First 1
    if ($cmakeVersionOutput -match "cmake version (\d+\.\d+\.\d+)") {
        $cmakeVersion = $matches[1]
        $versionParts = $cmakeVersion.Split('.')
        $major = [int]$versionParts[0]
        $minor = [int]$versionParts[1]

        if ($major -gt 3 -or ($major -eq 3 -and $minor -ge 15)) {
            Write-Host "      ✓ CMake $cmakeVersion (minimum 3.15 required)" -ForegroundColor Green
        } else {
            Write-Host "      ✗ CMake $cmakeVersion is too old (minimum 3.15 required)" -ForegroundColor Red
            Write-Host "        Install: winget install Kitware.CMake" -ForegroundColor Gray
            $allGood = $false
        }
    }
} catch {
    Write-Host "      ✗ CMake not found" -ForegroundColor Red
    Write-Host "        Install: winget install Kitware.CMake" -ForegroundColor Gray
    $allGood = $false
}

# Check Visual Studio
Write-Host "`n[2/4] Checking Visual Studio..." -ForegroundColor Yellow
$vsFound = $false
$vsVersions = @("2022", "2019")
$vsEditions = @("Enterprise", "Professional", "Community", "BuildTools")

foreach ($version in $vsVersions) {
    foreach ($edition in $vsEditions) {
        $vsPath = "C:\Program Files\Microsoft Visual Studio\$version\$edition"
        if (Test-Path $vsPath) {
            # Check for C++ tools
            $vcToolsPath = "$vsPath\VC\Tools\MSVC"
            if (Test-Path $vcToolsPath) {
                Write-Host "      ✓ Visual Studio $version $edition with C++ tools" -ForegroundColor Green
                $vsFound = $true
                break
            }
        }
    }
    if ($vsFound) { break }
}

if (-not $vsFound) {
    Write-Host "      ✗ Visual Studio 2019/2022 with C++ tools not found" -ForegroundColor Red
    Write-Host "        Download: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Gray
    Write-Host "        Required: Desktop development with C++ workload" -ForegroundColor Gray
    $allGood = $false
}

# Check Git (optional but recommended)
Write-Host "`n[3/4] Checking Git (optional)..." -ForegroundColor Yellow
try {
    $gitVersion = git --version 2>&1
    Write-Host "      ✓ $gitVersion" -ForegroundColor Green
} catch {
    Write-Host "      ⚠ Git not found (optional)" -ForegroundColor DarkYellow
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
        Write-Host "      ✓ Windows 10 SDK $($sdkVersions.Name)" -ForegroundColor Green
    } else {
        Write-Host "      ⚠ Windows SDK found but version unclear" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "      ⚠ Windows SDK not found at standard location" -ForegroundColor DarkYellow
    Write-Host "        (Usually installed with Visual Studio)" -ForegroundColor Gray
}

# Check PowerShell version
Write-Host "`n[Info] PowerShell Version: $($PSVersionTable.PSVersion)" -ForegroundColor Gray

# Summary
Write-Host "`n========================================" -ForegroundColor Cyan
if ($allGood) {
    Write-Host "✓ All required tools are installed!" -ForegroundColor Green
    Write-Host "`nYou can now build DexCorral:" -ForegroundColor White
    Write-Host "  .\build.ps1" -ForegroundColor Cyan
    Write-Host "`nOr manually:" -ForegroundColor White
    Write-Host "  mkdir build" -ForegroundColor Gray
    Write-Host "  cd build" -ForegroundColor Gray
    Write-Host "  cmake .. -G ""Visual Studio 17 2022"" -A x64" -ForegroundColor Gray
    Write-Host "  cmake --build . --config Release" -ForegroundColor Gray
} else {
    Write-Host "✗ Some required tools are missing" -ForegroundColor Red
    Write-Host "`nPlease install missing tools and run this script again." -ForegroundColor Yellow
    Write-Host "`nQuick install commands:" -ForegroundColor White
    Write-Host "  winget install Kitware.CMake" -ForegroundColor Gray
    Write-Host "  winget install Microsoft.VisualStudio.2022.Community --override ""--add Microsoft.VisualStudio.Workload.NativeDesktop""" -ForegroundColor Gray
}
Write-Host "========================================`n" -ForegroundColor Cyan
