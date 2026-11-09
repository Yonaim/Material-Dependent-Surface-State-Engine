# Reuse the exact channel-local numerical provider body; change only the selected channel range.
# Generated files are research-only and are not product shader catalog entries.
set(MDSS_MODULAR_OUTPUT "${CMAKE_BINARY_DIR}/research/$<CONFIG>")
set(ModularBinaries)
foreach(Pass IN ITEMS 1 2)
    set(Original "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Simulation/Solver/SurfaceSolverPass${Pass}.comp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${Original}")
    file(READ "${Original}" Body)
    set(Loop "for (uint ChannelIndex = 0u; ChannelIndex < Solver.StateChannelCount; ++ChannelIndex)")
    string(FIND "${Body}" "${Loop}" Found)
    if(Found LESS 0)
        message(FATAL_ERROR "Research provider extraction cannot find the channel loop in ${Original}")
    endif()
    string(REPLACE "${Loop}" "for (uint ChannelIndex = modularBegin(); ChannelIndex < modularEnd(); ++ChannelIndex)" Body "${Body}")
    set(Selector "\n// High bits are private to the research transport provider; 0 selects the fused channel batch.\nuint modularBegin() { uint C = Solver.Flags >> 29u; return C == 0u ? 0u : C-1u; }\nuint modularEnd() { uint C = Solver.Flags >> 29u; return C == 0u ? Solver.StateChannelCount : C; }\n")
    string(REPLACE "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;" "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n${Selector}" Body "${Body}")
    set(Generated "${CMAKE_BINARY_DIR}/research/ModulePass${Pass}.comp")
    file(WRITE "${Generated}" "${Body}")
    set(Binary "${MDSS_MODULAR_OUTPUT}/ModulePass${Pass}.spv")
    add_custom_command(OUTPUT "${Binary}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${MDSS_MODULAR_OUTPUT}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/Source" "-DMDSS_GPU_VALIDATION=$<IF:$<CONFIG:Debug>,1,0>"
            "${Generated}" -o "${Binary}"
        DEPENDS "${Generated}" "${Original}" ${MDSS_SHADER_INCLUDES}
        VERBATIM)
    list(APPEND ModularBinaries "${Binary}")
endforeach()
add_custom_target(MDSS_ModularShaders DEPENDS ${ModularBinaries})
add_dependencies(MDSS_CouplingStudy MDSS_ModularShaders)
target_compile_definitions(MDSS_CouplingStudy PRIVATE MDSS_MODULAR_SHADER_DIR="${MDSS_MODULAR_OUTPUT}")
