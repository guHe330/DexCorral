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
; Suppress the per-user areas warning (user config may be deleted on uninstall if user chooses)
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

[Registry]
; Start DexCorral at login by injecting into Explorer (brief EXE, then exits).
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "DexCorral"; \
  ValueData: """{app}\DexCorral.exe"" --startup --silent"; \
  Flags: uninsdeletevalue

[UninstallRun]
; Unregister the shell extension (silent)
Filename: "{app}\DexCorral.exe"; Parameters: "--unregister --silent"; RunOnceId: "UnregisterHook"; Flags: runhidden waituntilterminated
; Restart Explorer to unload the DLL before file removal
Filename: "{cmd}"; Parameters: "/c taskkill /F /IM explorer.exe & timeout /t 3 /nobreak & start explorer.exe"; RunOnceId: "RestartExplorer"; Flags: runhidden waituntilterminated

[UninstallDelete]
; Config directory is handled conditionally in [Code] (user is asked whether to keep it)

[Code]

// Whether the user chose to keep their config during uninstall
var
  g_KeepConfig: Boolean;

// Helper: convert Boolean to string (InnoSetup Pascal lacks a built-in BoolToStr)
function BoolToStr(Value: Boolean): String;
begin
  if Value then Result := 'True' else Result := 'False';
end;

// Win32 imports used for install-time polling and shell notification
function FindWindowW(ClassName: String; WindowName: Integer): Integer;
  external 'FindWindowW@user32.dll stdcall';

// Poll for a window by class name (returns True as soon as it appears)
function WaitForWindow(ClassName: String; MaxRetries: Integer; SleepMs: Integer): Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 0 to MaxRetries - 1 do begin
    if FindWindowW(ClassName, 0) <> 0 then begin
      Result := True;
      Exit;
    end;
    Sleep(SleepMs);
  end;
end;

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
    Exec(ExpandConstant('{win}\explorer.exe'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Sleep(2000);
  end;
end;

// After install: register the shell extension and restart Explorer.
// Done in [Code] instead of [Run] so it executes before the finish page
// and we can report errors properly.
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  AppFound: Boolean;
begin
  if CurStep = ssPostInstall then begin
    Log('--- DexCorral post-install sequence ---');
    Log('Install dir: ' + ExpandConstant('{app}'));
    Log('AppData dir: ' + ExpandConstant('{userappdata}'));

    // Register the shell extension
    Log('Step 1: Registering shell extension via DexCorral.exe --register...');
    if Exec(ExpandConstant('{app}\DexCorral.exe'), '--register --silent',
            ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      Log('Step 1 result: DexCorral.exe --register exited with code=' + IntToStr(ResultCode));
      if ResultCode <> 0 then begin
        Log('Step 1 ERROR: registration failed. Check that DLL exists and regsvr32 succeeded.');
        MsgBox('Shell extension registration failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 +
               'Try running "DexCorral.exe --register" manually as Administrator.', mbError, MB_OK);
      end;
    end else begin
      Log('Step 1 ERROR: Failed to launch DexCorral.exe --register (exe missing?)');
      MsgBox('Could not run DexCorral.exe to register the shell extension.' + #13#10 +
             'Try running "DexCorral.exe --register" manually as Administrator.', mbError, MB_OK);
    end;

    // Inject DexCorralHook.dll into the running Explorer via WH_GETMESSAGE hook.
    // DexCorral.exe --startup finds Explorer's Progman thread, injects the DLL,
    // waits ~1 s for the App to start inside Explorer, then exits.
    // No Explorer restart needed — the existing session is used.
    Log('Step 2: Running DexCorral.exe --startup to inject into Explorer...');
    if Exec(ExpandConstant('{app}\DexCorral.exe'), '--startup --silent',
            ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Log('Step 2: --startup exited with code=' + IntToStr(ResultCode))
    else
      Log('Step 2 ERROR: Failed to launch DexCorral.exe --startup');

    // Verify the App is running (its hidden message window should now exist)
    AppFound := WaitForWindow('DexCorralMessageWindow', 10, 300);
    Log('Step 3: DexCorralMessageWindow found=' + BoolToStr(AppFound));
    if not AppFound then begin
      Log('Step 3: App not detected — user can right-click desktop to activate manually.');
      Log('  -> Startup injection will retry automatically on next login via Run key.');
    end else
      Log('Step 3: SUCCESS — DexCorral App is running');

    // Copy the InnoSetup log to the DexCorral AppData folder for easy access
    Log('Step 4: Copying setup log to AppData...');
    if FileCopy(ExpandConstant('{log}'),
                ExpandConstant('{userappdata}\DexCorral\install.log'), False) then
      Log('Step 4: Setup log copied to ' + ExpandConstant('{userappdata}\DexCorral\install.log'))
    else
      Log('Step 4: Could not copy setup log (AppData folder may not exist yet)');

    Log('--- DexCorral post-install sequence complete ---');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then begin
    Exec('taskkill', '/F /IM DexCorral.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(500);

    // Ask whether to keep the user's configuration files
    g_KeepConfig := MsgBox(
      'Would you like to keep your DexCorral configuration?' + #13#10 + #13#10 +
      'Your corral layouts and appearance settings are stored in:' + #13#10 +
      '  ' + ExpandConstant('{userappdata}') + '\DexCorral' + #13#10 + #13#10 +
      'Click Yes to keep them for future use, or No to delete everything.',
      mbConfirmation, MB_YESNO) = IDYES;
  end;

  if CurUninstallStep = usPostUninstall then begin
    if not g_KeepConfig then begin
      DelTree(ExpandConstant('{localappdata}\DexCorral'), True, True, True);
      DelTree(ExpandConstant('{userappdata}\DexCorral'), True, True, True);
    end;
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
