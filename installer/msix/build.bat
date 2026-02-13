@echo off
REM Quick build wrapper for build-msix.ps1

echo Building DexCorral MSIX installer...
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0build-msix.ps1" %*

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build completed successfully!
) else (
    echo.
    echo Build failed!
    exit /b 1
)
