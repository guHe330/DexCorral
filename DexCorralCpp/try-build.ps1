# Diagnostic build script
Write-Host "=== Diagnostic Build ===" -ForegroundColor Cyan

# Check cl.exe
Write-Host "`nLooking for cl.exe..." -ForegroundColor Yellow
$clPaths = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC" -Recurse -Filter "cl.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
foreach ($p in $clPaths) {
    Write-Host "  Found: $p" -ForegroundColor Green
}

# Check cmake version
Write-Host "`nCMake version:" -ForegroundColor Yellow
cmake --version | Select-Object -First 1

# Clean build dir
$buildDir = Join-Path $PSScriptRoot "build"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory $buildDir | Out-Null

# Try cmake with explicit toolset
Write-Host "`nRunning CMake..." -ForegroundColor Yellow
Push-Location $buildDir

# First try: standard generator
Write-Host "  Attempt 1: Standard generator" -ForegroundColor Gray
$result = cmake .. -G "Visual Studio 17 2022" -A x64 2>&1
$exitCode = $LASTEXITCODE

if ($exitCode -ne 0) {
    Write-Host "  Failed with standard generator" -ForegroundColor Red

    # Show error
    $result | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }

    # Clean and try with Ninja + vcvarsall
    Remove-Item -Recurse -Force $buildDir\* -ErrorAction SilentlyContinue

    Write-Host "`n  Attempt 2: Using vcvarsall + Ninja" -ForegroundColor Gray
    $vcvarsall = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

    if (Test-Path $vcvarsall) {
        Write-Host "  Found vcvarsall.bat" -ForegroundColor Green

        # Use cmd to call vcvarsall and then cmake
        $cmdScript = @"
call "$vcvarsall" x64
cd "$buildDir"
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release 2>&1
if %ERRORLEVEL% EQU 0 (
    cmake --build . 2>&1
)
"@
        cmd /c $cmdScript
    } else {
        Write-Host "  vcvarsall.bat not found!" -ForegroundColor Red
    }
} else {
    Write-Host "  CMake configuration succeeded!" -ForegroundColor Green
    Write-Host "`nBuilding..." -ForegroundColor Yellow
    cmake --build . --config Release 2>&1
}

Pop-Location
Write-Host "`n=== Done ===" -ForegroundColor Cyan
