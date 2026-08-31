cmake_minimum_required(VERSION 3.15)

# Single source of truth for the version: include/Version.h (see that file's header).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/Version.h" _dexcorral_version_h)
string(REGEX MATCH "DEXCORRAL_VERSION_MAJOR ([0-9]+)" _ "${_dexcorral_version_h}")
set(_dexcorral_ver_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "DEXCORRAL_VERSION_MINOR ([0-9]+)" _ "${_dexcorral_version_h}")
set(_dexcorral_ver_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "DEXCORRAL_VERSION_PATCH ([0-9]+)" _ "${_dexcorral_version_h}")
set(_dexcorral_ver_patch "${CMAKE_MATCH_1}")

project(DexCorral VERSION ${_dexcorral_ver_major}.${_dexcorral_ver_minor}.${_dexcorral_ver_patch} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources are UTF-8 (the string catalog contains umlauts); tell MSVC so wide
# literals are decoded correctly regardless of the system ANSI codepage.
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Use static runtime linking (no vcredist needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Registration tool (minimal EXE)
add_executable(DexCorral WIN32
    src/main.cpp
    src/Strings.cpp
    DexCorral.rc
)

target_include_directories(DexCorral PRIVATE include)

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
    src/CorralWindowKeyboard.cpp
    src/Config.cpp
    src/MouseHook.cpp
    src/TrayIcon.cpp
    src/DesktopMonitor.cpp
    src/MonitorManager.cpp
    src/FolderWatcher.cpp
    src/HookBridge.cpp
    src/IconUtils.cpp
    src/LayoutMath.cpp
    src/UpdateChecker.cpp
    src/Strings.cpp
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
    winhttp
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
    # A tag archive is mutable - GitHub regenerates it and a tag can be moved.
    # Pin the content, not just the name.
    URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Match the static CRT used by the main targets
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(DexCorralTests
    tests/test_config.cpp
    tests/test_layout.cpp
    tests/test_icon_utils.cpp
    tests/test_chrome_alpha.cpp
    tests/test_strings.cpp
    src/LayoutMath.cpp
    src/IconUtils.cpp
    src/Config.cpp
    src/Strings.cpp
)

target_include_directories(DexCorralTests PRIVATE include)
target_link_libraries(DexCorralTests PRIVATE GTest::gtest_main shell32)
target_compile_definitions(DexCorralTests PRIVATE UNICODE _UNICODE NOMINMAX)
set_property(TARGET DexCorralTests PROPERTY MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()
add_test(NAME DexCorralTests COMMAND DexCorralTests)
