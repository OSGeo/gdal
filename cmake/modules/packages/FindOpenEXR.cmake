# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindOpenEXR
# -----------
#
# CMake module to search for OpenEXR library
#
# Copyright (C) 2020, Hiroshi Miura
#
# If it's found it sets OpenEXR_FOUND to TRUE
# and following variables are set:
#    OpenEXR_INCLUDE_DIRS
#    OpenEXR_LIBRARIES
#
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OpenEXR QUIET OpenEXR)
    set(OpenEXR_VERSION_STRING ${PC_OpenEXR_VERSION})
endif ()

find_path(OpenEXR_INCLUDE_DIR
          NAMES ImfVersion.h
          HINTS ${PC_OpenEXR_INCLUDE_DIRS}
          PATH_SUFFIXES OpenEXR)
find_path(Imath_INCLUDE_DIR
          NAMES ImathMatrix.h
          HINTS ${PC_OpenEXR_INCLUDE_DIRS}
          PATH_SUFFIXES Imath)

if (OpenEXR_VERSION_STRING VERSION_GREATER_EQUAL 3.0)
    find_library(OpenEXR_LIBRARY
                 NAMES OpenEXR
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_UTIL_LIBRARY
                 NAMES OpenEXRUtil
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_HALF_LIBRARY
                 NAMES Imath
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_IEX_LIBRARY
                 NAMES Iex
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
else()
    find_library(OpenEXR_LIBRARY
                 NAMES IlmImf
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_UTIL_LIBRARY
                 NAMES IlmImfUtil
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_HALF_LIBRARY
                 NAMES Half
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    find_library(OpenEXR_IEX_LIBRARY
                 NAMES Iex
                 HINTS ${PC_OpenEXR_LIBRARY_DIRS})
endif()

find_package_handle_standard_args(OpenEXR FOUND_VAR OpenEXR_FOUND
                                  REQUIRED_VARS OpenEXR_LIBRARY OpenEXR_UTIL_LIBRARY OpenEXR_HALF_LIBRARY OpenEXR_IEX_LIBRARY OpenEXR_INCLUDE_DIR Imath_INCLUDE_DIR
                                  VERSION_VAR OpenEXR_VERSION_STRING)

if (OpenEXR_FOUND)
  set(OpenEXR_INCLUDE_DIRS ${OpenEXR_INCLUDE_DIR} ${Imath_INCLUDE_DIR})
  set(OpenEXR_LIBRARIES ${OpenEXR_LIBRARY} ${OpenEXR_UTIL_LIBRARY} ${OpenEXR_HALF_LIBRARY} ${OpenEXR_IEX_LIBRARY})
  if (NOT TARGET OpenEXR::OpenEXR)
    add_library(OpenEXR::IlmImf UNKNOWN IMPORTED)
    add_library(OpenEXR::IlmImfUtil UNKNOWN IMPORTED)
    add_library(OpenEXR::Half UNKNOWN IMPORTED)
    add_library(OpenEXR::Iex UNKNOWN IMPORTED)
    set_target_properties(OpenEXR::IlmImf  PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIRS}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${OpenEXR_LIBRARY})
    set_target_properties(OpenEXR::IlmImfUtil PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIRS}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${OpenEXR_UTIL_LIBRARY})
    set_target_properties(OpenEXR::Half PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIRS}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${OpenEXR_HALF_LIBRARY})
    set_target_properties(OpenEXR::Iex PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIRS}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${OpenEXR_IEX_LIBRARY})
  endif ()
endif ()
