# Find Libkml
# ~~~~~~~~~
# Copyright (c) 2012, Dmitry Baryshnikov <polimax at mail.ru>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for Libkml library
#
# If it's found it sets LIBKML_FOUND to TRUE
# and following variables are set:
#    LIBKML_INCLUDE_DIR
#    LIBKML_LIBRARY

# FIND_PATH and FIND_LIBRARY normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing. 

# try to use framework on mac
# want clean framework path, not unix compatibility path
IF (APPLE)
  IF (CMAKE_FIND_FRAMEWORK MATCHES "FIRST"
      OR CMAKE_FRAMEWORK_PATH MATCHES "ONLY"
      OR NOT CMAKE_FIND_FRAMEWORK)
    SET (CMAKE_FIND_FRAMEWORK_save ${CMAKE_FIND_FRAMEWORK} CACHE STRING "" FORCE)
    SET (CMAKE_FIND_FRAMEWORK "ONLY" CACHE STRING "" FORCE)
    #FIND_PATH(LIBKML_INCLUDE_DIR LIBKML/dom.h)
    FIND_LIBRARY(LIBKML_LIBRARY LIBKML)
    IF (LIBKML_LIBRARY)
      # FIND_PATH doesn't add "Headers" for a framework
      SET (LIBKML_INCLUDE_DIR ${LIBKML_LIBRARY}/Headers CACHE PATH "Path to a file.")
    ENDIF (LIBKML_LIBRARY)
    SET (CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_save} CACHE STRING "" FORCE)
  ENDIF ()
ENDIF (APPLE)

FIND_PATH(LIBKML_INCLUDE_DIR_T dom.h
  "$ENV{LIB_DIR}/include/kml"
  "$ENV{LIB_DIR}/include"
  /usr/include/kml
  /usr/local/include/kml
  #mingw
  c:/msys/local/include/kml
  NO_DEFAULT_PATH
  )
#FIND_PATH(LIBKML_INCLUDE_DIR_T dom.h)
mark_as_advanced(LIBKML_INCLUDE_DIR_T)

get_filename_component(LIBKML_INCLUDE_DIR ${LIBKML_INCLUDE_DIR_T} PATH)

FIND_LIBRARY(LIBKML_LIBRARY NAMES kml libkml PATHS
  "$ENV{LIB_DIR}/lib"
  /usr/lib
  /usr/local/lib
  #mingw
  c:/msys/local/lib
  NO_DEFAULT_PATH
  )
FIND_LIBRARY(LIBKML_LIBRARY NAMES libkml)

IF (LIBKML_INCLUDE_DIR AND LIBKML_LIBRARY)
   SET(LIBKML_FOUND TRUE)
ENDIF (LIBKML_INCLUDE_DIR AND LIBKML_LIBRARY)


IF (LIBKML_FOUND)

   IF (NOT LIBKML_FIND_QUIETLY)
      MESSAGE(STATUS "Found LIBKML: ${LIBKML_LIBRARY}")
      MESSAGE(STATUS "Found LIBKML Headers: ${LIBKML_INCLUDE_DIR}")
   ENDIF (NOT LIBKML_FIND_QUIETLY)

ELSE (LIBKML_FOUND)

   IF (LIBKML_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find LIBKML")
   ENDIF (LIBKML_FIND_REQUIRED)

ENDIF (LIBKML_FOUND)
