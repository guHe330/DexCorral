# DexCorral Technical Notes

Details for administrators and the curious. Everyday use is covered by the [User Manual](USER_MANUAL.md); how DexCorral hooks into Explorer is in [Windows APIs and Hooks](WIN32_API_AND_HOOKS.md).

## Install modes

DexCorral installs either for all users (`C:\Program Files\DexCorral`, registered in `HKLM`, Administrator rights) or for one account (`%LOCALAPPDATA%\Programs\DexCorral`, registered in `HKCU`, no Administrator rights). With an all-users install an administrator updates it once for everybody; with a per-user install you update your own copy.

The two cannot be installed side by side. They register the same shell extension, and a per-user install would quietly take precedence over a machine-wide one for your account, leaving you on a different version than everyone else with nothing on screen to say so. Setup checks for the other kind before it writes anything: installing just for yourself over an existing all-users install offers to remove that one first, and installing for all users warns you about any per-user installs it can see (only the owner of one can remove it).

The all-users install has one extra load path into Explorer, so it recovers on its own if Explorer crashes mid-session, where a per-user install waits until your next login or your next right-click on the desktop.

## What DexCorral writes outside its own folder

Everything you configure lives in `config.json` in `%APPDATA%\DexCorral`. The registry is used only where Windows requires it: to register the shell extension, to start at login, and for a few settings that must be readable before the config file is.

Which hive these live in follows the install mode you chose. `<root>` below is `HKLM` for an all-users install and `HKCU` for a per-user one. Two keys are all-users only, because Windows reads them from `HKLM` and nowhere else.

| Key | Value | Why | Removed by the uninstaller |
|---|---|---|---|
| `<root>\Software\Microsoft\Windows\CurrentVersion\Run` | `DexCorral` | Starts DexCorral at login. Written by the installer only. | Yes |
| `<root>\Software\DexCorral` | `Language` | The interface language, so it is known before `config.json` exists and by `DexCorral.exe`'s own dialogs, which never read the config. Written by the installer and by the **Language** choice in the settings window. | Yes |
| `<root>\Software\DexCorral` | `InstallDir`, `InstallScope`, `Version` | Records where and how DexCorral is installed, so Setup can tell you if you are about to install the two modes on top of each other. | Yes |
| `HKCU\Software\DexCorral` | `ConflictNoticeShown` | Remembers that the Stardock Fences explanation has been shown, so later starts only remind. Removed by DexCorral itself once Fences is gone. | Yes |
| `HKCU\Software\DexCorral` | `HookStartPending`, `GuardLastFailure`, `GuardStandby`, `GuardVersion` | The crash guard's memory (see [When DexCorral pauses itself](USER_MANUAL.md#when-dexcorral-pauses-itself)). Written from inside Explorer, where the config file is not available, so always per-user. | Yes |
| `HKCU\Software\DexCorral` | `AutoArrange` | Whether **Auto arrange icons** is on for the desktop. DexCorral arranges the free icons itself and keeps Explorer's own setting off, so the choice has to be remembered here. | Yes |
| `<root>\Software\Classes\CLSID\{7A3B9E42-…}` | (several) | Standard COM registration for `DexCorralHook.dll`. | Yes |
| `<root>\Software\Classes\DesktopBackground\ShellEx\ContextMenuHandlers\DexCorral` | (default) | The desktop right-click menu entry. | Yes |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler` | `{7A3B9E42-…}` | How Explorer loads the extension at startup. All-users installs only. | Yes |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ DexCorral` | (default) | Second startup load path. No overlay is ever drawn. All-users installs only. | Yes |

The `Software\Classes` and `HKLM` entries are the ones `DexCorral.exe --register` creates and `--unregister` removes. They are the reason an all-users registration needs Administrator rights; a per-user registration writes only to `HKCU` and needs none. Nothing else on the system is touched.

## Removing a portable package

Deleting the folder of a portable package leaves its registry entries behind. Run `DexCorral.exe --unregister` first, with the same `--scope` you registered with (as Administrator for `--scope=machine`); to also clear the per-user settings, delete `HKCU\Software\DexCorral` and `%APPDATA%\DexCorral`.

## Leftovers of old versions

Upgrading from 1.0.27 or earlier? Those versions registered without any notion of scope. The installer clears the old entries for you; for a portable install, run `DexCorral.exe --cleanup-legacy` once as Administrator.

Versions up to 1.0.27 wrote the COM registration through `HKEY_CLASSES_ROOT` instead, with no per-user option. `DexCorral.exe --cleanup-legacy`, run once as Administrator, removes what those versions left behind; the installer does it for you.

Early versions had a **Hide shortcut arrows** option, removed since. If you switched it on back then and want the arrows back, delete the value `29` under `SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Icons` in `HKLM` or `HKCU`, whichever holds it, and restart Explorer.

## The crash guard

The [User Manual](USER_MANUAL.md#when-dexcorral-pauses-itself) describes what the crash guard does and how to resume.

What counts as a crash: Explorer ending within a minute of DexCorral's start, without a clean shutdown. That includes Explorer being ended from Task Manager or a script, which is why a single one is only noted and forgotten again if nothing follows.

For scripts: `DexCorral.exe --reset-guard` clears the guard's memory, the same as **Resume**, effective at the next start.

## How Stardock Fences is recognised

Why the two cannot run together is in the [User Manual](USER_MANUAL.md#stardock-fences). The doubled icons were observed with Fences 6.5.4.1.

Fences is recognised by its entry in Windows' list of installed programs and by its desktop module being loaded in Explorer, checked against that file's own company and product description.

`DexCorral.exe --register` refuses with exit code 3 when Fences is installed. `--check-conflicts` reports the same result without doing anything else.

## A Quick-Hide hotkey with the Win key

The hotkey field of the settings window cannot take a combination with the Win key. Set it as `QuickHideHotkey` in `config.json` instead, for example `"QuickHideHotkey": "Win+Alt+D"`, then sign out and back in. The settings window leaves such a hotkey alone unless you enter a new one.
