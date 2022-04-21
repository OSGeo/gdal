# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying file COPYING-CMAKE-SCRIPTS or
# https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindBRUNSLI
--------

Find the BRUNSLI libraries

Brunsli encode and decode libraries are built with CMake. 
Unfortunately Brunsli does not export cmake config files yet, thus this find module

IMPORTED targets
^^^^^^^^^^^^^^^^

This module defines the following 
:prop_tgt:`IMPORTED` target: ``BRUNSLI::ENCODE``
:prop_tgt:`IMPORTED` target: ``BRUNSLI::DECODE``

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables if found:

``BRUNSLI_INCLUDE_DIR`` - where to find encode.h, decode.h, etc.
``BRUNSLI_ENC_LIB`` - the library to link against to encode Brunsli.
``BRUNSLI_DEC_LIB`` - the library to link against to decode Brunsli.
``BRUNSLI_FOUND`` - TRUE if found

#]=======================================================================]

set(BRUNSLI_NAME BRUNSLI)

find_path(BRUNSLI_INCLUDE_DIR brunsli/encode.h
    HINTS ${BRUNSLI_ROOT}
    PATH_SUFFIXES ${BRUNSLI_NAME}/include include
)

find_library(BRUNSLI_ENC_LIB 
    NAMES brunslienc-c
    HINTS ${BRUNSLI_ROOT}
    PATH_SUFFIXES ${BRUNSLI_NAME}/lib lib
)

find_library(BRUNSLI_DEC_LIB 
    NAMES brunslidec-c
    HINTS ${BRUNSLI_ROOT}
    PATH_SUFFIXES ${BRUNSLI_NAME}/lib lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BRUNSLI
    REQUIRED_VARS   BRUNSLI_ENC_LIB BRUNSLI_DEC_LIB BRUNSLI_INCLUDE_DIR
)
mark_as_advanced(BRUNSLI_INCLUDE_DIR BRUNSLI_ENC_LIB BRUNSLI_DEC_LIB)

if(BRUNSLI_FOUND)
    if(NOT TARGET BRUNSLI::ENCODE)
        add_library(BRUNSLI::ENCODE UNKNOWN IMPORTED)
        set_target_properties(BRUNSLI::ENCODE PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${BRUNSLI_INCLUDE_DIR}
            IMPORTED_LINK_INTERFACE_LANGUAGES C
            IMPORTED_LOCATION ${BRUNSLI_ENC_LIB}
        )
    endif()
    if(NOT TARGET BRUNSLI::DECODE)
        add_library(BRUNSLI::DECODE UNKNOWN IMPORTED)
        set_target_properties(BRUNSLI::DECODE PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${BRUNSLI_INCLUDE_DIR}
            IMPORTED_LINK_INTERFACE_LANGUAGES C
            IMPORTED_LOCATION ${BRUNSLI_DEC_LIB}
        )
    endif()
endif()
