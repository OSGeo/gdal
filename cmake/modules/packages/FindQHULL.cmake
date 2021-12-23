# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# Find QHULL library
# ~~~~~~~~~
#
# Copyright (c) 2017,2018, Hiroshi Miura <miurahr@linux.com>
#
# If it's found it sets QHULL_FOUND to TRUE
# and following variables are set:
#    QHULL_INCLUDE_DIR
#    QHULL_INCLUDE_SUBDIR (libqhull_r/qhull/libqhull)
#    QHULL_LIBRARY
#

find_path(QHULL_INCLUDE_DIR libqhull_r/libqhull_r.h qhull/libqhull.h libqhull/libqhull.h)
if(QHULL_INCLUDE_DIR)
  if(EXISTS ${QHULL_INCLUDE_DIR}/libqhull_r/libqhull_r.h)
    set(QHULL_INCLUDE_SUBDIR "libqhull_r")
  elseif(EXISTS ${QHULL_INCLUDE_DIR}/qhull/libqhull.h)
    set(QHULL_INCLUDE_SUBDIR "qhull")
  elseif(EXISTS ${QHULL_INCLUDE_DIR}/libqhull/libqhull.h)
    set(QHULL_INCLUDE_SUBDIR "libqhull")
  else()
    message(FATAL_ERROR "Cannot guess QHULL_INCLUDE_SUBDIR from QHULL_INCLUDE_DIR=${QHULL_INCLUDE_DIR}")
  endif()
endif()

find_library(QHULL_LIBRARY NAMES qhull_r qhull libqhull)
mark_as_advanced(QHULL_INCLUDE_SUBDIR QHULL_INCLUDE_DIR QHULL_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QHULL
    REQUIRED_VARS QHULL_LIBRARY QHULL_INCLUDE_DIR)
