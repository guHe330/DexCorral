# Build Guide

This document describes how to build DexCorral from source.

## Prerequisites

To build the project, you need the following tools installed:

*   Visual Studio 2022 or later (with "Desktop development with C++" workload)
*   CMake 3.15 or later
*   Windows 10 SDK (usually included with Visual Studio)

## Dependencies

The project uses the following third-party libraries:

*   **nlohmann/json**: A header-only JSON library for C++. It is included in the source tree under `DexCorralCpp/include/nlohmann/`.

## Build Instructions

You can build the project using the provided PowerShell script or manually via CMake.

### Automated Build (PowerShell)

Run the following command from the root of the repository:

```powershell
.\build.ps1
```

This script will:
1.  Create a `build` directory.
2.  Run CMake to generate the Visual Studio solution.
3.  Build the project in Release configuration.

### Manual Build

If you prefer to run the commands manually:

1.  Open a terminal (PowerShell or Command Prompt).
2.  Navigate to the `DexCorralCpp` directory.
3.  Create and enter a build directory:
    ```powershell
    mkdir build
    cd build
    ```
4.  Generate the build files:
    ```powershell
    cmake .. -G "Visual Studio 17 2022" -A x64
    ```
5.  Build the project:
    ```powershell
    cmake --build . --config Release
    ```

## Output

After a successful build, the executable `DexCorralCpp.exe` will be located in:
`DexCorralCpp/build/Release/DexCorralCpp.exe`

## Folder Structure

*   `DexCorralCpp/include/`: Header files (.h)
*   `DexCorralCpp/src/`: Source files (.cpp)
*   `DexCorralCpp/include/nlohmann/`: Third-party JSON library
