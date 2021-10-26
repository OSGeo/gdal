# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindCFITSIO
# -----------
#
# Find the CFITSIO library
#
# Once done this will define
#
#  CFITSIO_FOUND - System has libgta
#  CFITSIO_INCLUDE_DIR - The libgta include directory
#  CFITSIO_LIBRARIES - The libraries needed to use libgta

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_CFITSIO QUIET cfitsio)
    set(CFITSIO_VERSION_STRING ${PC_CFITSIO_VERSION})
endif()

find_path(CFITSIO_INCLUDE_DIR fitsio.h
          HINTS ${PC_CFITSIO_INCLUDE_DIRS})
find_library(CFITSIO_LIBRARY NAMES cfitsio libcfitsio
             HINTS ${PC_CFITSIO_LIBRARY_DIRS})
mark_as_advanced(CFITSIO_INCLUDE_DIR CFITSIO_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CFITSIO
                                  REQUIRED_VARS CFITSIO_LIBRARY CFITSIO_INCLUDE_DIR
                                  VERSION_VAR CFITSIO_VERSION_STRING)

if(CFITSIO_FOUND)
    set(CFITSIO_LIBRARIES ${CFITSIO_LIBRARY})
    set(CFITSIO_INCLUDE_DIRS ${CFITSIO_INCLUDE_DIR})
    if(NOT TARGET CFITSIO::CFITSIO)
        add_library(CFITSIO::CFITSIO UNKNOWN IMPORTED)
        set_target_properties(CFITSIO::CFITSIO PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${CFITSIO_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${CFITSIO_LIBRARY})
    endif()
endif()
