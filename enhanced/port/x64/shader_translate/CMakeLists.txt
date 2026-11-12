cmake_minimum_required(VERSION 3.28)
project(BO1ShaderTranslate LANGUAGES C CXX)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build the translator with static libraries" FORCE)
set(PROFILE_HLSL OFF CACHE BOOL "Skip the D3D11 backend" FORCE)
set(EFFECT_SUPPORT OFF CACHE BOOL "Skip Effect framework support" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_SDL2 ON)
FetchContent_Declare(mojoshader
    GIT_REPOSITORY https://github.com/icculus/mojoshader.git
    GIT_TAG ad5dff84830c2863c841f4b1f4e3df78c705b383
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(mojoshader)

add_executable(bo1_shader_translate main.cpp)
target_compile_features(bo1_shader_translate PRIVATE cxx_std_17)
target_include_directories(bo1_shader_translate PRIVATE "${mojoshader_SOURCE_DIR}")
target_link_libraries(bo1_shader_translate PRIVATE mojoshader)
if(UNIX)
    target_link_libraries(bo1_shader_translate PRIVATE m)
endif()

enable_testing()
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME bo1_shader_translate
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/fixture_test.py"
                "$<TARGET_FILE:bo1_shader_translate>")
endif()
