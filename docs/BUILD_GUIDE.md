# Build Guide

How to build DexCorral from source.

## Prerequisites

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.15 or later (included with Visual Studio, or install separately)
- Windows 11 SDK (included with the workload above)
- Internet access on first build (CMake fetches Google Test automatically)
- Optional: [Inno Setup 6](https://jrsoftware.org/isdl.php): if installed, the build script also produces the installer

## Verify your environment

To confirm all prerequisites are present: `.\DexCorral\check-env.ps1`

## Build

Run from the repo root or the `DexCorral/` directory:

```powershell
cd DexCorral
powershell -File build.ps1
```

The script:
1. Kills any running DexCorral process (unlocks the exe for relinking).
2. Restarts Explorer if `DexCorralHook.dll` is locked by the shell.
3. Configures and builds with CMake + Ninja (falls back to NMake if Ninja is absent).
4. Runs the unit tests. If any test fails, the build stops and the zip/installer steps are skipped.
5. Packages `DexCorral_<version>.zip` from the two output binaries (older version-named zips are removed first).
6. Builds the Inno Setup installer if ISCC is available (output: `installer/innosetup/output/DexCorral_<version>_Setup.exe`).

Output goes to `DexCorral/build/`.

## Build switches

| Switch | Description |
|--------|-------------|
| `-Clean` | Delete `DexCorral/build/` before building (full rebuild). |
| `-SkipTests` | Skip running unit tests after a successful build. |
| `-BuildType <type>` | CMake build type. Default: `Release`. Use `Debug` for a debug build. |

```powershell
# Full clean rebuild
powershell -File build.ps1 -Clean

# Quick iteration: skip tests
powershell -File build.ps1 -SkipTests

# Debug build
powershell -File build.ps1 -BuildType Debug

# Combine switches
powershell -File build.ps1 -Clean -BuildType Debug -SkipTests
```

## Run tests manually

```powershell
DexCorral\build\DexCorralTests.exe
```

Win32-dependent behaviour (Explorer hook, drag-drop, DPI scaling, multi-monitor, etc.) is not covered by the unit tests and has to be verified by hand.

## Output binaries

| File | Description |
|------|-------------|
| `build/DexCorralHook.dll` | The shell extension. Holds all app logic and the Explorer hook. |
| `build/DexCorral.exe` | Registration tool (run once to register the shell extension). |
| `build/DexCorralTests.exe` | Unit test runner. |
| `build/DexCorral_<version>.zip` | Release archive containing the two distributable binaries, named after the version in `Version.h`. |

## Run your build

From `DexCorral/build/`, without Administrator rights:

```powershell
.\DexCorral.exe --register --scope=user   # one-time registration for your account; also starts it and adds the start at sign-in
.\DexCorral.exe --startup    # inject into the running Explorer again, e.g. after an Explorer restart
.\DexCorral.exe --register --scope=user --force   # register on an unsupported Windows version (Windows 11 is required otherwise)
```

Outside Program Files `--register` uses `--scope=user` anyway: it writes to `HKCU` only, for the account that runs it, even when run as Administrator. For that account it takes precedence over an all-users install and replaces a per-user one, so while it exists you run your build instead. `--scope=machine` (elevated) is only needed to test the `HKLM` load paths.

After the first registration, rebuilding the DLL only requires Explorer to reload it; the build script handles unlocking automatically. To remove the registration: `DexCorral.exe --unregister --scope=user`. With DexCorral also installed for you only, that removes its registration as well; run its Setup again afterwards.

## Unlock the DLL manually

If the build fails because Explorer has `DexCorralHook.dll` locked and the script's automatic unlock doesn't help:

```powershell
Stop-Process -Name explorer -Force; Start-Sleep 3; Start-Process explorer.exe
```

## Dependencies

| Dependency | License | How it's included |
|------------|---------|-------------------|
| [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | Single header checked into `DexCorral/include/nlohmann/json.hpp`, shipped in the binaries |
| [Google Test](https://github.com/google/googletest) 1.14.0 | [BSD-3-Clause](https://github.com/google/googletest/blob/main/LICENSE) | Downloaded by CMake `FetchContent` on first build; cached in the build tree. Tests only, not shipped |
