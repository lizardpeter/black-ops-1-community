# Reuse the pinned engine source list and target settings for the x64 port.
set(upstream_cmake "${kisakblack_source}/CMakeLists.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${upstream_cmake}")
file(READ "${upstream_cmake}" engine_cmake)

# The upstream build describes a Win32 executable. Adjust architecture-specific
# settings before evaluating it in this directory.
string(REPLACE [[set(CMAKE_GENERATOR_PLATFORM "WIN32")]] "" engine_cmake "${engine_cmake}")
string(REPLACE [[${CMAKE_CURRENT_SOURCE_DIR}]] "${kisakblack_source}" engine_cmake "${engine_cmake}")
string(REPLACE [[include(cmake_files.cmake)]] "include(\"${kisakblack_source}/cmake_files.cmake\")" engine_cmake "${engine_cmake}")
string(REPLACE "/Lib/x86" "/Lib/x64" engine_cmake "${engine_cmake}")
string(REPLACE "/machine:x86" "/machine:x64" engine_cmake "${engine_cmake}")
string(REPLACE [[COMPILE_OPTIONS "/arch:IA32"]] [[COMPILE_OPTIONS ""]] engine_cmake "${engine_cmake}")
string(REPLACE "/SAFESEH:NO" "" engine_cmake "${engine_cmake}")
string(REPLACE "/ZI" "/Zi" engine_cmake "${engine_cmake}")
string(REPLACE "nvapi.lib" "nvapi64.lib" engine_cmake "${engine_cmake}")

set(BO1_X64_VPX_LIBRARY "" CACHE FILEPATH "Path to an x64 static libvpx built with the bundled VPX 1.5 headers")
string(REPLACE "    libvpx.lib" "" engine_cmake "${engine_cmake}")
string(REPLACE "target_link_directories(KisakBlack PUBLIC \"${kisakblack_source}/src/libs/libvpx-1.5.0\")" "" engine_cmake "${engine_cmake}")

cmake_language(EVAL CODE "${engine_cmake}")
if(BO1_X64_VPX_LIBRARY)
    target_link_libraries(KisakBlack PUBLIC "${BO1_X64_VPX_LIBRARY}")
endif()
