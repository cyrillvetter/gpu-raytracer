cmake_minimum_required (VERSION 3.24)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(RayTracer VERSION 0.1)

find_package(Vulkan REQUIRED)

# Setup GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY  https://github.com/glfw/glfw.git
        GIT_TAG         3.4
        GIT_SHALLOW     TRUE
        GIT_PROGRESS    TRUE
)

# Disable GLFW building examples, tests, and docs to keep build lightweight
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Setup GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY  https://github.com/g-truc/glm.git
        GIT_TAG         1.0.2
        GIT_SHALLOW     TRUE
        GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(glm)

# Setup fastgltf
FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY  https://github.com/spnda/fastgltf.git
        GIT_TAG         v0.9.0
        GIT_SHALLOW     TRUE
        GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(fastgltf)

add_subdirectory(engine)
add_subdirectory(raytracer)
add_subdirectory(shaders)
