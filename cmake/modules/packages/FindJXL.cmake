# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindJXL
# -------
#
# CMake module to search for JXL library
#
# Copyright (C) 2021, Even Rouault
#
# If it's found it sets JXL_FOUND to TRUE
# and following variables are set:
#    JXL_INCLUDE_DIRS
#    JXL_LIBRARIES
#
#   Imported target
#   JXL::JXL
#

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_JXL QUIET libjxl)
    set(JXL_VERSION_STRING ${PC_JXL_VERSION})
endif ()

find_path(JXL_INCLUDE_DIR
          NAMES jxl/decode.h
          HINTS ${PC_JXL_INCLUDE_DIRS})
find_library(JXL_LIBRARY
             NAMES jxl
             HINTS ${PC_JXL_LIBRARY_DIRS})
mark_as_advanced(JXL_LIBRARY JXL_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JXL DEFAULT_MSG JXL_LIBRARY JXL_INCLUDE_DIR)

if(JXL_FOUND)
  set(JXL_INCLUDE_DIRS ${JXL_INCLUDE_DIR})
  set(JXL_LIBRARIES ${JXL_LIBRARY})
  if(NOT TARGET JXL::JXL)
      add_library(JXL::JXL UNKNOWN IMPORTED)
      set_target_properties(JXL::JXL PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${JXL_INCLUDE_DIRS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${JXL_LIBRARY}")
  endif()
endif()
