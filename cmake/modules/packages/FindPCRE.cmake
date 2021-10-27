# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# FindPCRE
# ~~~~~~~~~
# Copyright (C) 2017-2018, Hiroshi Miura
#
# Find the native PCRE headers and libraries.

find_path(PCRE_INCLUDE_DIR NAMES pcre.h)
find_library(PCRE_LIBRARY NAMES pcre)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE
                                  FOUND_VAR PCRE_FOUND
                                  REQUIRED_VARS PCRE_LIBRARY PCRE_INCLUDE_DIR)
mark_as_advanced(PCRE_INCLUDE_DIR PCRE_LIBRARY)
if(PCRE_FOUND)
    set(PCRE_LIBRARIES ${PCRE_LIBRARY})
    set(PCRE_INCLUDE_DIRS ${PCRE_INCLUDE_DIR})
    if(NOT TARGET PCRE::PCRE)
        add_library(PCRE::PCRE UNKNOWN IMPORTED)
        set_target_properties(PCRE::PCRE PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${PCRE_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${PCRE_LIBRARY})
    endif()
endif()
