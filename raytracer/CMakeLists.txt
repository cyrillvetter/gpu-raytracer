# Ray tracer source files
set(RAYTRACER_SOURCES
        src/util/gltf_scene.cpp
        src/util/gltf_scene.hpp
        src/util/window.cpp
        src/util/window.hpp

        # Add more files here.
)

function (add_custom_executable NAME FILE_NAME)
        add_executable(${NAME} ${FILE_NAME})

        target_sources(${NAME}
                PRIVATE ${RAYTRACER_SOURCES}
        )
        target_compile_definitions(${NAME}
                PUBLIC
                        GLM_FORCE_RIGHT_HANDED=1 GLM_FORCE_DEPTH_ZERO_TO_ONE=1 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES=1 # GLM
                        GLFW_INCLUDE_VULKAN=1 # GLFW
        )
        target_link_libraries(${NAME} PRIVATE engine glm::glm glfw fastgltf::fastgltf)
endfunction()

# Main executable
add_custom_executable(raytracer src/main.cpp)

# Tutorials
add_custom_executable(01_hello_triangle src/tutorial/01_hello_triangle.cpp)
add_custom_executable(02_hello_sphere src/tutorial/02_hello_sphere.cpp)
add_custom_executable(03_camera src/tutorial/03_camera.cpp)
add_custom_executable(04_diffuse_lighting src/tutorial/04_diffuse_lighting.cpp)
add_custom_executable(05_anti_aliasing src/tutorial/05_anti_aliasing.cpp)
add_custom_executable(06_reflection src/tutorial/06_reflection.cpp)
add_custom_executable(07_passing_vertex_data src/tutorial/07_passing_vertex_data.cpp)

# Examples
add_custom_executable(pathtracer src/examples/pathtracer.cpp)
