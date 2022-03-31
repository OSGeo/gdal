###############################################################################
# CMake module to search for ECW library - Enhanced Compression Wavelets for JPEG2000.
#
# Sets
#   ECW_FOUND.  If false, don't try to use ecw
#   ECW_INCLUDE_DIR
#   ECW_LIBRARY
# Author:   Alexander Lisovenko, alexander.lisovenko@gmail.com
# Author:   Dmitry Baryshnikov, bishop.dev@gmail.com
# Copyright (C) 2016, NextGIS <info@nextgis.com>
#
################################################################################


FIND_PATH( ECW_INCLUDE_DIR NCSECWClient.h
  /usr/include
  /usr/local/include
)

IF( ECW_INCLUDE_DIR )
  FIND_LIBRARY( ECW_LIBRARY NCSEcwd
    /usr/lib
    /usr/local/lib
    /usr/lib64
    /usr/local/lib64
  )
  
  set(MAJOR_VERSION 0)
  set(MINOR_VERSION 0)
  set(SRV_VERSION 0)
  set(BLD_VERSION 0)
  
  if(EXISTS "${ECW_INCLUDE_DIR}/ECWJP2BuildNumber.h")
        file(READ "${ECW_INCLUDE_DIR}/ECWJP2BuildNumber.h" VERSION_H_CONTENTS)
        string(REGEX MATCH "_VER_MAJOR[ \t]+([0-9]+)"
      MAJOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MAJOR_VERSION ${c})
    string(REGEX MATCH "_VER_MINOR[ \t]+([0-9]+)"
      MINOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MINOR_VERSION ${MINOR_VERSION})
    string(REGEX MATCH "_VER_SERVICE[ \t]+([0-9]+)"
      SRV_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      SRV_VERSION ${SRV_VERSION})
    string(REGEX MATCH "_VER_SERVICE[ \t]+([0-9]+)"
      BLD_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      BLD_VERSION ${BLD_VERSION})
        unset(VERSION_H_CONTENTS)
    endif()
      
    set(ECW_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}.${SRV_VERSION}.${BLD_VERSION}")

  IF( ECW_LIBRARY )
    add_definitions(-DECWSDK_VERSION=${MAJOR_VERSION}${MINOR_VERSION})
  ENDIF( ECW_LIBRARY )

ENDIF( ECW_INCLUDE_DIR )

# Handle the QUIETLY and REQUIRED arguments and set ECW_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ECW 
                                  REQUIRED_VARS ECW_LIBRARY ECW_INCLUDE_DIR 
                                  VERSION_VAR ECW_VERSION_STRING)

IF(ECW_FOUND)
  set(ECW_LIBRARIES ${ECW_LIBRARY})
  set(ECW_INCLUDE_DIRS ${ECW_INCLUDE_DIR})
ENDIF()

# Hide internal variables
mark_as_advanced(ECW_INCLUDE_DIR ECW_LIBRARY)

