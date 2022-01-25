# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPoppler
--------

Find the Poppler headers and libraries.

This is a component-based find module, which makes use of the COMPONENTS
and OPTIONAL_COMPONENTS arguments to find_package.  The following components
are available::

  Cpp  Qt5  Qt4  Glib

Copyright (c) 2017,2019, Hiroshi Miura <miurahr@linux.com>
Copyright (c) 2015, Alex Richardson <arichardson.kde@gmail.com>

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Poppler::Poppler``, if
curl has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Poppler_FOUND``
  True if Poppler found.

``Poppler_INCLUDE_DIRS``
  where to find poppler/poppler-config.h, etc.

``Poppler_LIBRARIES``
  List of libraries when using Poppler.

``Poppler_VERSION_STRING``
  The version of Poppler found.
#]=======================================================================]

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Poppler QUIET poppler)
  if(PC_Poppler_VERSION)
    set(Poppler_VERSION_STRING ${PC_Poppler_VERSION})
  endif()
endif()
find_path(Poppler_INCLUDE_DIR NAMES "poppler-config.h" "cpp/poppler-version.h" "qt5/poppler-qt5.h" "qt4/poppler-qt4.h"
          "glib/poppler.h"
          HINTS ${PC_Poppler_INCLUDE_DIRS}
          PATH_SUFFIXES poppler)

find_library(Poppler_LIBRARY NAMES poppler HINTS ${PC_Poppler_LIBRARY_DIRS})

set(Poppler_known_components Cpp Qt4 Qt5 Glib )
foreach(_comp IN LISTS Poppler_known_components)
  string(TOLOWER "${_comp}" _lc_comp)
  set(Poppler_${_comp}_pkg_config "poppler-${_lc_comp}")
  set(Poppler_${_comp}_lib "poppler-${_lc_comp}")
  set(Poppler_${_comp}_header_subdir "poppler/${_lc_comp}")
endforeach()
set(Poppler_known_components Core ${Poppler_known_components})

# poppler-config.h header is only installed with --enable-xpdf-headers
# fall back to using any header from a submodule with a path to make it work in that case too
set(Poppler_Cpp_header "poppler-version.h")
set(Poppler_Qt5_header "poppler-qt5.h")
set(Poppler_Qt4_header "poppler-qt4.h")
set(Poppler_Glib_header "poppler.h")

foreach(_comp IN LISTS Poppler_FIND_COMPONENTS)
  set(Poppler_${_comp}_FOUND FALSE)
endforeach()

foreach(_comp IN LISTS Poppler_known_components)
  list(FIND Poppler_FIND_COMPONENTS "${_comp}" _nextcomp)
  if(_nextcomp GREATER -1)
    find_path(Poppler_${_comp}_INCLUDE_DIR
              NAMES ${Poppler_${_comp}_header}
              PATH_SUFFIXES poppler
              HINTS ${PC_Poppler_${_comp}_INCLUDE_DIRS})
    find_library(Poppler_${_comp}_LIBRARY
            NAMES ${Poppler_${_comp}_lib}
            HINTS ${PC_Poppler_${_comp}_LIBRARY_DIRS})
  endif()
endforeach()

if(NOT Poppler_VERSION_STRING)
  find_file(Poppler_VERSION_HEADER
            NAMES "poppler-config.h" "cpp/poppler-version.h"
            HINTS ${Poppler_INCLUDE_DIR}
            PATH_SUFFIXES poppler
            )
  #if(Poppler_VERSION_HEADER)
  #  file(READ ${Poppler_VERSION_HEADER} _poppler_version_header_contents)
  #  string(REGEX REPLACE
  #         "^.*[ \t]+POPPLER_VERSION_MAJOR[ \t]+([0-9]+).*$"
  #         "\\1"
  #         Poppler_VERSION_MAJOR
  #         "${_poppler_version_header_contents}"
  #         )
  #  string(REGEX REPLACE
  #         "^.*[ \t]+POPPLER_VERSION_MINOR[ \t]+([0-9]+).*$"
  #         "\\1"
  #         Poppler_VERSION_MINOR
  #         "${_poppler_version_header_contents}"
  #         )
  #  string(REGEX REPLACE
  #         "^.*[ \t]+POPPLER_VERSION_MICRO[ \t]+([0-9]+).*$"
  #         "\\1"
  #         Poppler_VERSION_MICRO
  #         "${_poppler_version_header_contents}"
  #         )
  #  unset(_poppler_version_header_contents)
  #  set(Poppler_VERSION_STRING "${Poppler_VERSION_MAJOR}.${Poppler_VERSION_MINOR}.${Poppler_VERSION_MICRO}")
  #endif()
  if(Poppler_VERSION_HEADER)
    file(STRINGS "${Poppler_VERSION_HEADER}" _poppler_version_str REGEX "^#define[\t ]+POPPLER_VERSION[\t ]+\".*\"")
    string(REGEX REPLACE "^#define[\t ]+POPPLER_VERSION[\t ]+\"([^\"]*)\".*" "\\1" Poppler_VERSION_STRING "${_poppler_version_str}")
    if(NOT ${Poppler_VERSION_STRING} MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+")
      message(WARNING "POPPLER_VERSION (${Poppler_VERSION_STRING}) doesn't match *.*.* form")
    endif()
    unset(_poppler_version_str)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Poppler
                                  FOUND_VAR Poppler_FOUND
                                  REQUIRED_VARS Poppler_LIBRARY Poppler_INCLUDE_DIR
                                  VERSION_VAR  Poppler_VERSION_STRING
                                  HANDLE_COMPONENTS)
mark_as_advanced(Poppler_INCLUDE_DIR Poppler_LIBRARY)

if(Poppler_FOUND)
  set(Poppler_INCLUDE_DIRS ${Poppler_INCLUDE_DIR})
  set(Poppler_LIBRARIES ${Poppler_LIBRARY})
  if(NOT TARGET Poppler::Poppler)
    add_library(Poppler::Poppler UNKNOWN IMPORTED)
    set_target_properties(Poppler::Poppler PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${Poppler_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION "${Poppler_LIBRARY}")
    foreach(tgt IN LISTS Poppler_known_components)
      add_library(Poppler::${tgt} UNKNOWN IMPORTED)
      set_target_properties(Poppler::${tgt} PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES ${Poppler_${tgt}_INCLUDE_DIR}
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION ${POPPLER_${tgt}_LIBRARY})
    endforeach()
  endif()
endif()

include(FeatureSummary)
set_package_properties(Poppler PROPERTIES
                       DESCRIPTION "A PDF rendering library" URL "http://poppler.freedesktop.org")
