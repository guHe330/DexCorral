cmake_minimum_required(VERSION 3.15)
project(DexCorral VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static runtime linking (no vcredist needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Registration tool (minimal EXE)
add_executable(DexCorral WIN32
    src/main.cpp
    DexCorral.rc
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
    ShellExtension/DexCorralHook.rc
    # Hook / shell extension code
    ShellExtension/src/dllmain.cpp
    ShellExtension/src/CorralHook.cpp
    ShellExtension/src/ClassFactory.cpp
    ShellExtension/src/ShellExtension.cpp
    ShellExtension/src/Registration.cpp
    ShellExtension/src/Guids.cpp
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
    src/IconUtils.cpp
    src/LayoutMath.cpp
)

target_include_directories(DexCorralHook PRIVATE
    ShellExtension/include
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
    LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/ShellExtension/DexCorralHook.def\""
)

set_property(TARGET DexCorralHook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# nlohmann/json is included as a single header in include/nlohmann/json.hpp

# ============================================================================
# Unit tests (Google Test via FetchContent)
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Match the static CRT used by the main targets
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(DexCorralTests
    tests/test_config.cpp
    tests/test_layout.cpp
    tests/test_icon_utils.cpp
    src/LayoutMath.cpp
    src/IconUtils.cpp
)

target_include_directories(DexCorralTests PRIVATE include)
target_link_libraries(DexCorralTests PRIVATE GTest::gtest_main)
target_compile_definitions(DexCorralTests PRIVATE UNICODE _UNICODE NOMINMAX)
set_property(TARGET DexCorralTests PROPERTY MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()
add_test(NAME DexCorralTests COMMAND DexCorralTests)
