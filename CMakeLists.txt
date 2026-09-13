cmake_minimum_required(VERSION 3.24)

project(MDSS_Engine
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan QUIET)

# The macOS SDK installer uses ~/VulkanSDK/<version>/macOS by default.
# Prefer normal package discovery (including VULKAN_SDK) before this fallback.
if(APPLE AND NOT Vulkan_FOUND)
    file(GLOB VulkanSDKRoots LIST_DIRECTORIES TRUE "$ENV{HOME}/VulkanSDK/*/macOS")
    list(SORT VulkanSDKRoots COMPARE NATURAL ORDER DESCENDING)
    list(APPEND CMAKE_PREFIX_PATH ${VulkanSDKRoots})
endif()

find_package(Vulkan REQUIRED)

option(MDSS_USE_SYSTEM_GLFW
    "Use an already installed GLFW package instead of FetchContent"
    OFF
)

if(MDSS_USE_SYSTEM_GLFW)
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

file(GLOB_RECURSE MDSS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

add_executable(MDSS ${MDSS_SOURCES})

target_include_directories(MDSS PRIVATE Source)
target_link_libraries(MDSS PRIVATE glfw Vulkan::Vulkan)

if(MSVC)
    target_compile_options(MDSS PRIVATE /W4 /permissive-)
else()
    target_compile_options(MDSS PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(MDSS PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
