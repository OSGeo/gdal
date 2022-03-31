###############################################################################
# CMake module to search for PROJ.4 library
#
# On success, the macro sets the following variables:
# PROJ_FOUND       = if the library found
# PROJ_LIBRARY     = full path to the library
# PROJ_INCLUDE_DIR = where to find the library headers 
# also defined, but not for general use are
# PROJ_LIBRARY, where to find the PROJ.4 library.
#
# Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2015 NextGIS <info@nextgis.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

# Try to use OSGeo4W installation
IF(WIN32)
    SET(PROJ_OSGEO4W_HOME "C:/OSGeo4W") 

    IF($ENV{OSGEO4W_HOME})
        SET(PROJ_OSGEO4W_HOME "$ENV{OSGEO4W_HOME}") 
    ENDIF()
ENDIF(WIN32)

FIND_PATH(PROJ_INCLUDE_DIR proj_api.h
    PATHS ${PROJ_OSGEO4W_HOME}/include
    DOC "Path to PROJ library include directory")

SET(PROJ_NAMES ${PROJ_NAMES} proj proj_i)
FIND_LIBRARY(PROJ_LIBRARY
    NAMES ${PROJ_NAMES}
    PATHS ${PROJ_OSGEO4W_HOME}/lib
    DOC "Path to PROJ library file")

if(PROJ_INCLUDE_DIR)
    set(PROJ_VERSION_MAJOR 0)
    set(PROJ_VERSION_MINOR 0)
    set(PROJ_VERSION_PATCH 0)
    set(PROJ_VERSION_NAME "EARLY RELEASE")

    if(EXISTS "${PROJ_INCLUDE_DIR}/proj_api.h")
        file(READ "${PROJ_INCLUDE_DIR}/proj_api.h" PROJ_API_H_CONTENTS)
        string(REGEX MATCH "PJ_VERSION[ \t]+([0-9]+)"
          PJ_VERSION ${PROJ_API_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          PJ_VERSION ${PJ_VERSION})

        string(SUBSTRING ${PJ_VERSION} 0 1 PROJ_VERSION_MAJOR)
        string(SUBSTRING ${PJ_VERSION} 1 1 PROJ_VERSION_MINOR)
        string(SUBSTRING ${PJ_VERSION} 2 1 PROJ_VERSION_PATCH)
        unset(PROJ_API_H_CONTENTS)
    endif()
      
    set(PROJ_VERSION_STRING "${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_PATCH}")   
endif ()    
         
# Handle the QUIETLY and REQUIRED arguments and set SPATIALINDEX_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PROJ 
                                  REQUIRED_VARS PROJ_LIBRARY PROJ_INCLUDE_DIR 
                                  VERSION_VAR PROJ_VERSION_STRING)

IF(PROJ_FOUND)
  set(PROJ_LIBRARIES ${PROJ_LIBRARY})
  set(PROJ_INCLUDE_DIRS ${PROJ_INCLUDE_DIR})
ENDIF()

# Hide internal variables
mark_as_advanced(
  PROJ_INCLUDE_DIR
  PROJ_LIBRARY)

#======================
