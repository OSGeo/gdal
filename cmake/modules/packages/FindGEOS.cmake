# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGEOS
# -----------
#
# CMake module to search for GEOS library
#
# Copyright (C) 2017-2018, Hiroshi Miura
# Copyright (c) 2008, Mateusz Loskot <mateusz@loskot.net>
# (based on FindGDAL.cmake by Magnus Homann)
#
# If it's found it sets GEOS_FOUND to TRUE
# and following variables are set:
#    GEOS_INCLUDE_DIR
#    GEOS_LIBRARY
#

find_program(GEOS_CONFIG geos-config)
if(GEOS_CONFIG)
    exec_program(${GEOS_CONFIG} ARGS --version OUTPUT_VARIABLE GEOS_VERSION)
    exec_program(${GEOS_CONFIG} ARGS --prefix OUTPUT_VARIABLE GEOS_PREFIX)
endif()

find_path(GEOS_INCLUDE_DIR NAMES geos_c.h
          HINTS ${GEOS_PREFIX}/include)
find_library(GEOS_LIBRARY NAMES geos_c
             HINTS ${GEOS_PREFIX}/lib)
mark_as_advanced(GEOS_INCLUDE_DIR GEOS_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GEOS FOUND_VAR GEOS_FOUND
                                  REQUIRED_VARS GEOS_LIBRARY GEOS_INCLUDE_DIR)

if(GEOS_FOUND)
    set(GEOS_LIBRARIES ${GEOS_LIBRARY})
    set(GEOS_INCLUDE_DIRS ${GEOS_INCLUDE_DIR})
    set(GEOS_TARGET GEOS::GEOS)

    if(NOT TARGET ${GEOS_TARGET})
        add_library(${GEOS_TARGET} UNKNOWN IMPORTED)
        set_target_properties(${GEOS_TARGET} PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${GEOS_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${GEOS_LIBRARY}")
    endif()
endif()
