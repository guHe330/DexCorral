# DexCorral Build Script (PowerShell)
# Builds with static runtime linking (no vcredist needed)

param(
    [switch]$Clean,
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

# Kill any running instances (watchdog first to prevent auto-relaunch)
Write-Host "Stopping any running DexCorral instances..." -ForegroundColor Yellow
$watchdogs = Get-Process -Name "DexCorral.Watchdog" -ErrorAction SilentlyContinue
if ($watchdogs) {
    $watchdogs | Stop-Process -Force
    Write-Host "  Killed $($watchdogs.Count) watchdog instance(s)" -ForegroundColor Yellow
    Start-Sleep -Milliseconds 500
}
$processes = Get-Process -Name "DexCorral" -ErrorAction SilentlyContinue
if ($processes) {
    $processes | Stop-Process -Force
    Write-Host "  Killed $($processes.Count) instance(s)" -ForegroundColor Yellow
    Start-Sleep -Milliseconds 500
} else {
    Write-Host "  No running instances found" -ForegroundColor Gray
}

# Check if DexCorralHook.dll is locked by Explorer (shell extension mode)
$buildDir = Join-Path $PSScriptRoot "build"
$hookDll = Join-Path $buildDir "DexCorralHook.dll"
if (Test-Path $hookDll) {
    try {
        [IO.File]::OpenWrite($hookDll).Close()
    } catch {
        Write-Host "  DexCorralHook.dll is locked (loaded by Explorer as shell extension)" -ForegroundColor Yellow
        Write-Host "  Restarting Explorer to unlock DLL..." -ForegroundColor Yellow
        Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
        Start-Process explorer.exe
        Start-Sleep -Seconds 3
    }
}

# Find Visual Studio installation
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
} else {
    # Fallback to common paths
    $vcvarsall = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    if (!(Test-Path $vcvarsall)) {
        $vcvarsall = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    }
    if (!(Test-Path $vcvarsall)) {
        $vcvarsall = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    }
}

if (!(Test-Path $vcvarsall)) {
    Write-Host "ERROR: vcvarsall.bat not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio 2022 with 'Desktop development with C++' workload." -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio: $vcvarsall" -ForegroundColor Green

$sourceDir = $PSScriptRoot

# Clean if requested
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# Create build directory
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Create temporary batch file for build
$tempBat = Join-Path $env:TEMP "dexcorral_build_$([System.Guid]::NewGuid().ToString('N')).bat"

$batContent = @"
@echo off
call "$vcvarsall" x64
if %ERRORLEVEL% neq 0 (
    echo Failed to initialize Visual Studio environment
    exit /b 1
)

cd /d "$buildDir"

where ninja >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "GENERATOR=Ninja"
) else (
    set "GENERATOR=NMake Makefiles"
)

echo Using generator: %GENERATOR%
echo.

echo Running CMake configuration...
cmake "$sourceDir" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=$BuildType
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed
    exit /b %ERRORLEVEL%
)

echo.
echo Building project...
cmake --build . --config $BuildType
exit /b %ERRORLEVEL%
"@

Set-Content -Path $tempBat -Value $batContent -Encoding ASCII

Write-Host "Building DexCorral ($BuildType)..." -ForegroundColor Green

try {
    cmd /c $tempBat
    $exitCode = $LASTEXITCODE
} finally {
    # Clean up temp file
    Remove-Item -Path $tempBat -ErrorAction SilentlyContinue
}

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Shell Extension: $buildDir\DexCorralHook.dll" -ForegroundColor Green
    Write-Host "Registration:    $buildDir\DexCorral.exe" -ForegroundColor Green
    Write-Host "Watchdog:        $buildDir\DexCorral.Watchdog.exe" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "To install:   DexCorral.exe --register  (run as Admin)" -ForegroundColor Cyan
    Write-Host "To uninstall: DexCorral.exe --unregister" -ForegroundColor Cyan

    # Create release zip
    $zipPath = Join-Path $buildDir "DexCorral.zip"
    $filesToZip = @(
        (Join-Path $buildDir "DexCorralHook.dll"),
        (Join-Path $buildDir "DexCorral.exe"),
        (Join-Path $buildDir "DexCorral.Watchdog.exe")
    ) | Where-Object { Test-Path $_ }

    if ($filesToZip.Count -gt 0) {
        if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
        Compress-Archive -Path $filesToZip -DestinationPath $zipPath -CompressionLevel Optimal
        Write-Host ""
        Write-Host "Release zip: $zipPath" -ForegroundColor Cyan
    }
} else {
    Write-Host ""
    Write-Host "Build failed with exit code $exitCode" -ForegroundColor Red
}

exit $exitCode
