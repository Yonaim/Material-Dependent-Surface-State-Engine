# The only hand-maintained list of shader entry points and compiled variants.
# Paths are relative to Shaders/. Include-only .glsl files are dependencies,
# never independent SPIR-V outputs.
set(MDSS_SHADER_ENTRIES
    Debug/SurfaceDebug.frag
    Debug/SurfaceDebug.vert
    Rendering/Overlay/Geometry/TexelGeometry.comp
    Rendering/Overlay/Geometry/TexelGeometry.vert
    Rendering/Overlay/Geometry/TexelGeometryBaseline.comp
    Rendering/Overlay/Geometry/TexelGeometryHeight.comp
    Debug/TexelInspector.comp
    Rendering/Scene/Gizmo.frag
    Rendering/Scene/Gizmo.vert
    Rendering/Scene/StaticMesh.frag
    Rendering/Scene/StaticMesh.vert
    Rendering/Scene/WorldReference.frag
    Rendering/Scene/WorldReference.vert
    Rendering/Overlay/Shading/OverlayCombined.frag
    Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp
    Rendering/Overlay/Geometry/OverlayDrawReset.comp
    Rendering/Overlay/Shading/OverlayLava.frag
    Rendering/Overlay/Shading/OverlayMud.frag
    Rendering/Overlay/Coverage/OverlayOccupancyScan.comp
    Rendering/Overlay/Geometry/OverlaySide.vert
    Rendering/Overlay/Geometry/OverlaySideBoundary.comp
    Rendering/Overlay/Coverage/OverlaySideCoverage.comp
    Rendering/Overlay/Geometry/OverlayTop.vert
    Rendering/Overlay/Geometry/OverlayTopCommand.comp
    Rendering/Surface/Lighting/BaseSurfaceLit.frag
    Rendering/Surface/Smoothing/HeightFieldSmoothing.comp
    Rendering/Surface/StateTexture/RenderStateTexture.comp
    Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp
    Rendering/Surface/Lighting/SurfaceLit.frag
    Rendering/Surface/Lighting/SurfaceLit.vert
    Rendering/Surface/Lighting/TexelSurfaceLit.frag
    Rendering/Surface/Lighting/TexelSurfaceLit.vert
    Simulation/Geometry/SurfaceAccumulationHeight.comp
    Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp
    Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp
    Simulation/Solver/SurfaceSolverPass1.comp
    Simulation/Solver/SurfaceReaction.comp
    Simulation/Solver/SurfaceSolverPass2.comp
    Simulation/Scheduling/SurfaceSparseScheduleReset.comp
)

# Placeholder entry points have no #version and do not produce SPIR-V.
set(MDSS_SHADER_PLACEHOLDERS
    Simulation/Input/SurfaceInput.comp
)

set(MDSS_VARIANT_SHADER_NAMES
    SurfaceSolverPass1.comp
    SurfaceSolverPass2.comp
    SurfaceDynamicWeightsUpdate.comp
    HeightFieldSmoothing.comp
    OverlayCoverageSmoothing.comp
    RenderStateTextureSmoothing.comp
    SurfaceLit.frag
    BaseSurfaceLit.frag
    TexelSurfaceLit.frag
    OverlayCombined.frag
)

function(mdss_shader_path FileName OutputVariable)
    set(Matches)
    foreach(Entry IN LISTS MDSS_SHADER_ENTRIES)
        get_filename_component(EntryName "${Entry}" NAME)
        if(EntryName STREQUAL FileName)
            list(APPEND Matches "${Entry}")
        endif()
    endforeach()
    list(LENGTH Matches MatchCount)
    if(NOT MatchCount EQUAL 1)
        message(FATAL_ERROR "Expected one shader named ${FileName}; found ${MatchCount}")
    endif()
    list(GET Matches 0 Match)
    set(${OutputVariable} "${Match}" PARENT_SCOPE)
endfunction()

set(MDSS_VARIANT_SHADERS)
foreach(FileName IN LISTS MDSS_VARIANT_SHADER_NAMES)
    mdss_shader_path("${FileName}" ShaderRelativePath)
    list(APPEND MDSS_VARIANT_SHADERS "${ShaderRelativePath}")
endforeach()

macro(mdss_add_named_shader_variant ShaderFileName VariantName)
    mdss_shader_path("${ShaderFileName}" VariantShaderRelativePath)
    mdss_add_shader_variant("${VariantShaderRelativePath}" "${VariantName}" ${ARGN})
endmacro()

function(mdss_register_shader_variants)
    foreach(PassName SurfaceSolverPass1 SurfaceSolverPass2)
        mdss_add_named_shader_variant("${PassName}.comp" "runtime")
    endforeach()
    mdss_add_named_shader_variant("SurfaceDynamicWeightsUpdate.comp" "runtime")
    foreach(RawMode RANGE 0 4)
        foreach(WeightMode RANGE 0 1)
            foreach(PassName SurfaceSolverPass1 SurfaceSolverPass2)
                mdss_add_named_shader_variant("${PassName}.comp" "r${RawMode}w${WeightMode}"
                    "-DMDSS_RAW_MODE=${RawMode}" "-DMDSS_HALF_WEIGHTS=${WeightMode}")
            endforeach()
        endforeach()
    endforeach()
    foreach(WeightMode RANGE 0 1)
        mdss_add_named_shader_variant("SurfaceDynamicWeightsUpdate.comp" "w${WeightMode}"
            "-DMDSS_HALF_WEIGHTS=${WeightMode}")
    endforeach()
    foreach(HaloMode RANGE 0 1)
        mdss_add_named_shader_variant("HeightFieldSmoothing.comp" "h${HaloMode}"
            "-DMDSS_HEIGHT_HALO=${HaloMode}")
        mdss_add_named_shader_variant("OverlayCoverageSmoothing.comp" "h${HaloMode}"
            "-DMDSS_COVERAGE_HALO=${HaloMode}")
    endforeach()
    mdss_add_named_shader_variant("RenderStateTextureSmoothing.comp" "direct"
        "-DMDSS_SMOOTH_MODE=0" "-DMDSS_SMOOTH_HALO=0")
    foreach(AxisMode RANGE 1 2)
        foreach(HaloMode RANGE 0 1)
            mdss_add_named_shader_variant("RenderStateTextureSmoothing.comp" "a${AxisMode}h${HaloMode}"
                "-DMDSS_SMOOTH_MODE=${AxisMode}" "-DMDSS_SMOOTH_HALO=${HaloMode}")
        endforeach()
    endforeach()
    foreach(FragmentMode RANGE 0 2)
        foreach(FragmentName SurfaceLit BaseSurfaceLit TexelSurfaceLit)
            mdss_add_named_shader_variant("${FragmentName}.frag" "m${FragmentMode}"
                "-DMDSS_FRAGMENT_MODE=${FragmentMode}")
        endforeach()
        mdss_add_named_shader_variant("OverlayCombined.frag" "m${FragmentMode}"
            "-DMDSS_FRAGMENT_MODE=${FragmentMode}")
    endforeach()
    set(MDSS_SHADER_BINARIES ${MDSS_SHADER_BINARIES} PARENT_SCOPE)
endfunction()
