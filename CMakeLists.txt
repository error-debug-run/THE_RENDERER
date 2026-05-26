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
add_executable(THE_RENDERER main.cpp)

target_include_directories(THE_RENDERER PRIVATE
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(THE_RENDERER PRIVATE
        Vulkan::Vulkan
        glfw
        glm
        GPUOpen::VulkanMemoryAllocator
)