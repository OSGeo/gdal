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

if(OpenEXR_ROOT)
  list(APPEND OpenEXR_INC_HINTS ${OpenEXR_ROOT}/include/OpenEXR)
  list(APPEND OpenEXR_LIB_HINTS ${OpenEXR_ROOT}/lib)
  list(APPEND CMAKE_PREFIX_PATH ${OpenEXR_ROOT})
endif()

if(Imath_ROOT)
  list(APPEND Imath_INC_HINTS ${Imath_ROOT}/include/Imath)
  list(APPEND Imath_LIB_HINTS ${Imath_ROOT}/lib)
  list(APPEND CMAKE_PREFIX_PATH ${Imath_ROOT})
endif()

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OpenEXR QUIET OpenEXR)
  if(PC_OpenEXR_FOUND)
    list(APPEND OpenEXR_LIB_HINTS ${PC_OpenEXR_LIBRARY_DIRS})
    list(APPEND OpenEXR_INC_HINTS ${PC_OpenEXR_INCLUDE_DIRS})
    set(OpenEXR_VERSION_STRING ${PC_OpenEXR_VERSION})
  endif()
endif ()

find_path(OpenEXR_INCLUDE_DIR
          NAMES ImfVersion.h
          HINTS ${OpenEXR_INC_HINTS}
          PATH_SUFFIXES OpenEXR)
find_path(Imath_INCLUDE_DIR
          NAMES ImathMatrix.h
          HINTS ${Imath_INC_HINTS} ${OpenEXR_INCLUDE_DIR}
          PATH_SUFFIXES Imath)

if(OpenEXR_INCLUDE_DIR AND NOT OpenEXR_VERSION_STRING)
  # Fallback for PkgConfig not finding anything
  file(READ ${OpenEXR_INCLUDE_DIR}/OpenEXRConfig.h txt)
  string(REGEX MATCH "define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"([0-9]+(.[0-9]+)?(.[0-9]+)?)\".*$" _ ${txt})
  set(OpenEXR_VERSION_STRING ${CMAKE_MATCH_1})
endif()

if (OpenEXR_VERSION_STRING VERSION_GREATER_EQUAL 3.0)
    find_library(OpenEXR_LIBRARY
                 NAMES OpenEXR
                 HINTS ${OpenEXR_LIB_HINTS})
    find_library(OpenEXR_UTIL_LIBRARY
                 NAMES OpenEXRUtil
                 HINTS ${OpenEXR_LIB_HINTS})
    find_library(OpenEXR_HALF_LIBRARY
                 NAMES Imath
                 HINTS ${Imath_LIB_HINTS})  #Imath is considered separate since v3
    find_library(OpenEXR_IEX_LIBRARY
                 NAMES Iex
                 HINTS ${OpenEXR_LIB_HINTS})
else()
    find_library(OpenEXR_LIBRARY
                 NAMES IlmImf
                 HINTS ${OpenEXR_LIB_HINTS})
    find_library(OpenEXR_UTIL_LIBRARY
                 NAMES IlmImfUtil
                 HINTS ${OpenEXR_LIB_HINTS})
    find_library(OpenEXR_HALF_LIBRARY
                 NAMES Half
                 HINTS ${OpenEXR_LIB_HINTS})
    find_library(OpenEXR_IEX_LIBRARY
                 NAMES Iex
                 HINTS ${OpenEXR_LIB_HINTS})
endif()

mark_as_advanced(OpenEXR_LIBRARY
                 OpenEXR_UTIL_LIBRARY
                 OpenEXR_HALF_LIBRARY
                 OpenEXR_IEX_LIBRARY
                 OpenEXR_INCLUDE_DIR
                 Imath_INCLUDE_DIR)

find_package_handle_standard_args(OpenEXR FOUND_VAR OpenEXR_FOUND
                                  REQUIRED_VARS OpenEXR_LIBRARY
                                                OpenEXR_UTIL_LIBRARY
                                                OpenEXR_HALF_LIBRARY
                                                OpenEXR_IEX_LIBRARY
                                                OpenEXR_INCLUDE_DIR
                                                Imath_INCLUDE_DIR
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
