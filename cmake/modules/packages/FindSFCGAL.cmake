# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# FindSFCGAL
# ~~~~~~~~~
# Copyright (C) 2017-2018, Hiroshi Miura
#

find_program(SFCGAL_CONFIG sfcgal-config DOC "SFCGAL config command")
if(SFCGAL_CONFIG)
    execute_process(COMMAND ${SFCGAL_CONFIG} --prefix
                    OUTPUT_VARIABLE SC_SFCGAL_PREFIX
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SFCGAL_CONFIG} --version
                    OUTPUT_VARIABLE SC_SFCGAL_VERSION_STRING
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SFCGAL_CONFIG} --ldflags
                    OUTPUT_VARIABLE SC_SFCGAL_LDFLAGS
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "^-L(.*)$" "\\1" SC_SFCGAL_LIBDIR "${SC_SFCGAL_LDFLAGS}")
    if(SC_SFCGAL_VERSION_STRING)
        set(SFCGAL_VERSION_STRING ${SC_SFCGAL_VERSION_STRING})
    endif()
endif()

find_path(SFCGAL_INCLUDE_DIR SFCGAL/Kernel.h
          HINTS ${SC_SFCGAL_PREFIX}
          PATHS ${SFCGAL_ROOT}
          PATH_SUFFIXES include Include
          )

if(SFCGAL_INCLUDE_DIR)
    if(NOT SFCGAL_VERSION_STRING)
        if(EXISTS "${SFCGAL_INCLUDE_DIR}/SFCGAL/version.h")
            file(STRINGS "${SFCGAL_INCLUDE_DIR}/SFCGAL/version.h" sfcgal_version_str REGEX "^#define[\t ]+SFCGAL_VERSION[\t ]+\".*\"")
            string(REGEX REPLACE "^#define[\t ]+SFCGAL_VERSION[\t ]+\"([^\"]*)\".*" "\\1" SFCGAL_VERSION_STRING "${sfcgal_version_str}")
            if(${SFCGAL_VERSION_STRING} MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+")
                string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" SFCGAL_VERSION_MAJOR "${SFCGAL_VERSION_STRING}")
                string(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" SFCGAL_VERSION_MINOR "${SFCGAL_VERSION_STRING}")
                string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" SFCGAL_VERSION_PATCH "${SFCGAL_VERSION_STRING}")
            else()
                message(WARNING "SFCGAL_VERSION (${SFCGAL_VERSION_STRING}) doesn't match *.*.* form")
            endif()
            unset(sfcgal_version_str)
        else()
            message(WARNING "Failed toget SFCGAL version, header file not found.")
        endif()
    endif()
endif()

find_library(SFCGAL_LIBRARY_RELEASE NAMES SFCGAL
             HINTS ${SC_SFCGAL_LIBDIR}
             PATH ${SFCGAL_ROOT}
             PATH_SUFFIXES lib Lib
             )
find_library(SFCGAL_LIBRARY_DEBUG NAMES SFCGALd
             HINTS ${SC_SFCGAL_LIBDIR}
             PATH ${SFCGAL_ROOT}
             PATH_SUFFIXES lib Lib
             )
include(SelectLibraryConfigurations)
select_library_configurations(SFCGAL)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SFCGAL
                                  FOUND_VAR SFCGAL_FOUND
                                  REQUIRED_VARS SFCGAL_LIBRARY SFCGAL_INCLUDE_DIR
                                  VERSION_VAR SFCGAL_VERSION_STRING
                                  )
mark_as_advanced(SFCGAL_INCLUDE_DIR SFCGAL_LIBRARY)

if(SFCGAL_FOUND)
    set(SFCGAL_LIBRARIES ${SFCGAL_LIBRARY})
    set(SFCGAL_INCLUDE_DIRS "${SFCGAL_INCLUDE_DIR}")
    if(NOT TARGET SFCGAL::SFCGAL)
        add_library(SFCGAL::SFCGAL UNKNOWN IMPORTED)
        set_target_properties(SFCGAL::SFCGAL PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${SFCGAL_LIBRARY}"
                              INTERFACE_INCLUDE_DIRECTORIES "${SFCGAL_INCLUDE_DIR}")
    endif()
endif()
