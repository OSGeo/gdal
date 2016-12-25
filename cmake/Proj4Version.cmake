################################################################################
# Proj4Version.cmake - part of CMake configuration of Proj4 library
################################################################################
# Copyright (C) 2010 Mateusz Loskot <mateusz@loskot.net>
#
# Distributed under the Boost Software License, Version 1.0
################################################################################
# Macros in this module:
#   
#   proj_version - defines version information for PROJ library
#   (best known as PROJ4 because MAJOR version is 4 since a very long time) 
################################################################################

# Defines version information for PROJ library
#
# proj_version(MAJOR major_version MINOR minor_version PATCH patch_level)
#
#    MAJOR.MINOR version is used to set SOVERSION
#

macro(proj_version)
  parse_arguments(THIS_VERSION "MAJOR;MINOR;PATCH;"
    ""
    ${ARGN})

  # Set version components
  set(${PROJECT_INTERN_NAME}_VERSION_MAJOR ${THIS_VERSION_MAJOR})
  set(${PROJECT_INTERN_NAME}_VERSION_MINOR ${THIS_VERSION_MINOR})
  set(${PROJECT_INTERN_NAME}_VERSION_PATCH ${THIS_VERSION_PATCH})

  # Set VERSION string
  set(${PROJECT_INTERN_NAME}_VERSION
    "${${PROJECT_INTERN_NAME}_VERSION_MAJOR}.${${PROJECT_INTERN_NAME}_VERSION_MINOR}.${${PROJECT_INTERN_NAME}_VERSION_PATCH}")

  # Set ABI version string used to name binary output 
  # On Windows, ABI version is specified using binary file name suffix.
  if(WIN32)
    set(${PROJECT_INTERN_NAME}_ABI_VERSION
      "${${PROJECT_INTERN_NAME}_VERSION_MAJOR}_${${PROJECT_INTERN_NAME}_VERSION_MINOR}")
  endif()

  message(STATUS "")
  boost_report_value(${PROJECT_INTERN_NAME}_VERSION)
  if(WIN32)
    boost_report_value(${PROJECT_INTERN_NAME}_ABI_VERSION)
  endif(WIN32)
endmacro()
