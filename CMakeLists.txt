cmake_minimum_required(VERSION 4.1)
project(THE_RENDERER)

set(CMAKE_CXX_STANDARD 20)

# Vulkan
find_package(Vulkan REQUIRED)

# Fetch dependencies
include(FetchContent)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# VMA
FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
)
FetchContent_MakeAvailable(vma)

# Executable
add_executable(THE_RENDERER main.cpp "src/BVH.h" "src/BVH.cpp" "src/Renderer.h" "src/Renderer.cpp")

target_include_directories(THE_RENDERER PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${glfw_SOURCE_DIR}/include
        ${glm_SOURCE_DIR}
)

# Shader compilation
find_program(GLSLC glslc HINTS D:/Vulkan/Bin)

file(GLOB_RECURSE GLSL_FILES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/shaders/*.comp
)

foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV ${CMAKE_SOURCE_DIR}/shaders/${FILE_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(THE_RENDERER Shaders)

add_custom_command(TARGET THE_RENDERER POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPIRV_BINARY_FILES}
        $<TARGET_FILE_DIR:THE_RENDERER>
)

target_link_libraries(THE_RENDERER PRIVATE
        Vulkan::Vulkan
        glfw
        glm
        GPUOpen::VulkanMemoryAllocator
)

target_compile_features(THE_RENDERER PRIVATE cxx_std_20)