# 소스·의존성·셰이더·테스트와 연구 도구의 빌드 대상을 구성한다.
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
    GIT_TAG v1.92.9b-docking
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

include(CMake/ShaderManifest.cmake)
file(GLOB_RECURSE MDSS_DISCOVERED_SHADER_ENTRIES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.tesc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.tese"
)
foreach(ShaderRelativePath IN LISTS MDSS_DISCOVERED_SHADER_ENTRIES)
    if(NOT ShaderRelativePath IN_LIST MDSS_SHADER_ENTRIES AND
       NOT ShaderRelativePath IN_LIST MDSS_SHADER_PLACEHOLDERS)
        message(FATAL_ERROR "Unregistered shader entry point: ${ShaderRelativePath}")
    endif()
endforeach()

file(GLOB_RECURSE MDSS_SHADER_INCLUDES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.glsl"
)

# Keep shader binaries configuration-specific so Debug validation SPIR-V cannot
# be reused by a Release build (or vice versa).
set(MDSS_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/Shaders/$<CONFIG>")
set(MDSS_SHADER_BINARIES)

function(mdss_catalog_add ShaderRelativePath VariantName)
    string(REPLACE "/" "_" ShaderId "${ShaderRelativePath}")
    string(REPLACE "." "_" ShaderId "${ShaderId}")
    if(NOT "${VariantName}" STREQUAL "")
        set(ShaderId "${ShaderId}_${VariantName}")
        set(ShaderRelativePath "${ShaderRelativePath}.${VariantName}")
    endif()
    set_property(GLOBAL APPEND PROPERTY MDSS_SHADER_CATALOG "${ShaderId}|${ShaderRelativePath}.spv")
endfunction()

function(mdss_add_shader_variant ShaderRelativePath VariantName)
    set(ShaderSource "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${ShaderRelativePath}")
    set(ShaderBinary "${MDSS_SHADER_OUTPUT_DIR}/${ShaderRelativePath}.${VariantName}.spv")
    get_filename_component(ShaderOutputDirectory "${ShaderBinary}" DIRECTORY)
    add_custom_command(
        OUTPUT "${ShaderBinary}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ShaderOutputDirectory}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/Source"
                "-DMDSS_GPU_VALIDATION=$<IF:$<CONFIG:Debug>,1,0>"
                ${ARGN}
                "${ShaderSource}"
                -o "${ShaderBinary}"
        DEPENDS "${ShaderSource}" ${MDSS_SHADER_INCLUDES}
                "${CMAKE_CURRENT_SOURCE_DIR}/Source/SurfaceState/Types/SurfaceSolverRates.h"
        COMMENT "Compiling shader ${ShaderRelativePath}.${VariantName}"
        VERBATIM
    )
    set(MDSS_SHADER_BINARIES ${MDSS_SHADER_BINARIES} "${ShaderBinary}" PARENT_SCOPE)
    mdss_catalog_add("${ShaderRelativePath}" "${VariantName}")
endfunction()

foreach(ShaderRelativePath IN LISTS MDSS_SHADER_ENTRIES)
    set(ShaderSource "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${ShaderRelativePath}")
    if(NOT EXISTS "${ShaderSource}")
        message(FATAL_ERROR "Shader manifest references missing source: ${ShaderRelativePath}")
    endif()
    if(ShaderRelativePath IN_LIST MDSS_VARIANT_SHADERS)
        continue()
    endif()
    set(ShaderBinary "${MDSS_SHADER_OUTPUT_DIR}/${ShaderRelativePath}.spv")
    get_filename_component(ShaderOutputDirectory "${ShaderBinary}" DIRECTORY)

    add_custom_command(
        OUTPUT "${ShaderBinary}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ShaderOutputDirectory}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/Source"
                "-DMDSS_GPU_VALIDATION=$<IF:$<CONFIG:Debug>,1,0>"
                "${ShaderSource}"
                -o "${ShaderBinary}"
        DEPENDS "${ShaderSource}" ${MDSS_SHADER_INCLUDES}
                "${CMAKE_CURRENT_SOURCE_DIR}/Source/SurfaceState/Types/SurfaceSolverRates.h"
        COMMENT "Compiling shader ${ShaderRelativePath}"
        VERBATIM
    )

    list(APPEND MDSS_SHADER_BINARIES "${ShaderBinary}")
    mdss_catalog_add("${ShaderRelativePath}" "")
endforeach()

mdss_register_shader_variants()

get_property(MDSS_SHADER_CATALOG GLOBAL PROPERTY MDSS_SHADER_CATALOG)
set(MDSS_SHADER_CATALOG_HEADER "#pragma once\n#include <string>\n#include <stdexcept>\n\nnamespace MDSS::Shaders\n{\n    enum class EShader\n    {\n")
foreach(Entry IN LISTS MDSS_SHADER_CATALOG)
    string(REPLACE "|" ";" Parts "${Entry}")
    list(GET Parts 0 Id)
    string(APPEND MDSS_SHADER_CATALOG_HEADER "        ${Id},\n")
endforeach()
string(APPEND MDSS_SHADER_CATALOG_HEADER "    };\n\n    inline std::string Path(EShader Shader)\n    {\n        switch (Shader)\n        {\n")
foreach(Entry IN LISTS MDSS_SHADER_CATALOG)
    string(REPLACE "|" ";" Parts "${Entry}")
    list(GET Parts 0 Id)
    list(GET Parts 1 RelativePath)
    string(APPEND MDSS_SHADER_CATALOG_HEADER "            case EShader::${Id}: return std::string(MDSS_SHADER_DIR) + \"/${RelativePath}\";\n")
endforeach()
string(APPEND MDSS_SHADER_CATALOG_HEADER "        }\n        throw std::invalid_argument(\"Unknown shader id\");\n    }\n}\n")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/ShaderCatalog.h" CONTENT "${MDSS_SHADER_CATALOG_HEADER}")

add_custom_target(MDSS_Shaders DEPENDS ${MDSS_SHADER_BINARIES})
add_dependencies(MDSS MDSS_Shaders)

target_compile_definitions(MDSS PRIVATE
    MDSS_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}"
    MDSS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
    MDSS_SURFACE_CACHE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Cache/Surface"
    MDSS_ENABLE_VALIDATION=$<BOOL:${MDSS_ENABLE_VALIDATION}>
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)
target_include_directories(MDSS PRIVATE "${CMAKE_BINARY_DIR}/generated")

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
    set(MDSS_SURFACE_PACKING_SOURCES
        Source/SurfaceState/GPU/Packing/SharedGeometryPacking.cpp
        Source/SurfaceState/GPU/Packing/TransferWeightPacking.cpp
        Source/SurfaceState/GPU/Packing/ProfilePacking.cpp
        Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp
    )
    set(MDSS_SURFACE_RESOURCE_SOURCES
        Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp
        Source/SurfaceState/GPU/Resources/ProfileResources.cpp
        Source/SurfaceState/GPU/Resources/InstanceResources.cpp
        Source/SurfaceState/GPU/Resources/DescriptorResources.cpp
    )
    add_executable(MDSS_SimulationTransportTests
        Tests/SimulationTransportTests.cpp
        Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp
        ${MDSS_SURFACE_PACKING_SOURCES}
        Source/SurfaceState/Types/SurfaceStateRegistry.cpp
        Source/SurfaceState/Types/SurfaceStateTypes.cpp
        Source/SurfaceState/Types/SurfaceReactions.cpp
    )
    target_include_directories(MDSS_SimulationTransportTests PRIVATE Source)
    target_link_libraries(MDSS_SimulationTransportTests PRIVATE glm::glm)
    add_test(NAME MDSS_SimulationTransport COMMAND MDSS_SimulationTransportTests)

    option(MDSS_BUILD_VULKAN_SCENE_TESTS
        "Build and register Scene/Renderer tests that require a native Vulkan window"
        OFF
    )
    if(MDSS_BUILD_VULKAN_SCENE_TESTS)
        # Exercise the actual Scene/AssetManager/Renderer resource lifecycle with a small native Vulkan window.
        set(MDSS_SCENE_TEST_SOURCES ${MDSS_SOURCES})
        list(FILTER MDSS_SCENE_TEST_SOURCES EXCLUDE REGEX "/main\\.cpp$")
        add_executable(MDSS_SceneResourceTests Tests/SceneResourceTests.cpp Tests/SurfaceDebugRenderingTests.cpp ${MDSS_SCENE_TEST_SOURCES})
        add_dependencies(MDSS_SceneResourceTests MDSS_Shaders)
        target_include_directories(MDSS_SceneResourceTests PRIVATE Source)
        target_include_directories(MDSS_SceneResourceTests PRIVATE "${CMAKE_BINARY_DIR}/generated")
        target_include_directories(MDSS_SceneResourceTests SYSTEM PRIVATE "${tinyobjloader_SOURCE_DIR}" "${stb_SOURCE_DIR}")
        target_link_libraries(MDSS_SceneResourceTests PRIVATE MDSS_ImGui glfw Vulkan::Vulkan glm::glm nlohmann_json::nlohmann_json)
        target_compile_definitions(MDSS_SceneResourceTests PRIVATE
            MDSS_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}"
            MDSS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
            MDSS_SURFACE_CACHE_DIR="${CMAKE_CURRENT_BINARY_DIR}/SceneTestCache"
            MDSS_ENABLE_VALIDATION=$<BOOL:${MDSS_ENABLE_VALIDATION}>
            GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE
        )
        if(MSVC)
            target_compile_options(MDSS_SceneResourceTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(MDSS_SceneResourceTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME MDSS_SceneResources COMMAND MDSS_SceneResourceTests)
        add_test(NAME MDSS_ScratchDescriptorLifetime COMMAND MDSS_SceneResourceTests --scratch-layout-only)
        set_tests_properties(MDSS_ScratchDescriptorLifetime PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME MDSS_DemoAnimation COMMAND MDSS_SceneResourceTests --demo-animation)
        add_test(NAME MDSS_CavityFillDisplayBound COMMAND MDSS_SceneResourceTests --cavity-fill-bound)
        add_test(NAME MDSS_LavaScene COMMAND MDSS_SceneResourceTests --lava-scene)
        add_test(NAME MDSS_OverlaySideCompaction COMMAND MDSS_SceneResourceTests --overlay-side-compaction)
        set_tests_properties(MDSS_SceneResources PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(MDSS_OverlaySideCompaction PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(MDSS_LavaScene PROPERTIES SKIP_RETURN_CODE 77)
    endif()

    add_executable(MDSS_SurfaceCacheTests
        Tests/SurfaceCacheTests.cpp
        Source/AssetManager/Loaders/MTLLoader.cpp
        Source/AssetManager/Loaders/OBJLoader.cpp
        Source/AssetManager/Loaders/TextureLoader.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp
        Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp
        Source/SurfaceState/Cache/SurfaceCache.cpp
        ${MDSS_SURFACE_PACKING_SOURCES}
        Source/SurfaceState/Types/SurfaceStateRegistry.cpp
        Source/SurfaceState/Types/SurfaceStateTypes.cpp
        Source/SurfaceState/Types/SurfaceReactions.cpp
    )
    target_include_directories(MDSS_SurfaceCacheTests PRIVATE Source)
    target_include_directories(MDSS_SurfaceCacheTests SYSTEM PRIVATE "${tinyobjloader_SOURCE_DIR}" "${stb_SOURCE_DIR}")
    target_link_libraries(MDSS_SurfaceCacheTests PRIVATE glm::glm)
    target_compile_definitions(MDSS_SurfaceCacheTests PRIVATE
        MDSS_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
    )
    if(MSVC)
        target_compile_options(MDSS_SurfaceCacheTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SurfaceCacheTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SurfaceCache COMMAND MDSS_SurfaceCacheTests)

    add_executable(MDSS_SurfaceStateTests
        Tests/SurfaceStateContractTests.cpp
        Source/AssetManager/Core/Asset.cpp
        Source/AssetManager/Assets/SRProfileAsset.cpp
        Source/AssetManager/Loaders/SRProfileLoader.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp
        Source/SurfaceState/State/SurfaceInstanceStateData.cpp
        Source/SurfaceState/Types/SurfaceStateRegistry.cpp
        Source/SurfaceState/Types/SurfaceStateTypes.cpp
        Source/SurfaceState/Types/SurfaceReactions.cpp
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
        Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
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

    add_executable(MDSS_NormalMapTransferTests
        Tests/NormalMapTransferTests.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp
    )
    target_include_directories(MDSS_NormalMapTransferTests PRIVATE Source)
    target_link_libraries(MDSS_NormalMapTransferTests PRIVATE glm::glm)
    if(MSVC)
        target_compile_options(MDSS_NormalMapTransferTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_NormalMapTransferTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_NormalMapTransfer COMMAND MDSS_NormalMapTransferTests)

    add_executable(MDSS_SharedSurfaceGeometryTests
        Tests/SharedSurfaceGeometryTests.cpp
        Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp
        Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp
        Source/AssetManager/Loaders/MTLLoader.cpp
        Source/AssetManager/Loaders/OBJLoader.cpp
        Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp
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
        Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp
        Source/Logger/Logger.cpp
        Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp
        ${MDSS_SURFACE_PACKING_SOURCES}
        ${MDSS_SURFACE_RESOURCE_SOURCES}
        Source/SurfaceState/Solver/SurfaceStateSolver.cpp
        Source/SurfaceState/Types/SurfaceStateRegistry.cpp
        Source/SurfaceState/Types/SurfaceStateTypes.cpp
        Source/SurfaceState/Types/SurfaceReactions.cpp
        Source/GPU/Vulkan/Resource/GPUBuffer.cpp
    )
    add_dependencies(MDSS_SurfaceGPUResourceTests MDSS_Shaders)
    target_include_directories(MDSS_SurfaceGPUResourceTests PRIVATE Source)
    target_include_directories(MDSS_SurfaceGPUResourceTests PRIVATE "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(MDSS_SurfaceGPUResourceTests PRIVATE Vulkan::Vulkan glm::glm)
    target_compile_definitions(MDSS_SurfaceGPUResourceTests PRIVATE
        MDSS_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}"
    )
    if(MSVC)
        target_compile_options(MDSS_SurfaceGPUResourceTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MDSS_SurfaceGPUResourceTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MDSS_SurfaceGPUResource COMMAND MDSS_SurfaceGPUResourceTests)
    add_test(NAME MDSS_LocalReactions COMMAND MDSS_SurfaceGPUResourceTests --reactions-only)
    set_tests_properties(MDSS_LocalReactions PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME MDSS_MesoPeakPacking COMMAND MDSS_SurfaceGPUResourceTests --meso-peak-only)
    set_tests_properties(MDSS_SurfaceGPUResource PROPERTIES SKIP_RETURN_CODE 77)

    # Explicitly invoked headless study runner; uses the production mesh mapping and GPU solver.
    get_target_property(MDSS_RESEARCH_SOURCES MDSS_SurfaceGPUResourceTests SOURCES)
    list(REMOVE_ITEM MDSS_RESEARCH_SOURCES Tests/SurfaceGPUResourceTests.cpp)
    include(Scripts/Research/MesoStudy.cmake)
    add_executable(MDSS_CouplingStudy Scripts/Research/CouplingStudy.cpp ${MDSS_RESEARCH_SOURCES}
        Source/AssetManager/Core/Asset.cpp
        Source/AssetManager/Loaders/OBJLoader.cpp
        Source/AssetManager/Loaders/MTLLoader.cpp
        Source/AssetManager/Loaders/SRProfileLoader.cpp
        Source/AssetManager/Assets/SRProfileAsset.cpp
        Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp
        Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp
        Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp
        ${MesoGenerated}
        Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp
        Source/AssetManager/Loaders/TextureLoader.cpp)
    add_dependencies(MDSS_CouplingStudy MDSS_Shaders)
    target_include_directories(MDSS_CouplingStudy PRIVATE Source "${CMAKE_BINARY_DIR}/generated")
    target_include_directories(MDSS_CouplingStudy SYSTEM PRIVATE "${tinyobjloader_SOURCE_DIR}" "${stb_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/delaunator")
    target_link_libraries(MDSS_CouplingStudy PRIVATE Vulkan::Vulkan glm::glm nlohmann_json::nlohmann_json)
    target_compile_definitions(MDSS_CouplingStudy PRIVATE MDSS_SHADER_DIR="${MDSS_SHADER_OUTPUT_DIR}")
    include(Scripts/Research/ModularShaders.cmake)

    # Explicit read-only fixture probe, separate from the rendered application and GPU research runner.
    add_executable(MDSS_AuthoringProbe Scripts/Research/AuthoringProbe.cpp
        Source/AssetManager/Core/Asset.cpp
        Source/AssetManager/Assets/SRProfileAsset.cpp
        Source/AssetManager/Loaders/SRProfileLoader.cpp
        Source/SurfaceState/Types/SurfaceStateRegistry.cpp
        Source/SurfaceState/Types/SurfaceStateTypes.cpp
        Source/SurfaceState/Types/SurfaceReactions.cpp)
    target_include_directories(MDSS_AuthoringProbe PRIVATE Source)
    target_link_libraries(MDSS_AuthoringProbe PRIVATE glm::glm nlohmann_json::nlohmann_json)
endif()

# Vendored triangulation is used only by the source-topology render mesh builder.
foreach(Target MDSS MDSS_SceneResourceTests MDSS_SharedSurfaceGeometryTests MDSS_SurfaceGPUResourceTests)
    if(TARGET ${Target})
        target_include_directories(${Target} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/delaunator")
    endif()
endforeach()
