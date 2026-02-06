cmake_minimum_required(VERSION 3.15)
project(DexCorral VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static runtime linking (no vcredist needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add executable
add_executable(DexCorral WIN32
    src/main.cpp
    src/App.cpp
    src/DesktopIcons.cpp
    src/WallpaperManager.cpp
    src/CorralWindow.cpp
    src/CorralWindowRender.cpp
    src/CorralWindowIcons.cpp
    src/CorralWindowInput.cpp
    src/CorralWindowDialogs.cpp
    src/CorralWindowCommands.cpp
    src/Config.cpp
    src/MouseHook.cpp
    src/TrayIcon.cpp
    src/DesktopMonitor.cpp
    src/MonitorManager.cpp
    src/FolderWatcher.cpp
    src/HookBridge.cpp
    include/App.h
    include/HookBridge.h
    include/DesktopIcons.h
    include/WallpaperManager.h
    include/CorralWindow.h
    include/Config.h
    include/MouseHook.h
    include/TrayIcon.h
    include/DesktopMonitor.h
    include/MonitorManager.h
    include/FolderWatcher.h
)

# Include directories
target_include_directories(DexCorral PRIVATE include)

# Unicode and macro definitions
target_compile_definitions(DexCorral PRIVATE UNICODE _UNICODE NOMINMAX)

# Link Windows libraries
target_link_libraries(DexCorral PRIVATE
    comctl32
    comdlg32
    gdi32
    gdiplus
    shell32
    user32
    ole32
    uuid
    psapi
)

# Set subsystem to Windows
set_target_properties(DexCorral PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Watchdog executable
add_executable(DexCorral.Watchdog WIN32
    src/Watchdog.cpp
)

target_compile_definitions(DexCorral.Watchdog PRIVATE UNICODE _UNICODE)

target_link_libraries(DexCorral.Watchdog PRIVATE
    shell32
    user32
)

set_target_properties(DexCorral.Watchdog PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# CorralHook DLL (clean implementation for Explorer injection)
add_library(CorralHook SHARED
    ../CorralHook/src/dllmain.cpp
    ../CorralHook/src/CorralHook.cpp
)

target_include_directories(CorralHook PRIVATE ../CorralHook/include)

target_link_libraries(CorralHook PRIVATE
    comctl32
    user32
)

target_compile_definitions(CorralHook PRIVATE UNICODE _UNICODE)

set_property(TARGET CorralHook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# nlohmann/json is included as a single header in include/nlohmann/json.hpp
