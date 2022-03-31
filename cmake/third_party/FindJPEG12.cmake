#.rst:
# FindJPEG
# --------
#
# Find JPEG
#
# Find the native JPEG includes and library This module defines
#
# ::
#
#   JPEG12_INCLUDE_DIR, where to find jpeglib.h, etc.
#   JPEG12_LIBRARIES, the libraries needed to use JPEG.
#   JPEG12_FOUND, If false, do not try to use JPEG.
#
# also defined, but not for general use are
#
# ::
#
#   JPEG12_LIBRARY, where to find the JPEG library.

#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_path(JPEG12_INCLUDE_DIR jpeglib.h)

set(JPEG12_NAMES ${JPEG12_NAMES} jpeg12 libjpeg12)
find_library(JPEG12_LIBRARY NAMES ${JPEG12_NAMES} )

# handle the QUIETLY and REQUIRED arguments and set JPEG12_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JPEG12 DEFAULT_MSG JPEG12_LIBRARY JPEG12_INCLUDE_DIR)

if(JPEG12_FOUND)
  set(JPEG12_LIBRARIES ${JPEG12_LIBRARY})
  set(JPEG12_INCLUDE_DIRS ${JPEG12_INCLUDE_DIR})
endif()

# Deprecated declarations.
set (NATIVE_JPEG12_INCLUDE_PATH ${JPEG12_INCLUDE_DIR} )
if(JPEG12_LIBRARY)
  get_filename_component (NATIVE_JPEG12_LIB_PATH ${JPEG12_LIBRARY} PATH)
endif()

mark_as_advanced(JPEG12_LIBRARY JPEG12_INCLUDE_DIR )
