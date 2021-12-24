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
#    QHULL_LIBRARY
#

find_path(QHULL_INCLUDE_DIR libqhull_r/libqhull_r.h)
find_library(QHULL_LIBRARY NAMES qhull_r)
mark_as_advanced(QHULL_INCLUDE_DIR QHULL_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QHULL
    REQUIRED_VARS QHULL_LIBRARY QHULL_INCLUDE_DIR)
