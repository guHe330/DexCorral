cmake_minimum_required(VERSION 3.15)
project(DexCorralCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static runtime linking (no vcredist needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add executable
add_executable(DexCorralCpp WIN32
    src/main.cpp
    src/App.cpp
    src/DesktopIcons.cpp
    src/WallpaperManager.cpp
    src/CorralWindow.cpp
    src/Config.cpp
    src/MouseHook.cpp
    src/TrayIcon.cpp
    src/DesktopMonitor.cpp
    src/MonitorManager.cpp
    include/App.h
    include/DesktopIcons.h
    include/WallpaperManager.h
    include/CorralWindow.h
    include/Config.h
    include/MouseHook.h
    include/TrayIcon.h
    include/DesktopMonitor.h
    include/MonitorManager.h
)

# Include directories
target_include_directories(DexCorralCpp PRIVATE include)

# Unicode and macro definitions
target_compile_definitions(DexCorralCpp PRIVATE UNICODE _UNICODE NOMINMAX)

# Link Windows libraries
target_link_libraries(DexCorralCpp PRIVATE
    comctl32
    comdlg32
    gdi32
    gdiplus
    shell32
    user32
    ole32
    uuid
)

# Set subsystem to Windows
set_target_properties(DexCorralCpp PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# nlohmann/json is included as a single header in include/nlohmann/json.hpp
