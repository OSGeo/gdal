#
# CMake module to support ccache (or clcache for MSVC)
#
# Copyright (c) 2021, Mike Taves <mwtoews at gmail dot com>
#
# Usage:
# Add "include(Ccache)" to CMakeLists.txt and enable
# using the option -D USE_CCACHE=ON

cmake_minimum_required(VERSION 3.4...3.23)


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
    set(CCACHE_INVOCATION_COMMAND "${CCACHE_PROGRAM}")
    if (USE_PRECOMPILED_HEADERS)
        execute_process(COMMAND ccache --help OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CCACHE_HELP)
        execute_process(COMMAND ccache --get-config sloppiness OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CCACHE_SLOPPINESS)
        string(FIND "${CCACHE_SLOPPINESS}" "pch_defines" fpch_defines_found_index)
        string(FIND "${CCACHE_SLOPPINESS}" "time_macros" time_macros_found_index)
        string(FIND "${CCACHE_SLOPPINESS}" "include_file_mtime" include_file_mtime_found_index)
        string(FIND "${CCACHE_SLOPPINESS}" "include_file_ctime" include_file_ctime_found_index)
        # Detect if we have ccache >= 4.8 which accepts passing configuration settings when invoking the compiler
        string(FIND "${CCACHE_HELP}" "ccache [KEY=VALUE ...] compiler" ccache_key_value_found_index)
        if (fpch_defines_found_index EQUAL -1 OR time_macros_found_index EQUAL -1 OR
            include_file_mtime_found_index EQUAL -1 OR include_file_ctime_found_index EQUAL -1)
            set(CCACHE_SLOPPINESS_REQUIRED "pch_defines,time_macros,include_file_mtime,include_file_ctime")
        else()
            set(CCACHE_SLOPPINESS_REQUIRED "")
        endif()
        if (MSVC)
           # CCache doesn't work yet with precompiled headers (cf https://github.com/ccache/ccache/issues/1383)
           # so no need to set specific ccache configuration items
        elseif (ccache_key_value_found_index EQUAL -1 )
          if (CCACHE_SLOPPINESS_REQUIRED)
              message(FATAL_ERROR "The use of precompiled headers only works if the ccache 'sloppiness' settings contains 'pch_defines' and 'time_macros'. Consider running 'ccache --set-config sloppiness=${CCACHE_SLOPPINESS_REQUIRED}' to define them")
          endif()
        else()
          if (CCACHE_SLOPPINESS_REQUIRED)
            string(APPEND CCACHE_INVOCATION_COMMAND " sloppiness=${CCACHE_SLOPPINESS_REQUIRED}")
          endif()
        endif()
    endif()
    # Most other generators (Unix Makefiles, Ninja, etc.)
    if(CMAKE_C_COMPILER_LAUNCHER STREQUAL "ccache")
        unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
        unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
    endif()
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_INVOCATION_COMMAND}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_INVOCATION_COMMAND}")
  endif()
else()
  message(WARNING "Ccache was requested, but no program was not found")
endif()
