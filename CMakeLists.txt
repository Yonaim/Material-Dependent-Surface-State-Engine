cmake_minimum_required(VERSION 3.24)

project(MDSS_Engine
    VERSION 0.1.0
    LANGUAGES CXX
)

option(MDSS_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

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

find_package(glm CONFIG QUIET)

if(NOT glm_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(glm)
endif()

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
target_link_libraries(MDSS PRIVATE glfw Vulkan::Vulkan glm::glm)

file(GLOB_RECURSE MDSS_SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.comp"
)

# Reconfigure when a placeholder shader receives its first implementation.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MDSS_SHADER_SOURCES})

set(MDSS_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/Shaders")
set(MDSS_SHADER_BINARIES)

foreach(ShaderSource IN LISTS MDSS_SHADER_SOURCES)
    file(STRINGS "${ShaderSource}" ShaderVersionLine
        LIMIT_COUNT 1
        REGEX "^[ \t]*#version"
    )
    if(NOT ShaderVersionLine)
        message(STATUS "Skipping placeholder shader: ${ShaderSource}")
        continue()
    endif()

    file(RELATIVE_PATH ShaderRelativePath
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
        "${ShaderSource}"
    )
    set(ShaderBinary "${MDSS_SHADER_OUTPUT_DIR}/${ShaderRelativePath}.spv")
    get_filename_component(ShaderOutputDirectory "${ShaderBinary}" DIRECTORY)

    add_custom_command(
        OUTPUT "${ShaderBinary}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ShaderOutputDirectory}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${ShaderSource}" -o "${ShaderBinary}"
        DEPENDS "${ShaderSource}"
        COMMENT "Compiling shader ${ShaderRelativePath}"
        VERBATIM
    )

    list(APPEND MDSS_SHADER_BINARIES "${ShaderBinary}")
endforeach()

add_custom_target(MDSS_Shaders DEPENDS ${MDSS_SHADER_BINARIES})
add_dependencies(MDSS MDSS_Shaders)

target_compile_definitions(MDSS PRIVATE
    MDSSP_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}"
    MDSSP_ENABLE_VALIDATION=$<BOOL:${MDSS_ENABLE_VALIDATION}>
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(MSVC)
    target_compile_options(MDSS PRIVATE /W4 /permissive-)
else()
    target_compile_options(MDSS PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(MDSS PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
