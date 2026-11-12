cmake_minimum_required(VERSION 3.21)
project(BlackOps1Community LANGUAGES NONE)

option(BO1_COMMUNITY_ENHANCED "Build the enhanced multiplayer executable" OFF)

if(NOT CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022")
    message(FATAL_ERROR "Use a Visual Studio 2022 preset.")
endif()
if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32" AND
   NOT (CMAKE_GENERATOR_PLATFORM STREQUAL "x64" AND BO1_COMMUNITY_ENHANCED))
    message(FATAL_ERROR "Use a Win32 preset or the enhanced-x64 preset.")
endif()

set(kisakblack_source "${CMAKE_CURRENT_SOURCE_DIR}/upstream/kisakblack")
if(NOT EXISTS "${kisakblack_source}/CMakeLists.txt")
    message(FATAL_ERROR "Initialize KisakBlack with git submodule update --init --recursive.")
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported build configurations")
enable_language(C CXX RC)

set(bo1_engine_source "${kisakblack_source}")
if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(bo1_engine_source "${CMAKE_CURRENT_BINARY_DIR}/kisakblack-x64")
    add_subdirectory(enhanced/port/x64/game)
else()
    add_subdirectory(upstream/kisakblack)
endif()

set_target_properties(KisakBlack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

if(BO1_COMMUNITY_ENHANCED)
    enable_testing()
    add_subdirectory(enhanced)
endif()
