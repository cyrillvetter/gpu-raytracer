add_library(engine STATIC)

target_sources(engine
        PUBLIC
                include/engine/acceleration_structure_builder.hpp
                include/engine/context.hpp
                include/engine/device.hpp
                include/engine/engine.hpp
                include/engine/renderer.hpp
                include/engine/resources.hpp
                include/engine/shader.hpp

                # vulkan abstraction:
                include/engine/vulkan/acceleration_structure.hpp
                include/engine/vulkan/buffer.hpp
                include/engine/vulkan/command_buffers.hpp
                include/engine/vulkan/command_pool.hpp
                include/engine/vulkan/descriptor_pool.hpp
                include/engine/vulkan/descriptor_set.hpp
                include/engine/vulkan/device.hpp
                include/engine/vulkan/fence.hpp
                include/engine/vulkan/image.hpp
                include/engine/vulkan/instance.hpp
                include/engine/vulkan/pipeline.hpp
                include/engine/vulkan/push_constants.hpp
                include/engine/vulkan/sampler.hpp
                include/engine/vulkan/semaphore.hpp
                include/engine/vulkan/shader_binding_tables.hpp
                include/engine/vulkan/shader_module.hpp
                include/engine/vulkan/surface.hpp
                include/engine/vulkan/swapchain.hpp
                include/engine/vulkan/vulkan.hpp

        PRIVATE
                src/acceleration_structure_builder.cpp
                src/context.cpp
                src/device.cpp
                src/renderer.cpp
                src/resources.cpp
                src/shader.cpp

                # vulkan abstraction:
                src/vulkan/acceleration_structure.cpp
                src/vulkan/buffer.cpp
                src/vulkan/command_buffers.cpp
                src/vulkan/command_pool.cpp
                src/vulkan/descriptor_pool.cpp
                src/vulkan/descriptor_set.cpp
                src/vulkan/device.cpp
                src/vulkan/fence.cpp
                src/vulkan/image.cpp
                src/vulkan/instance.cpp
                src/vulkan/pipeline.cpp
                src/vulkan/push_constants.cpp
                src/vulkan/sampler.cpp
                src/vulkan/semaphore.cpp
                src/vulkan/shader_binding_tables.cpp
                src/vulkan/shader_module.cpp
                src/vulkan/surface.cpp
                src/vulkan/swapchain.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(engine
        PUBLIC
                GLM_FORCE_RIGHT_HANDED=1 GLM_FORCE_DEPTH_ZERO_TO_ONE=1 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES=1 GLM_ENABLE_EXPERIMENTAL=1 # GLM
                VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1 # Vulkan
)

set_target_properties(engine PROPERTIES CXX_STANDARD 20)
target_link_libraries(engine
        PUBLIC Vulkan::Vulkan Vulkan::Headers
        PRIVATE glm::glm
)
