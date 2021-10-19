# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# Find Rasterlite2
# ~~~~~~~~~~~~~~~
#
# CMake module to search for rasterlite library
#
# Copyright (c) 2009, Sandro Furieri <a.furieri at lqt.it>
# Copyright (C) 2017,2018 Hiroshi Miura
#
#
# If it's found it sets RASTERLITE2_FOUND to TRUE
# and following variables are set:
#    RASTERLITE2_INCLUDE_DIR
#    RASTERLITE2_LIBRARIES
#    RASTERLITE2_VERSION_STRING

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_RASTERLITE2 QUIET rasterlite2)
  set(RASTERLITE2_VERSION_STRING ${PC_RASTERLITE2_VERSION})
endif()

find_path(RASTERLITE2_INCLUDE_DIR
          NAMES rasterlite2.h
          SUFFIX_PATHS rasterlite2
          HINTS ${PC_RASTERLITE2_INCLUDEDIR})
find_library(RASTERLITE2_LIBRARY
             NAMES rasterlite2
             HINTS ${PC_RASTERLITE2_LIBDIR})
mark_as_advanced(RASTERLITE2_LIBRARY RASTERLITE2_INCLUDE_DIR)

if(NOT RASTERLITE2_VERSION_STRING AND RASTERLITE2_INCLUDE_DIR AND RASTERLITE2_LIBRARY)
  file(WRITE "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c"
          "#include <stdio.h>\n#include \"rasterlite2.h\"\n
          int main(int argc, void *argv) { const char *version = rl2_version(); printf(\"%s\", version); }\n")
  set(CMAKE_REQUIRED_INCLUDES "${RASTERLITE2_INCLUDE_DIR}")
  try_run(RL2_EXITCODE RL2_COMPILED ${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c
      LINK_LIBRARIES  ${RASTERLITE2_LIBRARY}
      CMAKE_FLAGS "-DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}" "-DINCLUDE_DIRECTORIES:STRING=${RASTERLITE2_INCLUDE_DIR}"
      COMPILE_OUTPUT_VARIABLE RL2_OUTPUT)
  if(RL2_COMPILED AND RL2_EXITCODE)
    set(RASTERLITE2_VERSION_STRING "${RL2_OUTPUT}")
  endif()
  unset(RL2_EXITCODE)
  unset(RL2_COMPILED)
  unset(RL2_OUTPUT)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Rasterlite2
                                  FOUND_VAR RASTERLITE2_FOUND
                                  REQUIRED_VARS RASTERLITE2_LIBRARY RASTERLITE2_INCLUDE_DIR
                                  VERSION_VAR RASTERLITE2_VERSION_STRING)
if(RASTERLITE2_FOUND)
    set(RASTERLITE2_LIBRARIES ${RASTERLITE2_LIBRARY})
    set(RASTERLITE2_INCLUDE_DIRS ${RASTERLITE2_INCLUDE_DIR})
    if(NOT TARGET RASTERLITE2::RASTERLITE2)
        add_library(RASTERLITE2::RASTERLITE2 UNKNOWN IMPORTED)
        set_target_properties(RASTERLITE2::RASTERLITE2 PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${RASTERLITE2_INCLUDE_DIRS}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${RASTERLITE2_LIBRARY})
    endif()
endif()
