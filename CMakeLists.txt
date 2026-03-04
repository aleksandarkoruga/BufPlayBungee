cmake_minimum_required(VERSION 3.5)
set(project_name "BufPlayBungee")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})

####################################################################################################
# load modules – this must come before project()
include(SuperColliderServerPlugin RESULT_VARIABLE server_plugin_found)
if(NOT server_plugin_found)
    message(FATAL_ERROR "Could not find server plugin functions module")
endif()

include(SuperColliderCompilerConfig RESULT_VARIABLE compiler_config_found)
if(NOT compiler_config_found)
    message(FATAL_ERROR "Could not find compiler config module")
endif()

# Windows - puts redistributable DLLs in install directory
include(InstallRequiredSystemLibraries)

sc_check_sc_path("${SC_PATH}")
message(STATUS "Found SuperCollider: ${SC_PATH}")
set(SC_PATH "${SC_PATH}" CACHE PATH
    "Path to SuperCollider source. Relative paths are treated as relative to this script" FORCE)

include("${SC_PATH}/SCVersion.txt")
message(STATUS "Building plugins for SuperCollider version: ${SC_VERSION}")

# set project here to avoid SCVersion.txt clobbering our version info
project(${project_name})
sc_do_initial_compiler_config() # do after setting project so compiler ID is available

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
    message(WARNING "No install prefix provided, defaulting to $BUILD_DIR/install")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()
message(STATUS "Install directory set to: ${CMAKE_INSTALL_PREFIX}")

####################################################################################################
# options
option(SUPERNOVA "Build plugins for supernova" ON)
option(SCSYNTH "Build plugins for scsynth" ON)
option(NATIVE "Optimize for native architecture" OFF)
option(STRICT "Use strict warning flags" OFF)
option(NOVA_SIMD "Build plugins with nova-simd support." ON)

if (NOVA_SIMD)
    add_definitions(-DNOVA_SIMD)
    include_directories(${SC_PATH}/external_libraries/nova-simd)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

# Windows compatibility (unistd.h)
if(WIN32)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
    set(BUNGEE_COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/windows_compat)
    file(MAKE_DIRECTORY ${BUNGEE_COMPAT_DIR})
    file(WRITE ${BUNGEE_COMPAT_DIR}/unistd.h 
"#ifndef _WINDOWS_UNISTD_H
#define _WINDOWS_UNISTD_H
#include <io.h>
#include <process.h>
#include <stddef.h>
#define R_OK 4
#define W_OK 2
#define F_OK 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define access _access
#define dup2 _dup2
#define getcwd _getcwd
#define unlink _unlink
#define fileno _fileno
#define isatty _isatty
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#endif")
    include_directories(SYSTEM ${BUNGEE_COMPAT_DIR})
endif()

# Assert fix 
set(ASSERT_FIX_H ${CMAKE_BINARY_DIR}/assert_fix.h)
file(WRITE ${ASSERT_FIX_H} 
"#pragma once
#include <cassert>
#ifndef assert
#define assert(x) ((void)0)
#endif
#define BUNGEE_VERSION \"1.0.0\"
#define BUNGEE_VERSION_MAJOR 1
#define BUNGEE_VERSION_MINOR 0
#define BUNGEE_VERSION_PATCH 0
")

# Patch Resample.h for MSVC (__attribute__ -> __declspec)
if(MSVC)
    set(RESAMPLE_H_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/src/Resample.h)
    file(READ ${RESAMPLE_H_PATH} RESAMPLE_H_CONTENT)
    if(NOT RESAMPLE_H_CONTENT MATCHES "BUNGEE_NOINLINE")
        message(STATUS "Patching Resample.h for MSVC compatibility...")
        string(REPLACE 
            "static __attribute__((noinline)) void run"
            "static \n#ifdef _MSC_VER\n    __declspec(noinline)\n#else\n    __attribute__((noinline))\n#endif\n void run"
            PATCHED_CONTENT "${RESAMPLE_H_CONTENT}"
        )
        file(WRITE ${RESAMPLE_H_PATH} "${PATCHED_CONTENT}")
    endif()
endif()

set(CPP_SOURCES
    plugins/BufPlayBungee/BufPlayBungee.cpp
)

# Bungee
file(GLOB_RECURSE BUNGEE_CPP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/src/*.cpp"
)
list(FILTER BUNGEE_CPP_SOURCES EXCLUDE REGEX ".*main\.cpp$")
list(FILTER BUNGEE_CPP_SOURCES EXCLUDE REGEX ".*test.*")
list(FILTER BUNGEE_CPP_SOURCES EXCLUDE REGEX ".*example.*")
list(APPEND CPP_SOURCES ${BUNGEE_CPP_SOURCES})

# PFFFT
file(GLOB PFFFT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/submodules/pffft/*.c"
)
list(FILTER PFFFT_SOURCES EXCLUDE REGEX ".*test.*")

# static library for all C files
add_library(bungee_c_objects STATIC ${PFFFT_SOURCES})
target_include_directories(bungee_c_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/submodules/pffft
    ${BUNGEE_COMPAT_DIR}
)
set_target_properties(bungee_c_objects PROPERTIES LANGUAGE C)

# version.h 
set(BUNGEE_VERSION_H ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/src/bungee/version.h)
if(NOT EXISTS ${BUNGEE_VERSION_H})
    file(WRITE ${BUNGEE_VERSION_H} 
"#pragma once
#define BUNGEE_VERSION \"1.0.0\"
#define BUNGEE_VERSION_MAJOR 1
#define BUNGEE_VERSION_MINOR 0
#define BUNGEE_VERSION_PATCH 0
")
endif()

# Eigen
set(EIGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/submodules/eigen)
if(NOT EXISTS ${EIGEN_INCLUDE_DIR})
    message(STATUS "Downloading Eigen...")
    file(DOWNLOAD 
        https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/eigen.tar.gz
        SHOW_PROGRESS
    )
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/eigen-src)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_CURRENT_BINARY_DIR}/eigen.tar.gz
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/eigen-src
    )
    file(GLOB EIGEN_EXTRACTED_DIR ${CMAKE_CURRENT_BINARY_DIR}/eigen-src/eigen-*)
    set(EIGEN_INCLUDE_DIR ${EIGEN_EXTRACTED_DIR})
endif()

sc_add_server_plugin(
    "BufPlayBungee/BufPlayBungee"
    "BufPlayBungee"
    "${CPP_SOURCES}"
    "plugins/BufPlayBungee/BufPlayBungee.sc"
    "plugins/BufPlayBungee/BufPlayBungee.schelp"
)

foreach(PLUGIN_TARGET BufPlayBungee_scsynth BufPlayBungee_supernova)
    target_include_directories(${PLUGIN_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee
        ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/bungee/submodules/pffft
        ${EIGEN_INCLUDE_DIR}
        ${BUNGEE_COMPAT_DIR}
    )

    target_link_libraries(${PLUGIN_TARGET} PRIVATE bungee_c_objects)

    # For C++ files, force include assert_fix.h (cross‑platform)
    if(MSVC)
        target_compile_options(${PLUGIN_TARGET} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/FI"${ASSERT_FIX_H}">
            /fp:fast
            /wd4244 /wd4305 /wd4018
        )
    else()
        target_compile_options(${PLUGIN_TARGET} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include ${ASSERT_FIX_H}>
            -ffast-math -fno-finite-math-only -fno-exceptions
        )
    endif()

    if(WIN32)
        target_compile_definitions(${PLUGIN_TARGET} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _SCL_SECURE_NO_WARNINGS
        )
    endif()
endforeach()