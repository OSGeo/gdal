# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLibKML
----------

Find the LibKML headers and libraries.

This module accept optional COMPONENTS to check supported sub modules::

    LIBKML_BASE_LIBRARY
    LIBKML_DOM_LIBRARY
    LIBKML_ENGINE_LIBRARY
    LIBKML_CONVENIENCE_LIBRARY
    LIBKML_XSD_LIBRARY


IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``LIBKML::LibKML``, if found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``LIBKML_FOUND``
  True if libkml found.

``LIBKML_INCLUDE_DIRS``
  where to find header files.

``LIBKML_LIBRARIES``
  List of libraries when using libkml.

``LIBKML_VERSION_STRING``
  The version of libkml found.
#]=======================================================================]

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBKML QUIET libkml)
  if(PC_LIBKML_VERSION)
  endif()
endif()

find_path(LIBKML_INCLUDE_DIR
          NAMES kml/engine.h kml/dom.h
          HINTS ${PC_LIBKML_INCLUDE_DIRS})
mark_as_advanced(LIBKML_INCLUDE_DIR)

find_library(LIBKML_BASE_LIBRARY
             NAMES kmlbase libkmlbase
             HINTS ${PC_LIBKML_LIBRARY_DIRS} )
mark_as_advanced(LIBKML_BASE_LIBRARY)

set(libkml_known_components DOM CONVENIENCE ENGINE REGIONATOR)
foreach(_comp IN LISTS libkml_known_components)
  if(${_comp} IN_LIST LibKML_FIND_COMPONENTS)
    string(TOLOWER ${_comp} _name)
    find_library(LIBKML_${_comp}_LIBRARY
                 NAMES kml${_name} libkml${_name}
                 HINTS ${PC_LIBKML_LIBRARY_DIRS} )
    mark_as_advanced(LIBKML_${_comp}_LIBRARY)
  endif()
endforeach()

if(LIBKML_INCLUDE_DIR AND NOT LIBKML_VERSION)
  file(STRINGS ${LIBKML_INCLUDE_DIR}/kml/base/version.h libkml_version_h_string
       REGEX "^#define[\t ]+LIBKML_(MAJOR|MINOR|MICRO)_VERSION[\t ]+[0-9]+")
  string(REGEX REPLACE ".*LIBKML_MAJOR_VERSION[\t ]+([0-9]+).*" "\\1" LIBKML_VERSION_MAJOR "${libkml_version_h_string}")
  string(REGEX REPLACE ".*LIBKML_MINOR_VERSION[\t ]+([0-9]+).*" "\\1" LIBKML_VERSION_MINOR "${libkml_version_h_string}")
  string(REGEX REPLACE ".*LIBKML_MICRO_VERSION[\t ]+([0-9]+).*" "\\1" LIBKML_VERSION_MICRO "${libkml_version_h_string}")
  set(LIBKML_VERSION_STRING "${LIBKML_VERSION_MAJOR}.${LIBKML_VERSION_MINOR}.${LIBKML_VERSION_MICRO}")
endif()

set(libkml_required_vars LIBKML_BASE_LIBRARY LIBKML_INCLUDE_DIR)
foreach(_comp IN LISTS LibKML_FIND_COMPONENTS)
  set(libkml_required_vars ${libkml_required_vars} "LIBKML_${_comp}_LIBRARY")
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibKML
                                  FOUND_VAR LIBKML_FOUND
                                  REQUIRED_VARS ${libkml_required_vars}
                                  VERSION_VAR LIBKML_VERSION_STRING)

if(LIBKML_FOUND)
  set(LIBKML_INCLUDE_DIRS ${LIBKML_INLCUDE_DIR})
  set(LIBKML_LIBRARIES ${LIBKML_BASE_LIBRARY})
  if(NOT TARGET LIBKML::LibKML)
    add_library(LIBKML::LibKML UNKNOWN IMPORTED)
    set_target_properties(LIBKML::LibKML PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${LIBKML_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LAGUAGES "C++"
                          IMPORTED_LOCATION ${LIBKML_BASE_LIBRARY})
  endif()
  foreach(_comp IN LISTS libkml_known_components)
    if(${_comp} IN_LIST LibKML_FIND_COMPONENTS)
      list(APPEND LIBKML_LIBRARIES "${LIBKML_${_comp}_LIBRARY}")
      if(NOT TARGET LIBKML::${_comp})
        add_library(LIBKML::${_comp} UNKNOWN IMPORTED)
        set_target_properties(LIBKML::${_comp} PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C++"
                              IMPORTED_LOCATION "${LIBKML_${_comp}_LIBRARY}")
      endif()
    endif()
  endforeach()
endif()
