# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# FindPCRE2
# ~~~~~~~~~
# Copyright (C) 2017-2018, Hiroshi Miura
#
# Find the native PCRE2 headers and libraries.

find_path(PCRE2_INCLUDE_DIR NAMES pcre2.h)
find_library(PCRE2_LIBRARY NAMES pcre2-8)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE2
                                  FOUND_VAR PCRE2_FOUND
                                  REQUIRED_VARS PCRE2_LIBRARY PCRE2_INCLUDE_DIR)
mark_as_advanced(PCRE2_INCLUDE_DIR PCRE2_LIBRARY)
if(PCRE2_FOUND)
    set(PCRE2_LIBRARIES ${PCRE2_LIBRARY})
    set(PCRE2_INCLUDE_DIRS ${PCRE2_INCLUDE_DIR})
    if(NOT TARGET PCRE2::PCRE2)
        add_library(PCRE2::PCRE2 UNKNOWN IMPORTED)
        set_target_properties(PCRE2::PCRE2 PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${PCRE2_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${PCRE2_LIBRARY})
    endif()
endif()
