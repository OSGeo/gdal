# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# Find QHULL library
# ~~~~~~~~~
#
# Copyright (c) 2017,2018, Hiroshi Miura <miurahr@linux.com>
#
# Input variables:
#    QHULL_PACKAGE_NAME: name (or list of names) of the pkg-config package, typically qhull_r or qhullstatic_r
#
# If it's found it sets QHULL_FOUND to TRUE
# and following variables are set:
#    QHULL_INCLUDE_DIR
#    QHULL_LIBRARY
#

set(QHULL_PACKAGE_NAME "qhull_r;qhullstatic_r" CACHE STRING "Names of the pkg-config package, typically qhull_r or qhullstatic_r")
mark_as_advanced(QHULL_PACKAGE_NAME)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_search_module(PC_QHULL QUIET ${QHULL_PACKAGE_NAME})
endif()

find_path(QHULL_INCLUDE_DIR libqhull_r/libqhull_r.h
          HINTS ${PC_QHULL_INCLUDE_DIRS})

if(PC_QHULL_FOUND)
  set(_library_name "${PC_QHULL_LIBRARIES}")
else()
  set(_library_name qhull_r qhullstatic_r)
endif()
find_library(QHULL_LIBRARY NAMES ${_library_name}
             HINTS ${PC_QHULL_LIBRARY_DIRS})
unset(_library_name)

mark_as_advanced(QHULL_INCLUDE_DIR QHULL_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QHULL
    REQUIRED_VARS QHULL_LIBRARY QHULL_INCLUDE_DIR)
