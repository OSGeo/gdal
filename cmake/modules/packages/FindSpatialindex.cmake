# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# - Find Spatialindex
#
# Once run this will define:
#
# SPATIALINDEX_FOUND       = system has Spatialindex lib
# SPATIALINDEX_LIBRARY     = full path to the Spatialindex library
# SPATIALINDEX_INCLUDE_DIR = where to find headers
#

find_path(SPATIALINDEX_INCLUDE_DIR NAMES SpatialIndex.h PATH_SUFFIXES spatialindex)
find_library(SPATIALINDEX_LIBRARY NAMES spatialindex_i spatialindex)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Spatialindex FOUND_VAR SPATIALINDEX_FOUND
                                  REQUIRED_VARS SPATIALINDEX_LIBRARY MYSQL_SPATIALINDEX_DIR)
