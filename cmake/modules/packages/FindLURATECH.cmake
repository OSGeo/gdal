# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

# - Try to find the LURATECH library
#
# Once done this will define
#
#  LURATECH_FOUND - System has Luratech
#  LURATECH_INCLUDE_DIR - The Luratech include directory
#  LURATECH_LIBRARIES - The libraries needed to use Luratech

if(CMAKE_VERSION VERSION_LESS 3.13)
    set(LURATECH_ROOT CACHE PATH "Root directory of Luratech SDK")
endif()

find_path(LURATECH_INCLUDE_DIR lwf_jp2.h HINTS ${LURATECH_ROOT}/include)

find_library(LURATECH_LIBRARY NAMES _lwf_jp2 lwf_jp2 PATH_SUFFIXES library HINTS ${LURATECH_ROOT}/library)

mark_as_advanced(LURATECH_INCLUDE_DIR LURATECH_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LURATECH
                                  FOUND_VAR LURATECH_FOUND
                                  REQUIRED_VARS LURATECH_LIBRARY LURATECH_INCLUDE_DIR
                                  VERSION_VAR LURATECH_VERSION_STRING
)

if(LURATECH_FOUND)
    set(LURATECH_LIBRARIES ${LURATECH_LIBRARY})
    set(LURATECH_INCLUDE_DIRS ${LURATECH_INCLUDE_DIR})
    if(NOT TARGET LURATECH::LURATECH)
        add_library(LURATECH::LURATECH UNKNOWN IMPORTED)
        set_target_properties(LURATECH::LURATECH PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${LURATECH_INCLUDE_DIRS}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${LURATECH_LIBRARIES})
    endif()
endif()
