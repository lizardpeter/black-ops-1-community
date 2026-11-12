target_sources(KisakBlack PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
set_target_properties(KisakBlack PROPERTIES OUTPUT_NAME "BlackOpsMP-Community")

find_package(Git REQUIRED)
set(upstream_dir "${PROJECT_SOURCE_DIR}/upstream/kisakblack")
set(overlay_dir "${CMAKE_CURRENT_BINARY_DIR}/kisakblack")
set(overlay_patch "${CMAKE_CURRENT_SOURCE_DIR}/patches/kisakblack.patch")
set(overlay_units
    src/database/db_load.cpp
    src/client_mp/cl_main_mp.cpp
    src/client_mp/cl_scrn_mp.cpp
    src/client_mp/sv_client_mp.cpp
    src/win32/win_main.cpp
    src/cgame/cg_camera.cpp
    src/cgame_mp/cg_main_mp.cpp
    src/gfx_d3d/r_init.cpp
    src/gfx_d3d/r_state.cpp
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

set(replaced_units
    src/live/live_steam.cpp
    src/live/live_steam_achievements.cpp
    src/live/live_steam_client.cpp
    src/live/live_steam_server.cpp
    src/live/live_steamp2p.cpp
    src/live/live_friends_pc.cpp
    src/win32/win_steam.cpp
    src/gfx_d3d/r_cinematic.cpp
    src/binklib/binktextures.cpp
)
foreach(unit IN LISTS replaced_units)
    if(NOT "${upstream_dir}/${unit}" IN_LIST engine_sources)
        message(FATAL_ERROR "Review enhanced backend replacement for ${unit}")
    endif()
    list(REMOVE_ITEM engine_sources "${upstream_dir}/${unit}")
endforeach()
set_property(TARGET KisakBlack PROPERTY SOURCES "${engine_sources}")
get_target_property(engine_libraries KisakBlack LINK_LIBRARIES)
get_target_property(engine_interface KisakBlack INTERFACE_LINK_LIBRARIES)
foreach(library IN ITEMS xinput.lib steam_api.lib binkw32.lib)
    if(NOT "${library}" IN_LIST engine_libraries)
        message(FATAL_ERROR "Review enhanced linkage for ${library}")
    endif()
    list(REMOVE_ITEM engine_libraries "${library}")
    list(REMOVE_ITEM engine_interface "${library}")
endforeach()
set_property(TARGET KisakBlack PROPERTY LINK_LIBRARIES "${engine_libraries}")
set_property(TARGET KisakBlack PROPERTY INTERFACE_LINK_LIBRARIES "${engine_interface}")
target_include_directories(KisakBlack PRIVATE "${PROJECT_SOURCE_DIR}")
target_sources(KisakBlack PRIVATE
    cgame/fov.cpp
    gfx_d3d/cinematic_unavailable.cpp
    gfx_d3d/material_capture.cpp
    live/local_platform.cpp
    win32/win_rawinput.cpp
)

add_executable(bo1_fov_test "${PROJECT_SOURCE_DIR}/tests/enhanced/fov_test.cpp")
target_compile_features(bo1_fov_test PRIVATE cxx_std_17)
target_include_directories(bo1_fov_test PRIVATE "${PROJECT_SOURCE_DIR}")
add_test(NAME bo1_fov COMMAND bo1_fov_test)
