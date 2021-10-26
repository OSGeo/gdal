#------------------------------------------------------------------------------#
# Distributed under the OSI-approved Apache License, Version 2.0.  See
# accompanying file Copyright.txt for details.
#------------------------------------------------------------------------------#
#
# FindBLOSC
# -----------
#
# Try to find the BLOSC library
#
# This module defines the following variables:
#
#   BLOSC_FOUND        - System has BLOSC
#   BLOSC_INCLUDE_DIRS - The BLOSC include directory
#   BLOSC_LIBRARIES    - Link these to use BLOSC
#   BLOSC_VERSION      - Version of the BLOSC library to support
#
# and the following imported targets:
#   Blosc::Blosc - The core BLOSC library
#
# You can also set the following variable to help guide the search:
#   BLOSC_ROOT - The install prefix for BLOSC containing the
#                     include and lib folders
#                     Note: this can be set as a CMake variable or an
#                           environment variable.  If specified as a CMake
#                           variable, it will override any setting specified
#                           as an environment variable.

if(NOT BLOSC_FOUND)
  if((NOT BLOSC_ROOT) AND (NOT (ENV{BLOSC_ROOT} STREQUAL "")))
    set(BLOSC_ROOT "$ENV{BLOSC_ROOT}")
  endif()
  if(BLOSC_ROOT)
    set(BLOSC_INCLUDE_OPTS HINTS ${BLOSC_ROOT}/include NO_DEFAULT_PATHS)
    set(BLOSC_LIBRARY_OPTS
      HINTS ${BLOSC_ROOT}/lib ${BLOSC_ROOT}/lib64
      NO_DEFAULT_PATHS
    )
  endif()

  find_path(BLOSC_INCLUDE_DIR blosc.h ${BLOSC_INCLUDE_OPTS})
  find_library(BLOSC_LIBRARY blosc ${BLOSC_LIBRARY_OPTS})
  if(BLOSC_INCLUDE_DIR)
    file(STRINGS ${BLOSC_INCLUDE_DIR}/blosc.h _ver_strings
      REGEX "BLOSC_VERSION_[^ ]* [0-9]+"
    )
    foreach(v IN LISTS _ver_strings)
      string(REGEX MATCH "BLOSC_VERSION_([^ ]+) ([0-9]+)" v "${v}")
      set(BLOSC_VERSION_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
    endforeach()
    set(BLOSC_VERSION
      ${BLOSC_VERSION_MAJOR}.${BLOSC_VERSION_MINOR}.${BLOSC_VERSION_PATCH}
    )
  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Blosc
    FOUND_VAR BLOSC_FOUND
    VERSION_VAR BLOSC_VERSION
    REQUIRED_VARS BLOSC_LIBRARY BLOSC_INCLUDE_DIR
  )
  if(BLOSC_FOUND)
    set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIR})
    set(BLOSC_LIBRARIES ${BLOSC_LIBRARY})
    if(BLOSC_FOUND AND NOT TARGET Blosc::Blosc)
      add_library(Blosc::Blosc UNKNOWN IMPORTED)
      set_target_properties(Blosc::Blosc PROPERTIES
        IMPORTED_LOCATION             "${BLOSC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BLOSC_INCLUDE_DIR}"
      )
    endif()
  endif()
endif()
