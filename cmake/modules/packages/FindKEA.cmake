# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindKEA
# ~~~~~~~~~
#
# Copyright (c) 2017,2018 Hiroshi Miura <miurahr@linux.com>
#
# If it's found it sets KEA_FOUND to TRUE
# and following variables are set:
#    KEA_INCLUDE_DIR
#    KEA_LIBRARY
#    KEA_VERSION
#

find_path(KEA_INCLUDE_DIR
          NAMES KEACommon.h kea-config.h
          PATH_SUFFIXES libkea
)
find_library(KEA_LIBRARY NAMES kea)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KEA FOUND_VAR KEA_FOUND
                                  REQUIRED_VARS KEA_LIBRARY KEA_INCLUDE_DIR)

if(KEA_FOUND)
    set(KEA_INCLUDE_DIRS ${KEA_INCLUDE_DIR})
    set(KEA_LIBRARIES ${KEA_LIBRARY})

    if(NOT TARGET KEA::KEA)
        add_library(KEA::KEA UNKNOWN IMPORTED)
        set_target_properties(KEA::KEA PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${KEA_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${KEA_LIBRARY}")
    endif()
endif()