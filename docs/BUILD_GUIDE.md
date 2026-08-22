# Build Guide

How to build DexCorral from source.

## Prerequisites

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.15 or later (included with Visual Studio, or install separately)
- Windows 10 SDK (included with the workload above)
- Internet access on first build (CMake fetches Google Test automatically)
- Optional: [Inno Setup 6](https://jrsoftware.org/isdl.php) — if installed, the build script also produces the installer

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
4. Runs the unit tests — if any test fails, the build stops and the zip/installer steps are skipped.
5. Packages `DexCorral.zip` from the two output binaries.
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

# Quick iteration — skip tests
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
| `build/DexCorralHook.dll` | Monolith shell extension — all app logic + Explorer hook. |
| `build/DexCorral.exe` | Registration tool (run once as admin to install the shell extension). |
| `build/DexCorralTests.exe` | Unit test runner. |
| `build/DexCorral.zip` | Release archive containing the two distributable binaries. |

## Run your build

From `DexCorral/build/`, as Administrator:

```powershell
.\DexCorral.exe --register   # one-time shell extension registration
.\DexCorral.exe --startup    # inject into the running Explorer (no restart needed)
```

After the first registration, rebuilding the DLL only requires Explorer to reload it — the build script handles unlocking automatically. To remove the registration: `DexCorral.exe --unregister`.

## Unlock the DLL manually

If the build fails because `DexCorralHook.dll` is locked by Explorer and the script's automatic unlock doesn't work:

```powershell
Stop-Process -Name explorer -Force; Start-Sleep 3; Start-Process explorer.exe
```

## Dependencies

| Dependency | How it's included |
|------------|-------------------|
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | Single header checked into `DexCorral/include/nlohmann/json.hpp` |
| [Google Test](https://github.com/google/googletest) 1.14.0 | Downloaded by CMake `FetchContent` on first build; cached in the build tree |
