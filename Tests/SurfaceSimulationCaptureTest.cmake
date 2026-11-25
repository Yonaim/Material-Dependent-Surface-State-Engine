if(NOT DEFINED MDSS_EXECUTABLE OR NOT DEFINED CAPTURE_DIR)
    message(FATAL_ERROR "SurfaceSimulationCaptureTest requires MDSS_EXECUTABLE and CAPTURE_DIR.")
endif()

foreach(mode IN ITEMS textured untextured)
    set(options)
    if(mode STREQUAL "untextured")
        list(APPEND options --surface-no-textures)
    endif()
    set(capture "${CAPTURE_DIR}/${mode}.png")
    execute_process(
        COMMAND "${MDSS_EXECUTABLE}" --capture-surface --surface-scene kiln --surface-shading flat
                --frames 125 --surface-capture "${capture}" ${options}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output_errors
        TIMEOUT 45
    )
    if(NOT result STREQUAL "0" OR output MATCHES "Validation Error|SYNC-HAZARD"
       OR output_errors MATCHES "Validation Error|SYNC-HAZARD")
        message(FATAL_ERROR "Surface Simulation capture failed (${mode}, ${result}):
${output}
${output_errors}")
    endif()

    # PNG color type 2 stores opaque RGB. The tested frame includes water and active emissions.
    file(READ "${capture}" header HEX LIMIT 26)
    if(NOT header MATCHES "^89504e470d0a1a0a0000000d49484452")
        message(FATAL_ERROR "Expected a PNG capture for ${mode}.")
    endif()
    string(SUBSTRING "${header}" 50 2 color_type)
    if(NOT color_type STREQUAL "02")
        message(FATAL_ERROR "Expected an opaque RGB capture for ${mode}; PNG color type is ${color_type}.")
    endif()
    file(SHA256 "${capture}" hash_${mode})
endforeach()

if(hash_textured STREQUAL hash_untextured)
    message(FATAL_ERROR "Material texture toggle produced identical captures.")
endif()
