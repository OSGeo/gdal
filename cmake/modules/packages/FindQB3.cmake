
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying file COPYING-CMAKE-SCRIPTS or
# https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindQB3
--------

Find the QB3 library

QB3 library
Brunsli encode and decode libraries are built with CMake and exports cmake config files

IMPORTED targets
^^^^^^^^^^^^^^^^

This module defines the following 
:prop_tgt:`IMPORTED` target: ``QB3::QB3``

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables if found:

``QB3_INCLUDE_DIR`` - where to find QB3 public headers
``QB3_LIBRARY`` - the library to link against
``QB3_FOUND`` - TRUE if found

#]=======================================================================]

find_package(libQB3 CONFIG)
find_path(QB3_INCLUDE_DIR NAMES QB3.h)
find_library(QB3_LIBRARY NAMES QB3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  QB3
  FOUND_VAR QB3_FOUND
  REQUIRED_VARS QB3_LIBRARY QB3_INCLUDE_DIR
)

set_package_properties(
  QB3 PROPERTIES
  DESCRIPTION "QB3 - Fast Integer Raster compression algorithm"
  URL "https://github.com/lucianpls/QB3"
)
mark_as_advanced(QB3_INCLUDE_DIR QB3_LIB)

if(QB3_FOUND)
  set(QB3_TARGET QB3::QB3)
  if (NOT TARGET ${QB3_TARGET})
    add_library(${QB3_TARGET} UNKNOWN IMPORTED)
    set_target_properties(${QB3_TARGET} PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${QB3_INCLUDE_DIR})
    if (EXISTS "${QB3_LIBRARY}")
      set_target_properties(${QB3_TARGET} PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES C
        IMPORTED_LOCATION ${QB3_LIBRARY})
    endif()
  endif()
endif()
