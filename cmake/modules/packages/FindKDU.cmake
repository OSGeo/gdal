# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindKDU
# -----------
#
# CMake module to search for KAKADU library
#
# Copyright (C) 2017-2018, Hiroshi Miura
#
# If it's found it sets KDU_FOUND to TRUE
# and following variables are set:
#    KDU_INCLUDE_DIR
#    KDU_LIBRARY
#

if(CMAKE_VERSION VERSION_LESS 3.13)
    set(KDU_ROOT "KDU-ROOT-NOT-FOUND" CACHE STRING "KAKADU library base directory")
endif()

# NOTE:
# $(KAKDIR)/coresys/common $(KAKDIR)/apps/compressed_io
# $(KAKDIR)/apps/jp2 $(KAKDIR)/apps/image $(KAKDIR)/apps/args
# $(KAKDIR)/apps/support $(KAKDIR)/apps/kdu_compress
if(KDU_ROOT)
    find_path(KDU_INCLUDE_DIR kdu_file_io.h
              PATH_SUFFIX coresys/common
              PATH ${KDU_DIRECTORY})
    find_library(KDU_LIBRARY kdu
                 PATH ${KDU_DIRECTORY})
else()
    find_path(KDU_INCLUDE_DIR kdu_file_io.h
              PATH_SUFFIX coresys/common)
    find_library(KDU_LIBRARY kdu)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KDU
                                  VERSION_VAR KDU_VERSION_STRING
                                  REQUIRED_VARS KDU_INCLUDE_DIR KDU_LIBRARY)
mark_as_advanced(KDU_INCLUDE_DIR KDU_LIBRARY)

if(KDU_FOUND)
    set(KDU_INCLUDE_DIRS ${KDU_INCLUDE_DIR})
    set(KDU_LIBRARIES ${KDU_LIBRARY})
endif()