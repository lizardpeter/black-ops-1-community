target_sources(KisakBlack PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
set_target_properties(KisakBlack PROPERTIES OUTPUT_NAME "BlackOpsMP-Community")

find_package(Git REQUIRED)
set(upstream_dir "${PROJECT_SOURCE_DIR}/upstream/kisakblack")
set(overlay_dir "${CMAKE_CURRENT_BINARY_DIR}/kisakblack")
set(overlay_patch "${CMAKE_CURRENT_SOURCE_DIR}/patches/kisakblack.patch")
set(overlay_units
    src/cgame/cg_camera.cpp
    src/cgame_mp/cg_main_mp.cpp
    src/gfx_d3d/r_init.cpp
    src/win32/win_gamepad.cpp
    src/win32/win_gamerprofile.cpp
    src/win32/win_input.cpp
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${overlay_patch}")
get_target_property(engine_sources KisakBlack SOURCES)
foreach(unit IN LISTS overlay_units)
    set(original "${upstream_dir}/${unit}")
    set(replacement "${overlay_dir}/${unit}")
    list(FIND engine_sources "${original}" source_index)
    if(source_index EQUAL -1)
        message(FATAL_ERROR "Enhanced source override missing from KisakBlack: ${unit}")
    endif()
    configure_file("${original}" "${replacement}" COPYONLY)
    get_filename_component(original_directory "${original}" DIRECTORY)
    set_source_files_properties("${replacement}" TARGET_DIRECTORY KisakBlack PROPERTIES
        INCLUDE_DIRECTORIES "${original_directory}")
    list(REMOVE_AT engine_sources ${source_index})
    list(INSERT engine_sources ${source_index} "${replacement}")
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_BINARY_DIR}"
        "${GIT_EXECUTABLE}" apply --ignore-space-change "${overlay_patch}"
    WORKING_DIRECTORY "${overlay_dir}"
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Cannot apply the KisakBlack enhancement patch:\n${patch_error}")
endif()

set_property(TARGET KisakBlack PROPERTY SOURCES "${engine_sources}")
get_target_property(engine_libraries KisakBlack LINK_LIBRARIES)
if(NOT "xinput.lib" IN_LIST engine_libraries)
    message(FATAL_ERROR "Review the upstream XInput linkage before building the enhanced edition")
endif()
list(REMOVE_ITEM engine_libraries "xinput.lib")
set_property(TARGET KisakBlack PROPERTY LINK_LIBRARIES "${engine_libraries}")
get_target_property(engine_interface KisakBlack INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM engine_interface "xinput.lib")
set_property(TARGET KisakBlack PROPERTY INTERFACE_LINK_LIBRARIES "${engine_interface}")
target_include_directories(KisakBlack PRIVATE "${PROJECT_SOURCE_DIR}")
target_sources(KisakBlack PRIVATE cgame/fov.cpp win32/win_rawinput.cpp)

add_executable(bo1_fov_test "${PROJECT_SOURCE_DIR}/tests/enhanced/fov_test.cpp")
target_compile_features(bo1_fov_test PRIVATE cxx_std_17)
target_include_directories(bo1_fov_test PRIVATE "${PROJECT_SOURCE_DIR}")
add_test(NAME bo1_fov COMMAND bo1_fov_test)
