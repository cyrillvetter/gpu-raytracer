set(SHADERS 
        "main.slang"
        # Add more shaders here

        # Tutorial shaders
        "tutorial/01_hello_triangle.slang"
        "tutorial/02_hello_sphere.slang"
        "tutorial/03_camera.slang"
        "tutorial/04_diffuse_lighting.slang"
        "tutorial/05_anti_aliasing.slang"
        "tutorial/06_reflection.slang"
        "tutorial/07_passing_vertex_data.slang"

        # Example shaders
        "examples/pathtracer.slang"
)

set(SPV_CAPABILITIES
        "spvRayTracingKHR"
        "spvRayTracingPositionFetchKHR"
        "SPV_KHR_non_semantic_info"
        "SPV_GOOGLE_user_type"
        "spvDerivativeControl"
        "spvImageQuery"
        "spvImageGatherExtended"
        "spvSparseResidency"
        "spvMinLod"
        "spvFragmentFullyCoveredEXT"
        "spvShaderNonUniformEXT"
)

set(CAPABILITIES)
foreach(capability IN LISTS SPV_CAPABILITIES)
        list(APPEND CAPABILITIES -capability ${capability})
endforeach()

function (add_slang_shader_target TARGET)
        # Get absolute paths
        get_filename_component(SHADER_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
        get_filename_component(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}" ABSOLUTE)

        message(STATUS "Shader input directory: " "${SHADER_INPUT_DIR}")
        message(STATUS "Shader output directory: " "${SHADER_OUTPUT_DIR}")

        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

        set(COMPILED_SHADERS "")

        foreach(shader_file IN LISTS SHADERS)
                get_filename_component(SOURCE_PATH "${SHADER_INPUT_DIR}/${shader_file}" ABSOLUTE)

                get_filename_component(DEST_PATH_SLANG "${SHADER_OUTPUT_DIR}/${shader_file}" ABSOLUTE)
                cmake_path(REPLACE_EXTENSION DEST_PATH_SLANG
                        ".spv"
                        OUTPUT_VARIABLE DEST_PATH
                )

                add_custom_command(
                        OUTPUT "${DEST_PATH}"
                        COMMAND slangc "${SOURCE_PATH}" -target spirv -profile spirv_1_4 ${CAPABILITIES} -emit-spirv-directly -o "${DEST_PATH}"
                        DEPENDS "${SOURCE_PATH}"
                        COMMENT "Compiling shader: ${shader_file}"
                        VERBATIM
                )

                list(APPEND COMPILED_SHADERS "${DEST_PATH}")
        endforeach()

        # Add custom target that depends on all compiled shaders.
        add_custom_target(${TARGET} ALL DEPENDS ${COMPILED_SHADERS})
endfunction()

add_slang_shader_target(ShaderCompilation)
