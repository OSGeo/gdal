# Find json-c
# ~~~~~~~~~
# Copyright (c) 2012, Dmitry Baryshnikov <polimax at mail.ru>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for Libkml library
#
# If it's found it sets JSONC_FOUND to TRUE
# and following variables are set:
#    JSONC_INCLUDE_DIR
#    JSONC_LIBRARY

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
    #FIND_PATH(JSONC_INCLUDE_DIR JSONC/dom.h)
    FIND_LIBRARY(JSONC_LIBRARY JSONC)
    IF (JSONC_LIBRARY)
      # FIND_PATH doesn't add "Headers" for a framework
      SET (JSONC_INCLUDE_DIR ${JSONC_LIBRARY}/Headers CACHE PATH "Path to a file.")
    ENDIF (JSONC_LIBRARY)
    SET (CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_save} CACHE STRING "" FORCE)
  ENDIF ()
ENDIF (APPLE)

FIND_PATH(JSONC_INCLUDE_DIR json.h
  "$ENV{LIB_DIR}/"
  "$ENV{LIB_DIR}/include/"
  "$ENV{JSONC_ROOT}/"
  /usr/include/json-c
  /usr/local/include/json-c
  #mingw
  c:/msys/local/include/json-c
  NO_DEFAULT_PATH
  )
FIND_PATH(JSONC_INCLUDE_DIR json.h)

if(CMAKE_CL_64)
FIND_LIBRARY(JSONC_LIBRARY NAMES json-c libjson-c libjson PATHS
  "$ENV{LIB_DIR}/lib"
  "$ENV{JSONC_ROOT}/lib"
  "$ENV{JSONC_ROOT}/lib/x64"
  /usr/lib
  /usr/local/lib
  #mingw
  c:/msys/local/lib
  NO_DEFAULT_PATH
  )
else(CMAKE_CL_64)
FIND_LIBRARY(JSONC_LIBRARY NAMES json-c libjson-c libjson PATHS
  "$ENV{LIB_DIR}/lib"
  "$ENV{JSONC_ROOT}/lib"
  "$ENV{JSONC_ROOT}/lib/x32"
  /usr/lib
  /usr/local/lib
  #mingw
  c:/msys/local/lib
  NO_DEFAULT_PATH
  )
endif(CMAKE_CL_64)  
FIND_LIBRARY(JSONC_LIBRARY NAMES json-c libjson-c libjson)

IF (JSONC_INCLUDE_DIR AND JSONC_LIBRARY)
   SET(JSONC_FOUND TRUE)
ENDIF (JSONC_INCLUDE_DIR AND JSONC_LIBRARY)


IF (JSONC_FOUND)

   IF (NOT JSONC_FIND_QUIETLY)
      MESSAGE(STATUS "Found JSONC: ${JSONC_LIBRARY}")
      MESSAGE(STATUS "Found JSONC Headers: ${JSONC_INCLUDE_DIR}")
   ENDIF (NOT JSONC_FIND_QUIETLY)

ELSE (JSONC_FOUND)

   IF (JSONC_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find JSONC")
   ENDIF (JSONC_FIND_REQUIRED)

ENDIF (JSONC_FOUND)
