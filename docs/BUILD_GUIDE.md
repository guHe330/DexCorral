# Build Guide

How to build DexCorral from source.

## Prerequisites

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.15 or later (included with Visual Studio, or install separately)
- Windows 10 SDK (included with the workload above)
- Internet access on first build (CMake fetches Google Test automatically)

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
4. Runs the unit tests — build stops here if any test fails.
5. Packages `DexCorral.zip` from the two output binaries.
6. Builds the Inno Setup installer if ISCC is available.

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

For Win32-dependent behaviour that cannot be unit tested (Explorer hook, drag-drop, DPI scaling, multi-monitor, etc.) see [INTEGRATION_TESTS.md](../INTEGRATION_TESTS.md).

## Output binaries

| File | Description |
|------|-------------|
| `build/DexCorralHook.dll` | Monolith shell extension — all app logic + Explorer hook. |
| `build/DexCorral.exe` | Registration tool (run once as admin to install the shell extension). |
| `build/DexCorralTests.exe` | Unit test runner. |
| `build/DexCorral.zip` | Release archive containing the two distributable binaries. |

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
