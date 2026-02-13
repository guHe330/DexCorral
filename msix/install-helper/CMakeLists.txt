cmake_minimum_required(VERSION 3.20)
project(InstallHelper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Win32 executable
add_executable(install-helper WIN32
    install-helper.cpp
)

# Link libraries
target_link_libraries(install-helper
    shlwapi
)

# Set subsystem to console for logging output
set_target_properties(install-helper PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Static CRT to match main app
set_property(TARGET install-helper PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
