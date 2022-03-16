# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# FindJSONC
# ~~~~~~~~~
# Copyright (C) 2017-2018, Hiroshi Miura
# Copyright (c) 2012, Dmitry Baryshnikov <polimax at mail.ru>
#
# CMake module to search for jsonc library
#
# If it's found it sets JSONC_FOUND to TRUE
# and following variables are set:
#    JSONC_INCLUDE_DIR
#    JSONC_LIBRARY

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_JSONC QUIET json-c)
endif()

find_path(JSONC_INCLUDE_DIR
          NAMES json.h
          HINTS ${PC_JSONC_INCLUDE_DIRS}
                ${JSONC_ROOT}/include
          PATH_SUFFIXES json-c json)
find_library(JSONC_LIBRARY
             NAMES json-c json
             HINTS ${PC_JSONC_LIBRARY_DIRS}
                   ${JSONC_ROOT}/lib)
mark_as_advanced(JSONC_LIBRARY JSONC_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONC DEFAULT_MSG JSONC_LIBRARY JSONC_INCLUDE_DIR)

if(JSONC_FOUND)
  set(JSONC_INCLUDE_DIRS ${JSONC_INCLUDE_DIR})
  set(JSONC_LIBRARIES ${JSONC_LIBRARY})
  set(JSONC_TARGET JSONC::JSONC)
  if(NOT TARGET ${JSONC_TARGET})
      add_library(${JSONC_TARGET} UNKNOWN IMPORTED)
      set_target_properties(${JSONC_TARGET} PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${JSONC_INCLUDE_DIRS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${JSONC_LIBRARY}")
  endif()
endif()
