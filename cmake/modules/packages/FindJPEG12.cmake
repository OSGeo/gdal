# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindJPEG12
# --------
#
# Find the JPEG12 includes and library
#
# ::
#
#   JPEG12_INCLUDE_DIR, where to find jpeglib.h, etc.
#   JPEG12_LIBRARIES, the libraries needed to use JPEG.
#   JPEG12_FOUND, If false, do not try to use JPEG.
#

find_path(JPEG12_INCLUDE_DIR jpeglib.h)

set(JPEG12_NAMES ${JPEG12_NAMES} jpeg12 libjpeg12)
find_library(JPEG12_LIBRARY NAMES ${JPEG12_NAMES} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JPEG12 DEFAULT_MSG JPEG12_LIBRARY JPEG12_INCLUDE_DIR)

if(JPEG12_FOUND)
  set(JPEG12_LIBRARIES ${JPEG12_LIBRARY})
  set(JPEG12_INCLUDE_DIRS ${JPEG12_INCLUDE_DIR})
endif()

mark_as_advanced(JPEG12_LIBRARY JPEG12_INCLUDE_DIR )
