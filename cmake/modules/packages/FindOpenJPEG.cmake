###
# File:  FindOpenJPEG.cmake
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


# - Find OpenJPEG
# Find the OpenJPEG includes and library
#
# IMPORTED Target
#      OPENJPEG::OpenJPEG
#
# This module defines
#  OPENJPEG_INCLUDE_DIR, where to find openjpeg.h, etc.
#  OPENJPEG_LIBRARIES, the libraries needed to use OpenJPEG.
#  OPENJPEG_FOUND, If false, do not try to use OpenJPEG.
# also defined, but not for general use are
#  OPENJPEG_LIBRARY, where to find the OpenJPEG library.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OPENJPEG QUIET libopenjp2)
    set(OPENJPEG_VERSION_STRING ${PC_OPENJPEG_VERSION})
endif()


find_path(OPENJPEG_INCLUDE_DIR opj_config.h
          PATH_SUFFIXES
              openjpeg-2.5
              openjpeg-2.4
              openjpeg-2.3
              openjpeg-2.2
              openjpeg-2.1
              openjpeg-2.0
          HINTS ${PC_OPENJPEG_INCLUDE_DIRS}
          DOC "Location of OpenJPEG Headers"
)

find_library(OPENJPEG_LIBRARY
             NAMES openjp2
             HINTS ${PC_OPENJPEG_LIBRARY_DIRS}
             )
mark_as_advanced(OPENJPEG_LIBRARY OPENJPEG_INCLUDE_DIR)

if(OPENJPEG_INCLUDE_DIR)
    if(OPENJPEG_VERSION_STRING)
        string(REGEX MATCH "([0-9]+).([0-9]+).([0-9]+)" OPJ_VERSION ${OPENJPEG_VERSION_STRING})
        if(OPJ_VERSION)
            transform_version(OPENJPEG_VERSION_NUM ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})
        else()
            message(FATAL "OpenJPEG version not found")
        endif()
    else()
        if(OPENJPEG_INCLUDE_DIR MATCHES "openjpeg-2.0")
            if(EXISTS "${OPENJPEG_INCLUDE_DIR}/opj_config.h")
                file(READ "${OPENJPEG_INCLUDE_DIR}/opj_config.h" VERSION_H_CONTENTS)
                string(REGEX MATCH "OPJ_PACKAGE_VERSION[ \t]+\"([0-9]+).([0-9]+).([0-9]+)\""
                       OPJ_VERSION ${VERSION_H_CONTENTS})
                string(REGEX MATCH "([0-9]+).([0-9]+).([0-9]+)"
                       OPJ_VERSION ${OPJ_VERSION})
                if(OPJ_VERSION)
                    transform_version(OPENJPEG_VERSION_NUM ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})
                else()
                    message(FATAL "OpenJPEG 2.0 header version not found")
                endif()
            endif()
        else()
            set(MAJOR_VERSION 0)
            set(MINOR_VERSION 0)
            set(REV_VERSION 0)
            if(EXISTS "${OPENJPEG_INCLUDE_DIR}/opj_config.h")
                file(READ "${OPENJPEG_INCLUDE_DIR}/opj_config.h" VERSION_H_CONTENTS)
                string(REGEX MATCH "OPJ_VERSION_MAJOR[ \t]+([0-9]+)"
                  MAJOR_VERSION ${VERSION_H_CONTENTS})
                string (REGEX MATCH "([0-9]+)" MAJOR_VERSION ${MAJOR_VERSION})
                string(REGEX MATCH "OPJ_VERSION_MINOR[ \t]+([0-9]+)"
                  MINOR_VERSION ${VERSION_H_CONTENTS})
                string (REGEX MATCH "([0-9]+)"
                  MINOR_VERSION ${MINOR_VERSION})
                string(REGEX MATCH "OPJ_VERSION_BUILD[ \t]+([0-9]+)"
                  REV_VERSION ${VERSION_H_CONTENTS})
                string (REGEX MATCH "([0-9]+)"
                  REV_VERSION ${REV_VERSION})
                unset(VERSION_H_CONTENTS)
            endif()
            set(OPENJPEG_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}.${REV_VERSION}")
            TRANSFORM_VERSION(OPENJPEG_VERSION_NUM ${MAJOR_VERSION} ${MINOR_VERSION} ${REV_VERSION})
            unset(MAJOR_VERSION)
            unset(MINOR_VERSION)
            unset(REV_VERSION)
        endif()
   endif()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenJPEG
                                  FOUND_VAR OPENJPEG_FOUND
                                  REQUIRED_VARS OPENJPEG_LIBRARY OPENJPEG_INCLUDE_DIR
                                  VERSION_VAR OPENJPEG_VERSION_STRING)
if(OPENJPEG_FOUND)
  set(OPENJPEG_LIBRARIES ${OPENJPEG_LIBRARY})
  set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR})

  if(NOT TARGET OPENJPEG::OpenJPEG)
    add_library(OPENJPEG::OpenJPEG UNKNOWN IMPORTED)
    set_target_properties(OPENJPEG::OpenJPEG PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${OPENJPEG_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION "${OPENJPEG_LIBRARY}")
  endif()
endif()
