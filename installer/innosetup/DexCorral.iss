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
#define MyAppURL "https://github.com/guHe330/DexCorral"

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
; Small branding icon shown in the top-right corner of every wizard page.
; (SetupIconFile is intentionally left unset so Setup.exe/Uninstall.exe keep
; Inno Setup's default icon.)
WizardSmallImageFile=resources\DexCorral-64.png,resources\DexCorral-128.png,resources\DexCorral-256.png
; Uninstall
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\DexCorral.exe
; Minimum Windows version (Windows 11 21H2) -- DexCorral targets Windows 11 only
MinVersion=10.0.22000
; Allow upgrading over existing install without asking
UsePreviousAppDir=yes
; Ask for the language up front (also selects the app's UI language, see [Registry])
ShowLanguageDialog=yes
UsePreviousLanguage=no
CloseApplications=no
; Suppress the per-user areas warning (user config may be deleted on uninstall if user chooses)
UsedUserAreasWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
; Main binaries
Source: "..\..\DexCorral\build\DexCorral.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\DexCorral\build\DexCorralHook.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete
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
; UI language chosen in the installer's language dialog. The app uses this as
; the default; config.json's "Language" (in-app setting) overrides it.
Root: HKCU; Subkey: "Software\DexCorral"; \
  ValueType: string; ValueName: "Language"; \
  ValueData: "{code:GetLangCode}"; \
  Flags: uninsdeletevalue

[UninstallRun]
; Unregister the shell extension (silent)
Filename: "{app}\DexCorral.exe"; Parameters: "--unregister --silent"; RunOnceId: "UnregisterHook"; Flags: runhidden waituntilterminated
; Restart Explorer to unload the DLL before file removal
Filename: "{cmd}"; Parameters: "/c taskkill /F /IM explorer.exe & timeout /t 3 /nobreak & start explorer.exe"; RunOnceId: "RestartExplorer"; Flags: runhidden waituntilterminated

[UninstallDelete]
; Renamed-aside DLLs left by past upgrades (see RenameAsideLockedDll in [Code])
Type: files; Name: "{app}\DexCorralHook.dll.old*"
; Config directory is handled conditionally in [Code] (user is asked whether to keep it)

[Code]

// Whether the user chose to keep their config during uninstall
var
  g_KeepConfig: Boolean;
// Set in PrepareToInstall when an older install was found (drives the Explorer restart)
  g_IsUpgrade: Boolean;

// Maps the installer language to the app's language code (see [Registry])
function GetLangCode(Param: String): String;
begin
  if ActiveLanguage = 'german' then
    Result := 'de'
  else
    Result := 'en';
end;

// Helper: convert Boolean to string (InnoSetup Pascal lacks a built-in BoolToStr)
function BoolToStr(Value: Boolean): String;
begin
  if Value then Result := 'True' else Result := 'False';
end;

// Name of the shell extension DLL (renamed aside during upgrades, see below)
const
  HookDll = 'DexCorralHook.dll';
  DexCorralClsid = '{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}';

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

// Poll until a window is gone (taskkill returns before the process really dies)
function WaitForWindowGone(ClassName: String; MaxRetries: Integer; SleepMs: Integer): Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 0 to MaxRetries - 1 do begin
    if FindWindowW(ClassName, 0) = 0 then begin
      Result := True;
      Exit;
    end;
    Sleep(SleepMs);
  end;
end;

// Delete DLLs renamed aside for this or an earlier upgrade. A file whose image
// section is still mapped cannot be deleted, so each one is retried for a
// moment (Retries x 250 ms) before being queued for deletion at the next
// reboot. Callers pass a short retry budget while the old Explorer still holds
// the DLL, and a longer one after the restart, when the delete should succeed.
procedure CleanupRenamedDlls(Dir: String; Retries: Integer);
var
  FindRec: TFindRec;
  Path: String;
  i: Integer;
  Deleted: Boolean;
begin
  if not FindFirst(Dir + '\' + HookDll + '.old*', FindRec) then
    Exit;
  try
    repeat
      Path := Dir + '\' + FindRec.Name;
      Deleted := False;
      for i := 0 to Retries - 1 do begin
        Deleted := DeleteFile(Path);
        if Deleted or (i = Retries - 1) then
          Break;
        Sleep(250);
      end;
      if Deleted then
        Log('Cleanup: removed ' + FindRec.Name)
      else begin
        Log('Cleanup: ' + FindRec.Name + ' still mapped - queued for deletion at reboot');
        RestartReplace(Path, '');
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

// Rename the installed hook DLL out of the way so the new copy lands on a free
// name. Windows allows renaming a file with a mapped image section (loaded DLLs
// are opened with FILE_SHARE_DELETE), so this succeeds even while Explorer, a
// COM surrogate or AV holds it -- the same trick browsers use to self-update.
function RenameAsideHookDll(Dir: String): Boolean;
var
  Src, Dest: String;
  i: Integer;
begin
  Src := Dir + '\' + HookDll;
  Result := True;
  if not FileExists(Src) then
    Exit;

  for i := 0 to 99 do begin
    if i = 0 then
      Dest := Src + '.old'
    else
      Dest := Src + '.old' + IntToStr(i);
    if not FileExists(Dest) then begin
      Result := RenameFile(Src, Dest);
      if Result then
        Log('Renamed old ' + HookDll + ' to ' + Dest)
      else
        Log('WARNING: could not rename old ' + HookDll + ' - relying on restartreplace');
      Exit;
    end;
  end;
  Log('WARNING: no free .old name for ' + HookDll);
  Result := False;
end;

// Fallback when --unregister fails (missing or mismatched old exe): strip the
// registration by hand so a restarted Explorer cannot load the old DLL back in.
// Mirrors UnregisterShellExtension() in ShellExtension/src/Registration.cpp.
procedure ForceUnregisterShellExtension;
begin
  RegDeleteKeyIncludingSubkeys(HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ DexCorral');
  RegDeleteKeyIncludingSubkeys(HKCR64,
    'Directory\Background\ShellEx\ContextMenuHandlers\DexCorral');
  RegDeleteValue(HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler', DexCorralClsid);
  RegDeleteKeyIncludingSubkeys(HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler\' + DexCorralClsid);
  RegDeleteValue(HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellServiceObjectDelayLoad', 'DexCorral');
  RegDeleteKeyIncludingSubkeys(HKCR64, 'CLSID\' + DexCorralClsid);
end;

// Kill Explorer so the old DLL is unmapped, then wait for the shell to return.
// Winlogon's AutoRestartShell normally relaunches it within a second or two; we
// only start it ourselves if it stays down, which avoids two Explorers.
procedure RestartExplorer;
var
  ResultCode: Integer;
begin
  Exec('taskkill', '/F /IM explorer.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  WaitForWindowGone('Progman', 40, 250);   // up to 10 s
  if not WaitForWindow('Progman', 20, 500) then begin
    Log('Shell did not auto-restart - launching explorer.exe');
    Exec(ExpandConstant('{win}\explorer.exe'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    WaitForWindow('Progman', 40, 500);
  end;
end;

// Upgrade support: unregister the old shell extension and rename its DLL aside
// so the copy lands on a free name. Explorer keeps running here on purpose --
// restarting it before the copy just gives a fresh Explorer a window to map the
// DLL again; it is restarted after the files are in place (see CurStepChanged).
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  AppDir, ExePath: String;
begin
  Result := '';
  NeedsRestart := False;

  AppDir := ExpandConstant('{app}');
  ExePath := AppDir + '\DexCorral.exe';
  if not FileExists(ExePath) then
    Exit;  // fresh install, nothing to displace

  g_IsUpgrade := True;
  Log('Existing install detected - preparing for upgrade');

  // A leftover --startup/--register process holds a LoadLibrary reference on the DLL
  Exec('taskkill', '/F /IM DexCorral.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(500);

  if Exec(ExePath, '--unregister --silent', '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
     and (ResultCode = 0) then
    Log('Unregistered old shell extension')
  else begin
    Log('Unregister failed (exit code ' + IntToStr(ResultCode) + ') - removing keys directly');
    ForceUnregisterShellExtension;
  end;

  CleanupRenamedDlls(AppDir, 1);   // leftovers only; today's DLL is still loaded
  RenameAsideHookDll(AppDir);
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

    // Restart Explorer now that the new files are in place. On an upgrade the
    // old DLL is still mapped (under its .old name), so the session has to be
    // recycled before the new one is injected. A fresh install has nothing
    // loaded, so it is left alone.
    if g_IsUpgrade then begin
      Log('Step 2: Restarting Explorer to unload the old DLL...');
      RestartExplorer;
    end else
      Log('Step 2: Fresh install - no Explorer restart needed');

    // Inject DexCorralHook.dll into the running Explorer via WH_GETMESSAGE hook.
    // DexCorral.exe --startup finds Explorer's Progman thread, injects the DLL,
    // waits ~1 s for the App to start inside Explorer, then exits.
    Log('Step 3: Running DexCorral.exe --startup to inject into Explorer...');
    if Exec(ExpandConstant('{app}\DexCorral.exe'), '--startup --silent',
            ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Log('Step 3: --startup exited with code=' + IntToStr(ResultCode))
    else
      Log('Step 3 ERROR: Failed to launch DexCorral.exe --startup');

    // Verify the App is running (its hidden message window should now exist)
    AppFound := WaitForWindow('DexCorralMessageWindow', 10, 300);
    Log('Step 4: DexCorralMessageWindow found=' + BoolToStr(AppFound));
    if not AppFound then begin
      Log('Step 4: App not detected — user can right-click desktop to activate manually.');
      Log('  -> Startup injection will retry automatically on next login via Run key.');
    end else
      Log('Step 4: SUCCESS — DexCorral App is running');

    // With the old Explorer gone its image section is unmapped, so the DLL
    // renamed aside in PrepareToInstall can usually be deleted now. Done last so
    // the retry budget does not delay anything the user is waiting on. A copy
    // still mapped by another shell host (dllhost, the preview/thumbnail host,
    // an open file dialog) falls back to deletion at the next reboot.
    if g_IsUpgrade then begin
      Log('Step 5: Removing the DLL renamed aside for this upgrade...');
      CleanupRenamedDlls(ExpandConstant('{app}'), 20);   // up to ~5 s
    end;

    // Copy the InnoSetup log to the DexCorral AppData folder for easy access
    Log('Step 6: Copying setup log to AppData...');
    if CopyFile(ExpandConstant('{log}'),
                ExpandConstant('{userappdata}\DexCorral\install.log'), False) then
      Log('Step 6: Setup log copied to ' + ExpandConstant('{userappdata}\DexCorral\install.log'))
    else
      Log('Step 6: Could not copy setup log (AppData folder may not exist yet)');

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
    // (ActiveLanguage returns the language chosen at install time)
    if ActiveLanguage = 'german' then
      g_KeepConfig := MsgBox(
        'Möchten Sie Ihre DexCorral-Konfiguration behalten?' + #13#10 + #13#10 +
        'Ihre Corral-Layouts und Darstellungseinstellungen sind gespeichert unter:' + #13#10 +
        '  ' + ExpandConstant('{userappdata}') + '\DexCorral' + #13#10 + #13#10 +
        'Klicken Sie auf Ja, um sie zu behalten, oder auf Nein, um alles zu löschen.',
        mbConfirmation, MB_YESNO) = IDYES
    else
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
