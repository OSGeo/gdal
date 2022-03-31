# - Find QHull
# Find the QHull includes and client library
# This module defines
#  QHULL_INCLUDE_DIR, where to find qhull_a.h
#  QHULL_LIBRARIES, libraries needed to use PostgreSQL
#  QHULL_FOUND, if false, do not try to use PostgreSQL
#
# Copyright (c) 2017, NextGIS, <info@nextgis.com>
# Copyright (c) 2017, Dmitry Baryshnikov, <dmitry.baryshnikov@nextgis.com>
#
# This script is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this script.  If not, see <http://www.gnu.org/licenses/>.


find_path(QHULL_INCLUDE_DIR qhull_a.h
  PATHS
  /usr/include
  /usr/local/include
  PATH_SUFFIXES
  qhull
  libqhull
)

find_library(QHULL_LIBRARIES NAMES libqhull qhull
  PATHS
  /usr/lib
  /usr/local/lib
  /usr/lib64
  /usr/local/lib64
)

# Handle the QUIETLY and REQUIRED arguments and set QHULL_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QHULL DEFAULT_MSG
                                  QHULL_LIBRARIES QHULL_INCLUDE_DIR)

IF(QHULL_FOUND)
  set(QHULL_INCLUDE_DIRS ${QHULL_INCLUDE_DIR})
ENDIF()

mark_as_advanced(QHULL_INCLUDE_DIRS QHULL_LIBRARIES)
