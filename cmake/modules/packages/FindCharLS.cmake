# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindCharLS
----------

FindCharLS - JPEG Loss-Less Open SOurce Library CharLS

IMPORTED Targets
^^^^^^^^^^^^^^^^

``CharLS::charls``
  This module defines :prop_tgt:`IMPORTED` target ``CharLS::charls``, if found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

   ``CharLS_FOUND``
     If false, do not try to use CharLS.
   ``CharLS_INCLUDE_DIRS``
     where to find charls.h, etc.
   ``CharLS_LIBRARIES``
     the libraries needed to use CharLS.
   ``CharLS_VERSION``
     1 if CharLS/interface.h exist ,and 2 if CharLS/charls.h exist,
     and 2.1 if charls/charls.h exist when CharLS 2.1 and later.

#]=======================================================================]

find_path(CharLS_INCLUDE_DIR NAMES charls/charls.h)
find_path(CharLS_INCLUDE_DIR NAMES CharLS/charls.h)
find_path(CharLS_INCLUDE_DIR NAMES CharLS/interface.h)

if(CharLS_INCLUDE_DIR)
    if(EXISTS "${CharLS_INCLUDE_DIR}/charls/charls.h")
        set(CharLS_VERSION 2.1)
    elseif(EXISTS "${CharLS_INCLUDE_DIR}/CharLS/interface.h")
        set(CharLS_VERSION 1)
    else()
        set(CharLS_VERSION 2)
    endif()
endif()

find_library(CharLS_LIBRARY NAMES CharLS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CharLS
                                  FOUND_VAR CharLS_FOUND
                                  REQUIRED_VARS CharLS_LIBRARY CharLS_INCLUDE_DIR
                                  VERSION_VAR CharLS_VERSION)
mark_as_advanced(CharLS_LIBRARY CharLS_INCLUDE_DIR CharLS_VERSION)

include(FeatureSummary)
set_package_properties(CharLS PROPERTIES
                       DESCRIPTION "C++ JPEG Loss-Less Open Source Library Implementation."
                       URL "https://github.com/team-charls/charls"
)

if(CharLS_FOUND)
    set(CharLS_LIBRARIES ${CharLS_LIBRARY})
    set(CharLS_INCLUDE_DIRS ${CharLS_INCLUDE_DIR})
    if(NOT TARGET CharLS::charls)
        add_library(CharLS::charls UNKNOWN IMPORTED)
        if(CharLS_INCLUDE_DIRS)
          set_target_properties(CharLS::charls PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${CharLS_INCLUDE_DIR})
        endif()
        if(EXISTS "${CharLS_LIBRARY}")
          set_target_properties(CharLS::charls PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION ${CharLS_LIBRARY})
        endif()
   endif()
endif()
