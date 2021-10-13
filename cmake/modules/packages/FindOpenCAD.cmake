# Find opencad
# ~~~~~~~~~
# Copyright (c) 2016, Dmitry Baryshnikov <polimax at mail.ru>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for opencad library
#
# If it's found it sets OPENCAD_FOUND to TRUE
# and following variables are set:
#    OPENCAD_INCLUDE_DIR
#    OPENCAD_LIBRARY

# FIND_PATH and FIND_LIBRARY normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing. 

# try to use framework on mac
# want clean framework path, not unix compatibility path
# Try to use OSGeo4W installation
IF(WIN32)
    SET(OPENCAD_OSGEO4W_HOME "C:/OSGeo4W") 

    IF($ENV{OSGEO4W_HOME})
        SET(OPENCAD_OSGEO4W_HOME "$ENV{OSGEO4W_HOME}") 
    ENDIF()
ENDIF(WIN32)

FIND_PATH(OPENCAD_INCLUDE_DIR opencad.h
    PATHS ${OPENCAD_OSGEO4W_HOME}/include
    PATH_SUFFIXES opencad
    DOC "Path to OPENCAD library include directory")

SET(OPENCAD_NAMES ${OPENCAD_NAMES} opencad opencad_i)
FIND_LIBRARY(OPENCAD_LIBRARY
    NAMES ${OPENCAD_NAMES}
    PATHS ${OPENCAD_OSGEO4W_HOME}/lib
    DOC "Path to OPENCAD library file")

if(OPENCAD_INCLUDE_DIR)
    set(OPENCAD_VERSION_MAJOR 0)
    set(OPENCAD_VERSION_MINOR 0)
    set(OPENCAD_VERSION_PATCH 0)
    set(OPENCAD_VERSION_NAME "EARLY RELEASE")

    if(EXISTS "${OPENCAD_INCLUDE_DIR}/opencad.h")
        file(READ "${OPENCAD_INCLUDE_DIR}/opencad.h" OPENCAD_API_H_CONTENTS)
        string(REGEX MATCH "OCAD_VERSION_MAJOR[ \t]+([0-9]+)"
          OPENCAD_VERSION_MAJOR ${OPENCAD_API_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          OPENCAD_VERSION_MAJOR ${OPENCAD_VERSION_MAJOR})
        string(REGEX MATCH "OCAD_VERSION_MINOR[ \t]+([0-9]+)"
          OPENCAD_VERSION_MINOR ${OPENCAD_API_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          OPENCAD_VERSION_MINOR ${OPENCAD_VERSION_MINOR})

        string(REGEX MATCH "OCAD_VERSION_REV[ \t]+([0-9]+)"
          OPENCAD_VERSION_PATCH ${OPENCAD_API_H_CONTENTS})
        string (REGEX MATCH "([0-9]+)"
          OPENCAD_VERSION_PATCH ${OPENCAD_VERSION_PATCH})

        unset(OPENCAD_API_H_CONTENTS)
    endif()
      
    set(OPENCAD_VERSION_STRING "${OPENCAD_VERSION_MAJOR}.${OPENCAD_VERSION_MINOR}.${OPENCAD_VERSION_PATCH}")   
endif ()    
         
# Handle the QUIETLY and REQUIRED arguments and set SPATIALINDEX_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCAD
                                  FOUND_VAR OPENCAD_FOUND
                                  REQUIRED_VARS OPENCAD_LIBRARY OPENCAD_INCLUDE_DIR 
                                  VERSION_VAR OPENCAD_VERSION_STRING)

IF(OPENCAD_FOUND)
  set(OPENCAD_LIBRARIES ${OPENCAD_LIBRARY})
  set(OPENCAD_INCLUDE_DIRS ${OPENCAD_INCLUDE_DIR})
  if(NOT TARGET OpenCAD::opencad)
      add_library(OpenCAD::opencad UNKNOWN IMPORTED)
      set_target_properties(OpenCAD::opencad PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${OPENCAD_INCLUDE_DIR}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES C
                            IMPORTED_LOCATION "${OPENCAD_LIBRARY}")
  endif()
ENDIF()

# Hide internal variables
mark_as_advanced(
  OPENCAD_INCLUDE_DIR
  OPENCAD_LIBRARY)

#======================
