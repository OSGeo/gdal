# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# FindSDE - Find ESRI SDE C++ SDK library
# ~~~~~~~~~
#
# Copyright (c) 2017,2018, Hiroshi Miura <miurahr@linux.com>
#
#
# SDE_ROOT points SDE library location.
if(CMAKE_VERSION VERSION_LESS 3.13)
    set(SDE_ROOT "" CACHE PATH "ESRI C++ SDK directory")
endif()

find_path(SDE_INCLUDE_DIR
          NAMES sdetype.h
          PATHS ${SDE_ROOT}/include)
if(SDE_INCLUDE_DIR)
    find_library(SDE_LIBRARY
                 NAMES sde
                 PATHS ${SDE_ROOT}/lib)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDE
                                  REQUIRED_VARS SDE_LIBRARY SDE_INCLUDE_DIR)
mark_as_advanced(SDE_INCLUDE_DIR SDE_LIBRARY)
if(SDE_FOUND)
    if(NOT TARGET SDE::SDE)
        add_library(SDE::SDE UNKNOWN IMPORTED)
        set_target_properties(SDE::SDE PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${SDE_INCLUDE_DIR}
                              IMPORTED_INTERFACE_LINK_LANGUAGES C
                              IMPORTED_LOCATION ${SDE_LIBRARY})
    endif()
endif()
