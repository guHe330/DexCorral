; DexCorral Inno Setup Installer Script
; Requires Inno Setup 6+ (https://jrsoftware.org/isinfo.php)
;
; Version can be overridden from the command line:
;   iscc /DMyAppVersion=1.2.3 DexCorral.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

#define MyAppName "DexCorral"
#define MyAppPublisher "DexCorral"
#define MyAppURL "https://github.com/guHe330/DexCorralCpp"

[Setup]
AppId={{E4A7B2C1-3D5F-4E8A-9B1C-6F2D8E0A4C7B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; Show license agreement page
LicenseFile=..\..\LICENSE
OutputDir=output
OutputBaseFilename=DexCorral_{#MyAppVersion}_Setup
Compression=lzma2
SolidCompression=yes
; Require admin -- needed for COM registration
PrivilegesRequired=admin
; 64-bit only
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installer appearance
WizardStyle=modern
; Uninstall
UninstallDisplayName={#MyAppName}
; Minimum Windows version (Windows 10 1809)
MinVersion=10.0.17763
; Allow upgrading over existing install without asking
UsePreviousAppDir=yes
CloseApplications=no
; Suppress the per-user areas warning (we only delete user config on uninstall)
UsedUserAreasWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Main binaries
Source: "..\..\DexCorral\build\DexCorral.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\DexCorral\build\DexCorralHook.dll"; DestDir: "{app}"; Flags: ignoreversion
; License
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName} - Register"; Filename: "{app}\DexCorral.exe"; Parameters: "--register"
Name: "{group}\{#MyAppName} - Unregister"; Filename: "{app}\DexCorral.exe"; Parameters: "--unregister"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallRun]
; Unregister the shell extension (silent)
Filename: "{app}\DexCorral.exe"; Parameters: "--unregister --silent"; RunOnceId: "UnregisterHook"; Flags: runhidden waituntilterminated
; Restart Explorer to unload the DLL before file removal
Filename: "{cmd}"; Parameters: "/c taskkill /F /IM explorer.exe & timeout /t 3 /nobreak & start explorer.exe"; RunOnceId: "RestartExplorer"; Flags: runhidden waituntilterminated

[UninstallDelete]
; Clean up config directory
Type: filesandordirs; Name: "{localappdata}\DexCorral"
Type: filesandordirs; Name: "{userappdata}\DexCorral"

[Code]

// Upgrade support: unregister old shell extension and restart Explorer
// before copying new files, so the DLL is not locked by Explorer.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  ExePath: String;
begin
  Result := '';
  NeedsRestart := False;

  ExePath := ExpandConstant('{app}\DexCorral.exe');

  // If upgrading over an existing install, unregister the old DLL first
  if FileExists(ExePath) then begin
    Log('Existing install detected - unregistering old shell extension');
    Exec(ExePath, '--unregister --silent', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Log('Unregister exit code: ' + IntToStr(ResultCode));
    Sleep(500);

    // Restart Explorer to unload the old DLL so files can be overwritten
    Log('Restarting Explorer to release old DLL');
    Exec('taskkill', '/F /IM explorer.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(3000);
    Exec(ExpandConstant('{sys}\explorer.exe'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Sleep(2000);
  end;
end;

// After install: register the shell extension and restart Explorer.
// Done in [Code] instead of [Run] so it executes before the finish page
// and we can report errors properly.
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then begin
    // Register the shell extension
    Log('Registering shell extension...');
    if Exec(ExpandConstant('{app}\DexCorral.exe'), '--register --silent',
            ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      Log('DexCorral.exe --register exited with code: ' + IntToStr(ResultCode));
      if ResultCode <> 0 then
        MsgBox('Shell extension registration failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 +
               'Try running "DexCorral.exe --register" manually as Administrator.', mbError, MB_OK);
    end else begin
      Log('Failed to launch DexCorral.exe --register');
      MsgBox('Could not run DexCorral.exe to register the shell extension.' + #13#10 +
             'Try running "DexCorral.exe --register" manually as Administrator.', mbError, MB_OK);
    end;

    // Restart Explorer so it loads the new DLL
    Log('Restarting Explorer...');
    Exec('taskkill', '/F /IM explorer.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(3000);
    Exec(ExpandConstant('{sys}\explorer.exe'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Sleep(2000);
    Log('Explorer restarted');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then begin
    Exec('taskkill', '/F /IM DexCorral.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(500);
  end;
end;

// Customize the finish page - no "launch application" checkbox since
// DexCorral runs as a shell extension loaded by Explorer, not a standalone app.
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = wpFinished then
    Result := False; // Show finish page, but we removed the run checkbox
end;

procedure InitializeWizard;
begin
  // Remove the "Launch application" checkbox on the finish page
  WizardForm.RunList.Visible := False;
end;
