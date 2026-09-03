# DexCorral Installer

The Inno Setup script is [`innosetup/DexCorral.iss`](innosetup/DexCorral.iss). One script builds one Setup.exe that can install in either of two modes; the user picks on Setup's first page.

## Building

```powershell
iscc /DMyAppVersion=1.2.3 innosetup\DexCorral.iss
```

Output goes to `innosetup/output/`. Requires Inno Setup 6+.

## The two install modes

| | All users (machine) | Just me (user) |
|---|---|---|
| Files | `%ProgramFiles%\DexCorral` | `%LOCALAPPDATA%\Programs\DexCorral` |
| Registry root | `HKLM` | `HKCU` |
| Administrator rights | required, UAC prompt on the mode page | none |
| Who gets DexCorral | every account on the PC | the installing account only |
| Updates | an administrator updates once for everyone | each user updates their own copy |
| Uninstall entry | `HKLM\…\Uninstall`, visible to all | `HKCU\…\Uninstall`, visible to that user |
| Explorer load vectors | `SharedTaskScheduler`, icon overlay handler, `Run` key injection, context menu | `Run` key injection, context menu |

Inno's `PrivilegesRequired=lowest` plus `PrivilegesRequiredOverridesAllowed=dialog commandline` produces the mode page and the elevation. `{autopf}`, `{group}` and the `HKA` registry root all follow the chosen mode, which is why one script covers both. Silent installs pick a mode with `/ALLUSERS` or `/CURRENTUSER`; an upgrade keeps the mode of the install it replaces (`UsePreviousPrivileges=yes`).

`DexCorral.exe` takes the mode as `--scope=machine` or `--scope=user` on `--register` and `--unregister`. Setup always passes it explicitly rather than relying on the exe's own fallback, which infers machine scope from sitting under `%ProgramFiles%`. `--scope=machine` without elevation stops with a message instead of failing halfway through.

## Why the two modes are mutually exclusive

Both modes register the same CLSID, `{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}`. `HKEY_CLASSES_ROOT` is a merged view of `HKLM\Software\Classes` and `HKCU\Software\Classes` in which HKCU wins, so a per-user install silently shadows a machine-wide one for that account. Nothing breaks visibly: the account just runs a different build than everyone else on the PC, with no indication on screen. Both modes also write a `Run` entry and share one `config.json` in `%APPDATA%\DexCorral`, so an older shadowing copy can drop config fields a newer one wrote.

Setup therefore checks for the other mode in `PrepareToInstall`, before anything is written:

- **Installing just for me, machine install present**: found through the `HKLM` install marker, or the `HKLM` uninstall entry for installs before 1.0.28. Setup offers to run the machine uninstaller (which elevates itself) and continue, or aborts.
- **Installing for all users, per-user installs present**: found by walking the loaded hives under `HKEY_USERS` for an `InstallDir` marker. Only the owning user can remove one, so Setup lists them and asks whether to continue anyway.

The second check sees only hives that are currently loaded, which in practice means logged-on users. It is a warning, not a guarantee.

## Install marker

`DexCorral.exe --register` writes three values under `Software\DexCorral` of its scope's root, and `--unregister` removes them:

- **`InstallDir`**: the folder holding `DexCorralHook.dll`
- **`InstallScope`**: `machine` or `user`
- **`Version`**: the `DEXCORRAL_VERSION` of the registering binary

They exist for the cross-mode conflict detection above. The rest of `Software\DexCorral` (`Language`, and the hook's runtime `HookStartPending` / `HookFailureCount`) is untouched by registration.

## Registry keys, by mode

`<root>` is `HKLM` for a machine install and `HKCU` for a per-user one. Written by `--register`, removed by `--unregister`, unless noted.

| Key | Value | Scope | Purpose |
|---|---|---|---|
| `<root>\Software\Classes\CLSID\{7A3B9E42-…}\InprocServer32` | (default), `ThreadingModel` | both | COM registration for `DexCorralHook.dll` |
| `<root>\Software\Classes\Directory\Background\ShellEx\ContextMenuHandlers\DexCorral` | (default) | both | desktop right-click menu, and a load vector when the menu is opened |
| `<root>\Software\DexCorral` | `InstallDir`, `InstallScope`, `Version` | both | install marker, see above |
| `<root>\Software\DexCorral` | `Language` | both | UI language, written by Setup, not by `--register` |
| `<root>\Software\Microsoft\Windows\CurrentVersion\Run` | `DexCorral` | both | login injection, written by Setup, not by `--register` |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SharedTaskScheduler` | `{7A3B9E42-…}` | machine only | Explorer loads the DLL at startup |
| `HKLM\SOFTWARE\…\Explorer\ShellIconOverlayIdentifiers\ DexCorral` | (default) | machine only | second startup load vector, no overlay is ever drawn |

Neither mode writes through `HKEY_CLASSES_ROOT`. An `HKCR` write lands in `HKLM` if the key already exists there and in `HKCU` otherwise, which is exactly the ambiguity the scopes exist to remove.

A per-user install cannot use `SharedTaskScheduler` or `ShellIconOverlayIdentifiers`: Explorer reads both from `HKLM` only. It relies on the `Run` key injection instead, which is per-user by construction and needs no COM registration at all. The practical difference is that after an Explorer crash mid-session, a machine install reloads the DLL automatically while a per-user install does not until the next login or the next desktop right-click.

## Legacy cleanup

Installs before 1.0.28 had no notion of scope and registered through `HKEY_CLASSES_ROOT`. `DexCorral.exe --cleanup-legacy` removes that layout: the `HKCR` CLSID and context menu handler, the two `HKLM` load vectors, and the `ShellServiceObjectDelayLoad` value from even older builds. Setup runs it alongside `--register` on machine installs only, since it needs admin rights. A per-user install of 1.0.28 over an old machine install never reaches this path: the conflict check makes the user remove the old install first, and its own uninstaller does the cleanup.

## Upgrade and uninstall sequence

Unchanged in shape from earlier versions, now scope-aware:

1. `PrepareToInstall`: conflict check, kill any `DexCorral.exe`, `--unregister` the old copy in the same scope (falling back to deleting the keys directly), rename `DexCorralHook.dll` aside so the new copy lands on a free name.
2. `ssPostInstall`: `--register` in the chosen scope, restart Explorer if this was an upgrade, `--startup` to inject into the running Explorer, then delete the renamed-aside DLL.
3. Uninstall: `--unregister` in the recorded scope, restart Explorer, ask whether to keep `%APPDATA%\DexCorral`.

The Explorer restart filters `taskkill` by session id. Without that filter an elevated machine-mode install or uninstall reaches `explorer.exe` in every logged-on user's session and blanks their desktops too.

A machine-mode uninstall only removes the config of the account running it. Other users' `%APPDATA%\DexCorral` is theirs, and a running Explorer in another session keeps the already-loaded DLL until that user logs off.
