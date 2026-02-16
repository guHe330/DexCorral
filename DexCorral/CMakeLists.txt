cmake_minimum_required(VERSION 3.15)
project(DexCorral VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static runtime linking (no vcredist needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Registration tool (minimal EXE)
add_executable(DexCorral WIN32
    src/main.cpp
)

target_compile_definitions(DexCorral PRIVATE UNICODE _UNICODE)

target_link_libraries(DexCorral PRIVATE
    shell32
    user32
)

set_target_properties(DexCorral PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# DexCorralHook DLL — monolith shell extension (all app logic + Explorer hook)
add_library(DexCorralHook SHARED
    # Hook / shell extension code
    ../CorralHook/src/dllmain.cpp
    ../CorralHook/src/CorralHook.cpp
    ../CorralHook/src/ClassFactory.cpp
    ../CorralHook/src/ShellExtension.cpp
    ../CorralHook/src/Registration.cpp
    ../CorralHook/src/Guids.cpp
    # App logic (moved from EXE)
    src/App.cpp
    src/DesktopIcons.cpp
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
)

target_include_directories(DexCorralHook PRIVATE
    ../CorralHook/include
    include
)

target_link_libraries(DexCorralHook PRIVATE
    comctl32
    comdlg32
    gdi32
    shell32
    user32
    ole32
    oleaut32
    uuid
    psapi
    shlwapi
)

target_compile_definitions(DexCorralHook PRIVATE UNICODE _UNICODE NOMINMAX)

# Use .def file for COM exports
set_target_properties(DexCorralHook PROPERTIES
    LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/../CorralHook/DexCorralHook.def\""
)

set_property(TARGET DexCorralHook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# nlohmann/json is included as a single header in include/nlohmann/json.hpp
