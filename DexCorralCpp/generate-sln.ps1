# Generate Visual Studio 2022 Solution File
# This creates a .sln file in the vs2022/ directory for development in Visual Studio

param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Find Visual Studio installation
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
} else {
    Write-Host "ERROR: vswhere.exe not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio 2022 with 'Desktop development with C++' workload." -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio: $vsPath" -ForegroundColor Green

$sourceDir = $PSScriptRoot
$slnDir = Join-Path $PSScriptRoot "vs2022"

# Clean if requested
if ($Clean -and (Test-Path $slnDir)) {
    Write-Host "Cleaning solution directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $slnDir
}

# Create solution directory
if (!(Test-Path $slnDir)) {
    New-Item -ItemType Directory -Path $slnDir | Out-Null
}

Write-Host "Generating Visual Studio 2022 solution..." -ForegroundColor Green
Write-Host "Output directory: $slnDir" -ForegroundColor Cyan

# Generate Visual Studio solution
Push-Location $slnDir
try {
    cmake "$sourceDir" -G "Visual Studio 17 2022" -A x64
    $exitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Solution generated successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Solution file: $slnDir\DexCorral.sln" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Open in Visual Studio:" -ForegroundColor Yellow
    Write-Host "  start $slnDir\DexCorral.sln" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Note: The vs2022/ directory is for IDE development only." -ForegroundColor Yellow
    Write-Host "      Use build.ps1 for regular builds (Ninja/NMake)." -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "Solution generation failed with exit code $exitCode" -ForegroundColor Red
}

exit $exitCode
