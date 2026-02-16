@echo off
taskkill /F /IM explorer.exe
timeout /t 2 /nobreak >nul
cd /d "c:\SourceControlGh\DexCorralCpp\DexCorral"
call build.ps1 -Command powershell
timeout /t 1 /nobreak >nul
start explorer.exe
