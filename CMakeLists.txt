cmake_minimum_required(VERSION 3.24)

project(MDSSP_Engine
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDSSP_USE_SYSTEM_GLFW
    "Use an already installed GLFW package instead of FetchContent"
    OFF
)

if(MDSSP_USE_SYSTEM_GLFW)
    find_package(glfw3 CONFIG REQUIRED)
else()
    include(FetchContent)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(glfw)
endif()

# Only the files required for the current window-only milestone are compiled.
# Future module files already exist as placeholders and can be added here when implemented.
add_executable(MDSSP
    Source/main.cpp
    Source/Application/Application.h
    Source/Application/Application.cpp
    Source/Application/Window.h
    Source/Application/Window.cpp
)

target_include_directories(MDSSP PRIVATE Source)
target_link_libraries(MDSSP PRIVATE glfw)

if(MSVC)
    target_compile_options(MDSSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(MDSSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(MDSSP PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
