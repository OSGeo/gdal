###
# File:  FindGrok.cmake
#

function(transform_version _numerical_result _version_major _version_minor _version_patch)
  set(factor 100)
  if(_version_minor GREATER 99)
      set(factor 1000)
  endif()
  if(_verion_patch GREATER 99)
      set(factor 1000)
  endif()
  math(EXPR _internal_numerical_result
          "${_version_major}*${factor}*${factor} + ${_version_minor}*${factor} + ${_version_patch}"
          )
  set(${_numerical_result} ${_internal_numerical_result} PARENT_SCOPE)
endfunction()


# - Find Grok
# Find the Grok includes and library
#
# IMPORTED Target
#      GROK::Grok
#
# This module defines
#  GROK_INCLUDE_DIR, where to find openjpeg.h, etc.
#  GROK_LIBRARIES, the libraries needed to use Grok.
#  GROK_FOUND, If false, do not try to use Grok.
# also defined, but not for general use are
#  GROK_LIBRARY, where to find the Grok library.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_GROK QUIET libgrokj2k)
    set(GROK_VERSION_STRING ${PC_GROK_VERSION})
endif()


find_path(GROK_INCLUDE_DIR grk_config.h
          PATH_SUFFIXES
              grok-10.0
          HINTS ${PC_GROK_INCLUDE_DIRS}
          DOC "Location of Grok Headers"
)

find_library(GROK_LIBRARY
             NAMES grokj2k
             HINTS ${PC_GROK_LIBRARY_DIRS}
             )
mark_as_advanced(GROK_LIBRARY GROK_INCLUDE_DIR)

if(GROK_INCLUDE_DIR)
     string(REGEX MATCH "([0-9]+).([0-9]+).([0-9]+)" GRK_VERSION ${GROK_VERSION_STRING})
     if(GRK_VERSION)
         transform_version(GROK_VERSION_NUM ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})
     else()
         message(FATAL "Grok version not found")
     endif()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Grok
                                  FOUND_VAR GROK_FOUND
                                  REQUIRED_VARS GROK_LIBRARY GROK_INCLUDE_DIR
                                  VERSION_VAR GROK_VERSION_STRING)
if(GROK_FOUND)
  set(GROK_LIBRARIES "${GROK_LIBRARY}")
  set(GROK_INCLUDE_DIRS "${GROK_INCLUDE_DIR}")

  if(NOT TARGET GROK::Grok)
    add_library(GROK::Grok UNKNOWN IMPORTED)
    set_target_properties(GROK::Grok PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${GROK_INCLUDE_DIR}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION "${GROK_LIBRARY}")
  endif()
endif()
