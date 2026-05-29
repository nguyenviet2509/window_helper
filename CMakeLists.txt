cmake_minimum_required(VERSION 3.21)

# Toolchain (vcpkg) must be set before project()
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

project(pt-automation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

# Phase 0: PostMessage probe (standalone, no vcpkg deps)
add_subdirectory(tools/postmessage-probe)

# Phase 1+: WindowHelper main exe (gated until deps installed)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

# Mock game for testing
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mock/CMakeLists.txt")
    add_subdirectory(mock)
endif()
