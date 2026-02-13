@echo off
REM DexCorral MSIX Uninstallation Script
REM This script runs during package removal to clean up

echo Uninstalling DexCorral...

REM Kill running processes
taskkill /F /IM DexCorral.exe 2>nul
taskkill /F /IM DexCorral.Watchdog.exe 2>nul

REM Wait for processes to exit
timeout /t 2 /nobreak >nul

REM Unhook from Explorer
REM The hook DLL will be automatically unloaded when Explorer is restarted
REM or when the DLL file is removed

echo DexCorral uninstallation complete.
echo You may need to restart Explorer to fully remove the desktop hook.

exit /b 0
