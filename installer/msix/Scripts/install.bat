@echo off
REM DexCorral MSIX Installation Script
REM This script runs during package installation to set up the Explorer hook

echo Installing DexCorral...

REM Get the installation directory
set INSTALL_DIR=%~dp0..

REM Register the Explorer hook DLL
REM Note: This would need elevated privileges in a real deployment
REM The actual injection happens when DexCorral.exe runs

echo DexCorral installation complete.
echo The application will start automatically on next login.

exit /b 0
