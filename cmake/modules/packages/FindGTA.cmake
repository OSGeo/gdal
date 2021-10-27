# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGTA
# ~~~~~~~
#
# Once done this will define
#
# ::
#
#  GTA_FOUND - System has libgta
#  GTA_INCLUDE_DIR - The libgta include directory
#  GTA_LIBRARIES - The libraries needed to use libgta
#

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_GTA QUIET gta)
    set(GTA_VERSION_STRING ${PC_GTA_VERSION})
endif()

find_path(GTA_INCLUDE_DIR gta/gta.h HINTS ${PC_GTA_INCLUDE_DIRS})
find_library(GTA_LIBRARY NAMES gta libgta HINTS ${PC_GTA_LIBRARY_DIRS})
mark_as_advanced(GTA_INCLUDE_DIR GTA_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTA
                                  REQUIRED_VARS GTA_LIBRARY GTA_INCLUDE_DIR
                                  VERSION_VAR GTA_VERSION_STRING)
if(GTA_FOUND)
    set(GTA_LIBRARIES ${GTA_LIBRARY})
    set(GTA_INCLUDE_DIRS ${GTA_INCLUDE_DIR})
    if(NOT TARGET GTA::GTA)
        add_library(GTA::GTA UNKNOWN IMPORTED)
        set_target_properties(GTA::GTA PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${GTA_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${GTA_LIBRARY})
    endif()
endif()
