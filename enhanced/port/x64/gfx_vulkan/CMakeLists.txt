cmake_minimum_required(VERSION 3.21)
project(BO1VulkanBringup LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Vulkan bring-up target requires Windows x64.")
endif()

find_package(Vulkan REQUIRED)
add_executable(bo1_vulkan_probe main.cpp)
target_compile_features(bo1_vulkan_probe PRIVATE cxx_std_17)
target_compile_definitions(bo1_vulkan_probe PRIVATE NOMINMAX VK_USE_PLATFORM_WIN32_KHR)
target_link_libraries(bo1_vulkan_probe PRIVATE Vulkan::Vulkan user32)
set_property(TARGET bo1_vulkan_probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
