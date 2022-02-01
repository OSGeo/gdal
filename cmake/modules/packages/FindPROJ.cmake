# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPROJ
---------

CMake module to search for PROJ(PROJ.4 and PROJ) library

On success, the macro sets the following variables:
``PROJ_FOUND``
  if the library found

``PROJ_LIBRARIES``
  full path to the library

``PROJ_INCLUDE_DIRS``
  where to find the library headers

``PROJ_VERSION_STRING``
  version string of PROJ

Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
Copyright (c) 2015 NextGIS <info@nextgis.com>
Copyright (c) 2018 Hiroshi Miura

#]=======================================================================]

find_path(PROJ_INCLUDE_DIR proj.h
          PATHS ${PROJ_ROOT}/include
          DOC "Path to PROJ library include directory")

set(PROJ_NAMES ${PROJ_NAMES} proj proj_i)
set(PROJ_NAMES_DEBUG ${PROJ_NAMES_DEBUG} projd proj_d)

if(NOT PROJ_LIBRARY)
  find_library(PROJ_LIBRARY_RELEASE NAMES ${PROJ_NAMES})
  find_library(PROJ_LIBRARY_DEBUG NAMES ${PROJ_NAMES_DEBUG})
  include(SelectLibraryConfigurations)
  select_library_configurations(PROJ)
  mark_as_advanced(PROJ_LIBRARY_RELEASE PROJ_LIBRARY_DEBUG)
endif()

unset(PROJ_NAMES)
unset(PROJ_NAMES_DEBUG)

if(PROJ_INCLUDE_DIR)
    file(READ "${PROJ_INCLUDE_DIR}/proj.h" PROJ_H_CONTENTS)
    string(REGEX REPLACE "^.*PROJ_VERSION_MAJOR +([0-9]+).*$" "\\1" PROJ_VERSION_MAJOR "${PROJ_H_CONTENTS}")
    string(REGEX REPLACE "^.*PROJ_VERSION_MINOR +([0-9]+).*$" "\\1" PROJ_VERSION_MINOR "${PROJ_H_CONTENTS}")
    string(REGEX REPLACE "^.*PROJ_VERSION_PATCH +([0-9]+).*$" "\\1" PROJ_VERSION_PATCH "${PROJ_H_CONTENTS}")
    unset(PROJ_H_CONTENTS)
    set(PROJ_VERSION_STRING "${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_PATCH}")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PROJ
                                  REQUIRED_VARS PROJ_LIBRARY PROJ_INCLUDE_DIR
                                  VERSION_VAR PROJ_VERSION_STRING)
mark_as_advanced(PROJ_INCLUDE_DIR PROJ_LIBRARY)

if(PROJ_FOUND)
  set(PROJ_LIBRARIES ${PROJ_LIBRARY})
  set(PROJ_INCLUDE_DIRS ${PROJ_INCLUDE_DIR})
  if(NOT TARGET PROJ::proj)
    add_library(PROJ::proj UNKNOWN IMPORTED)
    set_target_properties(PROJ::proj PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${PROJ_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C")
    if(EXISTS "${PROJ_LIBRARY}")
      set_target_properties(PROJ::proj PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${PROJ_LIBRARY}")
    endif()
    if(EXISTS "${PROJ_LIBRARY_RELEASE}")
      set_property(TARGET PROJ::proj APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(PROJ::proj PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${PROJ_LIBRARY_RELEASE}")
    endif()
    if(EXISTS "${PROJ_LIBRARY_DEBUG}")
      set_property(TARGET PROJ::proj APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(PROJ::proj PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${PROJ_LIBRARY_DEBUG}")
    endif()
  endif()
endif()

