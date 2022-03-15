#
# CMake module to support ccache (or clcache for MSVC)
#
# Copyright (c) 2021, Mike Taves <mwtoews at gmail dot com>
#
# Usage:
# Add "include(Ccache)" to CMakeLists.txt and enable
# using the option -D USE_CCACHE=ON

cmake_minimum_required(VERSION 3.4)


option(USE_CCACHE
  "Use ccache (or clcache for MSVC) to compile C/C++ objects" OFF)
if(NOT USE_CCACHE)
  # stop here and return to including file
  return()
endif()

# Search priority:
# 1. ccache for many compilers except MSVC
# 2. clcache for MSVC

find_program(CCACHE_PROGRAM NAMES ccache clcache)

if(CCACHE_PROGRAM)
  message(STATUS "Configuring ccache with ${CCACHE_PROGRAM}")

  if(CMAKE_GENERATOR STREQUAL "Xcode")
    # see https://crascit.com/2016/04/09/using-ccache-with-cmake/
    set(C_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CXX_LAUNCHER "${CCACHE_PROGRAM}")
    set(CCACHE_LAUNCH_C ${CMAKE_BINARY_DIR}/ccache-c)
    set(CCACHE_LAUNCH_CXX ${CMAKE_BINARY_DIR}/ccache-cxx)
    file(WRITE "${CCACHE_LAUNCH_C}" "\
#!/bin/sh
shift
exec \"${C_LAUNCHER}\" \"${CMAKE_C_COMPILER}\" \"$@\"
")
    file(WRITE "${CCACHE_LAUNCH_CXX}" "\
#!/bin/sh
shift
exec \"${CXX_LAUNCHER}\" \"${CMAKE_CXX_COMPILER}\" \"$@\"
")
    # Note: file(CHMOD) introduced in CMake 3.19
    execute_process(
      COMMAND chmod a+rx
        "${CCACHE_LAUNCH_C}"
        "${CCACHE_LAUNCH_CXX}"
    )
    # Set Xcode project attributes to route compilation and linking
    # through the wrapper scripts
    set(CMAKE_XCODE_ATTRIBUTE_CC         "${CCACHE_LAUNCH_C}")
    set(CMAKE_XCODE_ATTRIBUTE_CXX        "${CCACHE_LAUNCH_CXX}")
    set(CMAKE_XCODE_ATTRIBUTE_LD         "${CCACHE_LAUNCH_C}")
    set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CCACHE_LAUNCH_CXX}")
  elseif(CCACHE_PROGRAM MATCHES "clcache")
    set(CMAKE_C_COMPILER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER "${CCACHE_PROGRAM}")
  else()
    # Most other generators (Unix Makefiles, Ninja, etc.)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  endif()
else()
  message(WARNING "Ccache was requested, but no program was not found")
endif()
