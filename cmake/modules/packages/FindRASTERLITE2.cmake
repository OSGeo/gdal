# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindRASTERLITE2
--------------

CMake module to search for RasterLite2 library

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``RASTERLITE2::RASTERLITE2``, if
RasterLite2 has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``RASTERLITE2_FOUND``
  True if RasterLite2 found.

``RASTERLITE2_INCLUDE_DIRS``
  where to find rasterlite2, etc.

``RASTERLITE2_LIBRARIES``
  List of libraries when using RasterLite2.

``RASTERLITE2_VERSION_STRING``
  The version of RasterLite2 found.
#]=======================================================================]

if(CMAKE_VERSION VERSION_LESS 3.13)
    set(RASTERLITE2_ROOT CACHE PATH "")
    mark_as_advanced(RASTERLITE2_ROOT)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_RASTERLITE2 QUIET rasterlite2)
    set(RASTERLITE2_VERSION_STRING ${PC_RASTERLITE2_VERSION})
endif()

find_path(RASTERLITE2_INCLUDE_DIR
          NAMES rasterlite2/rasterlite2.h
          HINTS ${RASTERLITE2_ROOT} ${PC_RASTERLITE2_INCLUDE_DIRS}
          PATH_SUFFIXES include)
find_library(RASTERLITE2_LIBRARY
             NAMES rasterlite2
             HINTS ${RASTERLITE2_ROOT} ${PC_RASTERLITE2_LIBRARY_DIRS}
             PATH_SUFFIXES lib)
mark_as_advanced(RASTERLITE2_LIBRARY RASTERLITE2_INCLUDE_DIR)

if(NOT RASTERLITE2_VERSION_STRING AND RASTERLITE2_INCLUDE_DIR AND RASTERLITE2_LIBRARY)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  file(WRITE "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/get_rasterlite2_version.c"
          "#include <stdio.h>\n#include \"rasterlite2/rasterlite2.h\"\n
          int main(int argc, void *argv) { const char *version = rl2_version(); printf(\"%s\", version); }\n")
  set(CMAKE_REQUIRED_INCLUDES "${RASTERLITE2_INCLUDE_DIR}")
  try_run(RL2_EXITCODE RL2_COMPILED ${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/get_rasterlite2_version.c
      LINK_LIBRARIES  ${RASTERLITE2_LIBRARY}
      CMAKE_FLAGS "-DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}" "-DINCLUDE_DIRECTORIES:STRING=${RASTERLITE2_INCLUDE_DIR}"
      RUN_OUTPUT_VARIABLE RL2_OUTPUT)
  if(RL2_COMPILED AND RL2_EXITCODE EQUAL 0)
    set(RASTERLITE2_VERSION_STRING "${RL2_OUTPUT}")
  endif()
  unset(RL2_EXITCODE)
  unset(RL2_COMPILED)
  unset(RL2_OUTPUT)
  cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RASTERLITE2
                                  FOUND_VAR RASTERLITE2_FOUND
                                  REQUIRED_VARS RASTERLITE2_LIBRARY RASTERLITE2_INCLUDE_DIR
                                  VERSION_VAR RASTERLITE2_VERSION_STRING)
if(RASTERLITE2_FOUND)
    set(RASTERLITE2_LIBRARIES ${RASTERLITE2_LIBRARY})
    set(RASTERLITE2_INCLUDE_DIRS ${RASTERLITE2_INCLUDE_DIR})
    if(NOT TARGET RASTERLITE2::RASTERLITE2)
        add_library(RASTERLITE2::RASTERLITE2 UNKNOWN IMPORTED)
        set_target_properties(RASTERLITE2::RASTERLITE2 PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${RASTERLITE2_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${RASTERLITE2_LIBRARY})
    endif()
endif()
