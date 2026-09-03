; DexCorral Inno Setup Installer Script
; Requires Inno Setup 6+ (https://jrsoftware.org/isinfo.php)
;
; Version can be overridden from the command line:
;   iscc /DMyAppVersion=1.2.3 DexCorral.iss
;
; Two install modes, chosen on Setup's first page (see installer/INSTALLER_NOTES.md):
;   All users  -> %ProgramFiles%\DexCorral, HKLM, needs admin, every account gets it
;   Me only    -> %LOCALAPPDATA%\Programs\DexCorral, HKCU, no admin, this account only
; Both modes share one CLSID, so they must never be installed side by side; the
; conflict checks in [Code] enforce that.
;
; Silent installs pick the mode with Inno's own switch:
;   DexCorral_x.y.z_Setup.exe /VERYSILENT /ALLUSERS
;   DexCorral_x.y.z_Setup.exe /VERYSILENT /CURRENTUSER

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
; {autopf} is Program Files in all-users mode and {localappdata}\Programs in
; me-only mode; {group}, {autoprograms} and the HKA registry root follow the
; same choice, so one script covers both installs.
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; Show license agreement page
LicenseFile=..\..\LICENSE
OutputDir=output
OutputBaseFilename=DexCorral_{#MyAppVersion}_Setup
Compression=lzma2
SolidCompression=yes
; Start unelevated and let the user pick. "All users" triggers UAC at that point;
; "me only" never elevates, which also keeps HKCU pointing at the real user.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
; An upgrade keeps the mode of the install it replaces
UsePreviousPrivileges=yes
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
Name: "{group}\{#MyAppName} - Register"; Filename: "{app}\DexCorral.exe"; Parameters: "--register {code:GetScopeArg}"
Name: "{group}\{#MyAppName} - Unregister"; Filename: "{app}\DexCorral.exe"; Parameters: "--unregister {code:GetScopeArg}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Registry]
; Start DexCorral at login by injecting into Explorer (brief EXE, then exits).
; HKA is HKLM in all-users mode, so every account on the PC gets it, and HKCU in
; me-only mode. Writing HKCU from an elevated all-users install would have put
; this in the elevating administrator's profile instead of the real user's.
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "DexCorral"; \
  ValueData: """{app}\DexCorral.exe"" --startup --silent"; \
  Flags: uninsdeletevalue
; UI language chosen in the installer's language dialog. The app uses this as
; the default; config.json's "Language" (in-app setting) overrides it.
; GetInstallerLanguage() reads HKCU first and falls back to HKLM, so an
; all-users install sets the default for every account with one value.
; uninsdeletekey, not uninsdeletevalue: --register writes InstallDir/InstallScope/
; Version here and the hook writes HookStartPending/HookFailureCount at runtime,
; so deleting only Language would leave the key behind. Nothing here needs to
; survive an uninstall.
Root: HKA; Subkey: "Software\DexCorral"; \
  ValueType: string; ValueName: "Language"; \
  ValueData: "{code:GetLangCode}"; \
  Flags: uninsdeletekey

[UninstallDelete]
; Renamed-aside DLLs left by past upgrades (see RenameAsideHookDll in [Code])
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

// --scope argument for DexCorral.exe, matching the chosen install mode.
// Passed explicitly rather than left to the exe's own path-based inference, so
// an install to a non-default directory still registers in the right hive.
function GetScopeArg(Param: String): String;
begin
  if IsAdminInstallMode then
    Result := '--scope=machine'
  else
    Result := '--scope=user';
end;

// Name of the shell extension DLL (renamed aside during upgrades, see below)
const
  HookDll = 'DexCorralHook.dll';
  DexCorralClsid = '{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}';
  MarkerKey = 'Software\DexCorral';
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E4A7B2C1-3D5F-4E8A-9B1C-6F2D8E0A4C7B}_is1';

// Win32 imports used for install-time polling, session scoping and shell notification
function FindWindowW(ClassName: String; WindowName: Integer): Integer;
  external 'FindWindowW@user32.dll stdcall';
function GetCurrentProcessId: DWORD;
  external 'GetCurrentProcessId@kernel32.dll stdcall';
function ProcessIdToSessionId(dwProcessId: DWORD; var pSessionId: DWORD): Boolean;
  external 'ProcessIdToSessionId@kernel32.dll stdcall';

// Helper: convert Boolean to string (InnoSetup Pascal lacks a built-in BoolToStr)
function BoolToStr(Value: Boolean): String;
begin
  if Value then Result := 'True' else Result := 'False';
end;

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

// ---------------------------------------------------------------------------
// Cross-scope conflict detection
//
// The two install modes share one CLSID. HKCR is a merge of HKLM\Software\Classes
// and HKCU\Software\Classes with HKCU winning, so a per-user install silently
// shadows a machine-wide one for that account: it keeps working, but on a
// different version than everyone else, with nothing on screen to say so. Both
// installs also write a Run entry, and both read the same config in %APPDATA%.
// So the modes are treated as mutually exclusive.
// ---------------------------------------------------------------------------

// Reads the InstallDir marker written by DexCorral.exe --register
function ReadInstallMarker(RootKey: Integer; SubKeyPrefix: String; var Dir: String; var Version: String): Boolean;
begin
  Dir := '';
  Version := '';
  Result := RegQueryStringValue(RootKey, SubKeyPrefix + MarkerKey, 'InstallDir', Dir) and (Dir <> '');
  if Result then
    RegQueryStringValue(RootKey, SubKeyPrefix + MarkerKey, 'Version', Version);
end;

// Runs the other mode's uninstaller and waits. Returns False if it could not be
// started or reported failure. The machine uninstaller elevates via UAC on its
// own, so this works from an unelevated me-only install.
function RunOtherUninstaller(RootKey: Integer): Boolean;
var
  Cmd: String;
  ResultCode: Integer;
begin
  Result := False;
  if not RegQueryStringValue(RootKey, UninstallKey, 'UninstallString', Cmd) then begin
    Log('Conflict: no UninstallString found for the other install mode');
    Exit;
  end;
  Cmd := RemoveQuotes(Cmd);
  Log('Conflict: running ' + Cmd + ' /VERYSILENT');
  Result := Exec(Cmd, '/VERYSILENT /NORESTART', '', SW_SHOW, ewWaitUntilTerminated, ResultCode)
            and (ResultCode = 0);
  Log('Conflict: other uninstaller finished ok=' + BoolToStr(Result) + ' code=' + IntToStr(ResultCode));
  // The uninstaller spawns a copy of itself and returns early; give it a moment
  Sleep(2000);
end;

// Me-only mode: a machine-wide install is visible in HKLM and can be removed
// from here (its uninstaller elevates itself). Offer that, or abort.
function CheckMachineInstallConflict: String;
var
  Dir, Version, Msg: String;
begin
  Result := '';
  // The InstallDir marker only exists from 1.0.28 on, so fall back to the
  // uninstall entry, which every past machine install wrote.
  if not ReadInstallMarker(HKLM64, '', Dir, Version) then begin
    if not RegQueryStringValue(HKLM64, UninstallKey, 'InstallLocation', Dir) or (Dir = '') then
      Exit;
    RegQueryStringValue(HKLM64, UninstallKey, 'DisplayVersion', Version);
  end;

  Log('Conflict: machine-wide install detected at ' + Dir + ' version ' + Version);

  if ActiveLanguage = 'german' then
    Msg := 'DexCorral ist bereits für alle Benutzer dieses PCs installiert:' + #13#10 +
           '  ' + Dir + '  (Version ' + Version + ')' + #13#10 + #13#10 +
           'Eine Installation nur für dich würde diese Version für dein Konto verdecken. ' +
           'Beide Varianten teilen sich dieselbe Registrierung und dürfen nicht nebeneinander bestehen.' + #13#10 + #13#10 +
           'Die vorhandene Installation jetzt entfernen und fortfahren? ' +
           '(Dafür sind Administratorrechte erforderlich.)'
  else
    Msg := 'DexCorral is already installed for all users on this PC:' + #13#10 +
           '  ' + Dir + '  (version ' + Version + ')' + #13#10 + #13#10 +
           'Installing for your account only would shadow that copy for you. The two ' +
           'modes share one registration and cannot sit side by side.' + #13#10 + #13#10 +
           'Remove the existing installation now and continue? ' +
           '(This needs administrator rights.)';

  if MsgBox(Msg, mbConfirmation, MB_YESNO) <> IDYES then begin
    if ActiveLanguage = 'german' then
      Result := 'Installation abgebrochen: DexCorral ist bereits für alle Benutzer installiert.'
    else
      Result := 'Setup cancelled: DexCorral is already installed for all users.';
    Exit;
  end;

  if not RunOtherUninstaller(HKLM64) then begin
    if ActiveLanguage = 'german' then
      Result := 'Die vorhandene systemweite Installation konnte nicht entfernt werden. ' +
                'Bitte deinstalliere DexCorral über "Apps & Features" und starte Setup erneut.'
    else
      Result := 'The existing all-users installation could not be removed. ' +
                'Please uninstall DexCorral from "Apps & Features" and run Setup again.';
    Exit;
  end;

  // Belt and braces: the old uninstaller may have left the marker if its
  // --unregister step failed.
  if ReadInstallMarker(HKLM64, '', Dir, Version) then
    Log('Conflict: HKLM marker still present after uninstall, continuing anyway');
end;

// All-users mode: per-user installs live in each account's own hive, so they
// can only be found by walking the loaded user hives, and only their own owner
// can uninstall them. Warn and let the admin decide.
function CheckPerUserInstallConflict: String;
var
  Sids: TArrayOfString;
  Found, Msg, Dir, Version: String;
  i, Count: Integer;
begin
  Result := '';
  Count := 0;
  if not RegGetSubkeyNames(HKEY_USERS, '', Sids) then
    Exit;

  for i := 0 to GetArrayLength(Sids) - 1 do begin
    // Skip the .DEFAULT hive and the _Classes companion keys
    if (Sids[i] = '.DEFAULT') or (Pos('_Classes', Sids[i]) > 0) then
      Continue;
    if ReadInstallMarker(HKEY_USERS, Sids[i] + '\', Dir, Version) then begin
      Found := Found + '  ' + Dir + '  (version ' + Version + ')' + #13#10;
      Count := Count + 1;
    end;
  end;

  if Count = 0 then
    Exit;

  Log('Conflict: ' + IntToStr(Count) + ' per-user install(s) found while installing for all users');

  if ActiveLanguage = 'german' then
    Msg := 'Auf diesem PC ist DexCorral bereits für einzelne Benutzer installiert:' + #13#10 + #13#10 +
           Found + #13#10 +
           'Diese Installationen verdecken die systemweite Version für die betreffenden Konten. ' +
           'Nur der jeweilige Benutzer kann sie entfernen (über "Apps & Features").' + #13#10 + #13#10 +
           'Trotzdem für alle Benutzer installieren?'
  else
    Msg := 'DexCorral is already installed for individual users on this PC:' + #13#10 + #13#10 +
           Found + #13#10 +
           'Those installations will shadow this all-users version for the accounts that own ' +
           'them. Only the user in question can remove one, from "Apps & Features".' + #13#10 + #13#10 +
           'Install for all users anyway?';

  if MsgBox(Msg, mbConfirmation, MB_YESNO) <> IDYES then begin
    if ActiveLanguage = 'german' then
      Result := 'Installation abgebrochen.'
    else
      Result := 'Setup cancelled.';
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
// Mirrors UnregisterShellExtension() in ShellExtension/src/Registration.cpp,
// plus the pre-1.0.28 keys that CleanupLegacyRegistration() handles.
procedure ForceUnregisterShellExtension;
var
  Root: Integer;
begin
  if IsAdminInstallMode then
    Root := HKLM64
  else
    Root := HKCU64;

  // Current layout, this scope
  RegDeleteKeyIncludingSubkeys(Root, 'Software\Classes\CLSID\' + DexCorralClsid);
  RegDeleteKeyIncludingSubkeys(Root,
    'Software\Classes\Directory\Background\ShellEx\ContextMenuHandlers\DexCorral');
  RegDeleteValue(Root, MarkerKey, 'InstallDir');
  RegDeleteValue(Root, MarkerKey, 'InstallScope');
  RegDeleteValue(Root, MarkerKey, 'Version');

  // Machine-only load vectors, and the pre-1.0.28 keys written through HKCR.
  // Only reachable with admin rights, so a me-only install skips them.
  if IsAdminInstallMode then begin
    RegDeleteKeyIncludingSubkeys(HKLM64,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ DexCorral');
    RegDeleteValue(HKLM64,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler', DexCorralClsid);
    RegDeleteKeyIncludingSubkeys(HKLM64,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler\' + DexCorralClsid);
    RegDeleteValue(HKLM64,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellServiceObjectDelayLoad', 'DexCorral');
    RegDeleteKeyIncludingSubkeys(HKCR64, 'CLSID\' + DexCorralClsid);
    RegDeleteKeyIncludingSubkeys(HKCR64,
      'Directory\Background\ShellEx\ContextMenuHandlers\DexCorral');
  end;
end;

// Kill Explorer so the old DLL is unmapped, then wait for the shell to return.
// Winlogon's AutoRestartShell normally relaunches it within a second or two; we
// only start it ourselves if it stays down, which avoids two Explorers.
//
// The SESSION filter matters: an elevated all-users install can otherwise reach
// explorer.exe in every logged-on user's session and blank their desktops too.
procedure RestartExplorer;
var
  ResultCode: Integer;
  SessionId: DWORD;
  Filter: String;
begin
  Filter := '';
  if ProcessIdToSessionId(GetCurrentProcessId, SessionId) then
    Filter := ' /FI "SESSION eq ' + IntToStr(SessionId) + '"'
  else
    Log('RestartExplorer: could not resolve session id - killing without a session filter');

  Exec('taskkill', '/F /IM explorer.exe' + Filter, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  WaitForWindowGone('Progman', 40, 250);   // up to 10 s
  if not WaitForWindow('Progman', 20, 500) then begin
    Log('Shell did not auto-restart - launching explorer.exe');
    Exec(ExpandConstant('{win}\explorer.exe'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    WaitForWindow('Progman', 40, 500);
  end;
end;

// Upgrade support: check for a conflicting install in the other mode, then
// unregister the old shell extension and rename its DLL aside so the new copy
// lands on a free name. Explorer keeps running here on purpose -- restarting it
// before the copy just gives a fresh Explorer a window to map the DLL again; it
// is restarted after the files are in place (see CurStepChanged).
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  AppDir, ExePath: String;
begin
  Result := '';
  NeedsRestart := False;

  Log('Install mode: ' + GetScopeArg(''));

  // Conflict with the other mode. Returning a non-empty string aborts Setup
  // with that message, before anything has been written.
  if IsAdminInstallMode then
    Result := CheckPerUserInstallConflict
  else
    Result := CheckMachineInstallConflict;
  if Result <> '' then
    Exit;

  AppDir := ExpandConstant('{app}');
  ExePath := AppDir + '\DexCorral.exe';
  if not FileExists(ExePath) then
    Exit;  // fresh install, nothing to displace

  g_IsUpgrade := True;
  Log('Existing install detected - preparing for upgrade');

  // A leftover --startup/--register process holds a LoadLibrary reference on the DLL
  Exec('taskkill', '/F /IM DexCorral.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(500);

  // Older builds have no --scope flag and ignore it; they infer from their own
  // path, which for an in-place upgrade gives the same answer.
  if Exec(ExePath, '--unregister --silent ' + GetScopeArg(''), '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
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
  RegArgs: String;
begin
  if CurStep = ssPostInstall then begin
    Log('--- DexCorral post-install sequence ---');
    Log('Install dir: ' + ExpandConstant('{app}'));
    Log('AppData dir: ' + ExpandConstant('{userappdata}'));

    // Register the shell extension in the chosen scope. An all-users install
    // also clears the pre-1.0.28 scopeless keys, which lived in HKLM/HKCR and
    // would otherwise shadow the new CLSID registration.
    RegArgs := '--register --silent ' + GetScopeArg('');
    if IsAdminInstallMode then
      RegArgs := RegArgs + ' --cleanup-legacy';

    Log('Step 1: Registering shell extension: DexCorral.exe ' + RegArgs);
    if Exec(ExpandConstant('{app}\DexCorral.exe'), RegArgs,
            ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      Log('Step 1 result: DexCorral.exe --register exited with code=' + IntToStr(ResultCode));
      if ResultCode <> 0 then begin
        Log('Step 1 ERROR: registration failed.');
        MsgBox('Shell extension registration failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 +
               'Try running "DexCorral.exe --register ' + GetScopeArg('') + '" manually.', mbError, MB_OK);
      end;
    end else begin
      Log('Step 1 ERROR: Failed to launch DexCorral.exe --register (exe missing?)');
      MsgBox('Could not run DexCorral.exe to register the shell extension.' + #13#10 +
             'Try running "DexCorral.exe --register ' + GetScopeArg('') + '" manually.', mbError, MB_OK);
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
    // waits for the App to start inside Explorer, then exits.
    //
    // Note for all-users installs: this only reaches the session Setup runs in.
    // Other logged-on users pick DexCorral up from the HKLM load vectors, or at
    // their next login via the HKLM Run entry.
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
  ExePath: String;
begin
  if CurUninstallStep = usUninstall then begin
    Exec('taskkill', '/F /IM DexCorral.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(500);

    // Unregister in the scope this copy was installed in. A me-only uninstall
    // must not touch the HKLM keys: they may belong to an all-users install
    // that the other accounts on this PC are still using.
    ExePath := ExpandConstant('{app}\DexCorral.exe');
    if FileExists(ExePath) then begin
      if Exec(ExePath, '--unregister --silent ' + GetScopeArg(''), '', SW_HIDE,
              ewWaitUntilTerminated, ResultCode) and (ResultCode = 0) then
        Log('Uninstall: unregistered ' + GetScopeArg(''))
      else begin
        Log('Uninstall: --unregister failed (code ' + IntToStr(ResultCode) + ') - removing keys directly');
        ForceUnregisterShellExtension;
      end;
    end else
      ForceUnregisterShellExtension;

    // Restart Explorer to unload the DLL before the files are removed
    RestartExplorer;

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
      // Only ever this account's config. An all-users uninstall cannot reach
      // the other users' %APPDATA%, and should not: their settings are theirs.
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
