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

include(FetchContent)

find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
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

# OBJ/MTL parser. We include the single header directly and compile the
# implementation in OBJLoader.cpp so tinyobjloader types stay inside Loaders/.
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# Image decoder used by TextureLoader (PNG/JPG/etc.).
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

# JSON parser for project assets. Vendor headers during active development so
# configure/build does not require network access; replace with a package-managed
# dependency when the project dependency workflow is finalized.
add_library(MDSS_nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS MDSS_nlohmann_json)
target_include_directories(MDSS_nlohmann_json SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/nlohmann_json/include"
)

# Dear ImGui debug/editor UI. We build only the core plus GLFW/Vulkan backends.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(MDSS_ImGui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

target_include_directories(MDSS_ImGui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(MDSS_ImGui PUBLIC glfw Vulkan::Vulkan)

file(GLOB_RECURSE MDSS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

add_executable(MDSS ${MDSS_SOURCES})

target_include_directories(MDSS PRIVATE
    Source
)

target_include_directories(MDSS SYSTEM PRIVATE
    "${tinyobjloader_SOURCE_DIR}"
    "${stb_SOURCE_DIR}"
)
target_link_libraries(MDSS PRIVATE MDSS_ImGui glfw Vulkan::Vulkan glm::glm nlohmann_json::nlohmann_json)

file(GLOB_RECURSE MDSS_SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.tesc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.tese"
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
    MDSS_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}"
    MDSS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
    MDSS_ENABLE_VALIDATION=$<BOOL:${MDSS_ENABLE_VALIDATION}>
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

include(CTest)
if(BUILD_TESTING)
    add_executable(MDSS_SurfaceStateTests
        Tests/SurfaceStateContractTests.cpp
        Source/AssetManager/Core/Asset.cpp
        Source/AssetManager/Assets/SRProfileAsset.cpp
        Source/AssetManager/Loaders/SRProfileLoader.cpp
        Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.cpp
        Source/SurfaceStateSystem/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.cpp
        Source/SurfaceStateSystem/State/SurfaceInstanceStateData.cpp
        Source/SurfaceStateSystem/Types/SurfaceStateRegistry.cpp
        Source/SurfaceStateSystem/Types/SurfaceStateTypes.cpp
    )
    target_include_directories(MDSS_SurfaceStateTests PRIVATE Source)
    target_link_libraries(MDSS_SurfaceStateTests PRIVATE glm::glm nlohmann_json::nlohmann_json)
    target_compile_definitions(MDSS_SurfaceStateTests PRIVATE
        MDSS_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
    )
    if(MSVC)
        target_compile_options(MDSS_SurfaceStateTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SurfaceStateTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SurfaceStateContract COMMAND MDSS_SurfaceStateTests)

    add_executable(MDSS_SurfaceMappingTests
        Tests/SurfaceMappingTests.cpp
        Source/AssetManager/Loaders/MTLLoader.cpp
        Source/AssetManager/Loaders/OBJLoader.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceStateSystem/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceStateSystem/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.cpp
    )
    target_include_directories(MDSS_SurfaceMappingTests PRIVATE Source)
    target_include_directories(MDSS_SurfaceMappingTests SYSTEM PRIVATE "${tinyobjloader_SOURCE_DIR}")
    target_link_libraries(MDSS_SurfaceMappingTests PRIVATE glm::glm)
    target_compile_definitions(MDSS_SurfaceMappingTests PRIVATE
        MDSS_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
    )
    if(MSVC)
        target_compile_options(MDSS_SurfaceMappingTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SurfaceMappingTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SurfaceMapping COMMAND MDSS_SurfaceMappingTests)

    add_executable(MDSS_SharedSurfaceGeometryTests
        Tests/SharedSurfaceGeometryTests.cpp
        Source/AssetManager/Loaders/MTLLoader.cpp
        Source/AssetManager/Loaders/OBJLoader.cpp
        Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.cpp
        Source/SurfaceStateSystem/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceStateSystem/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.cpp
    )
    target_include_directories(MDSS_SharedSurfaceGeometryTests PRIVATE Source)
    target_include_directories(MDSS_SharedSurfaceGeometryTests SYSTEM PRIVATE "${tinyobjloader_SOURCE_DIR}")
    target_link_libraries(MDSS_SharedSurfaceGeometryTests PRIVATE glm::glm nlohmann_json::nlohmann_json)
    target_compile_definitions(MDSS_SharedSurfaceGeometryTests PRIVATE
        MDSS_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
    )
    if(MSVC)
        target_compile_options(MDSS_SharedSurfaceGeometryTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SharedSurfaceGeometryTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SharedSurfaceGeometry COMMAND MDSS_SharedSurfaceGeometryTests)

    add_executable(MDSS_SurfaceGPUResourceTests
        Tests/SurfaceGPUResourceTests.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.cpp
        Source/SurfaceStateSystem/GPU/SurfaceGPUResources.cpp
        Source/SurfaceStateSystem/Types/SurfaceStateRegistry.cpp
        Source/SurfaceStateSystem/Types/SurfaceStateTypes.cpp
        Source/VulkanContext/GPU/GPUBuffer.cpp
    )
    target_include_directories(MDSS_SurfaceGPUResourceTests PRIVATE Source)
    target_link_libraries(MDSS_SurfaceGPUResourceTests PRIVATE Vulkan::Vulkan glm::glm)
    if(MSVC)
        target_compile_options(MDSS_SurfaceGPUResourceTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SurfaceGPUResourceTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SurfaceGPUResource COMMAND MDSS_SurfaceGPUResourceTests)
    set_tests_properties(MDSS_SurfaceGPUResource PROPERTIES SKIP_RETURN_CODE 77)
endif()
