# Find the MRSID library - Multi-resolution Seamless Image Database.
#
# Sets
#   MRSID_FOUND.  If false, don't try to use ecw
#   MRSID_INCLUDE_DIR
#   MRSID_LIBRARIES
# Copyright (c) 2015 NextGIS <info@nextgis.com>
#

FIND_PATH( MRSID_INCLUDE_DIR lt_base.h
  /usr/include
  /usr/local/include
)

IF( MRSID_INCLUDE_DIR )
  SET(SEARCH_DIRS
    /usr/lib
    /usr/local/lib
    /usr/lib64
    /usr/local/lib64
  )
  
  FIND_LIBRARY( MRSID_LIBRARY_LTI NAMES lti_dsdk
    ${SEARCH_DIRS}
  )
  
  FIND_LIBRARY( MRSID_LIBRARY_LTI_LIDAR NAMES lti_lidar_dsdk
    ${SEARCH_DIRS}
  )
    
  SET( MRSID_LIBRARIES ${MRSID_LIBRARY_LTI} ${MRSID_LIBRARY_LTI_LIDAR})

  set(MAJOR_VERSION 0)
  set(MINOR_VERSION 0)
  set(SRV_VERSION 0)
  set(BLD_VERSION 0)
  
  if(EXISTS "${MRSID_INCLUDE_DIR}/lti_version.h")
    file(READ "${MRSID_INCLUDE_DIR}/lti_version.h" VERSION_H_CONTENTS)
    string(REGEX MATCH "LTI_SDK_MAJOR[ \t]+([0-9]+)"
      MAJOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MAJOR_VERSION ${MAJOR_VERSION})
    string(REGEX MATCH "LTI_SDK_MINOR[ \t]+([0-9]+)"
      MINOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MINOR_VERSION ${MINOR_VERSION})
    string(REGEX MATCH "LTI_SDK_REV[ \t]+([0-9]+)"
      REV_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      REV_VERSION ${REV_VERSION})

    unset(VERSION_H_CONTENTS)
  endif()
      
  set(MRSID_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}.${REV_VERSION}")
    
ENDIF( MRSID_INCLUDE_DIR )

# Handle the QUIETLY and REQUIRED arguments and set ECW_FOUND to TRUE
# if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MRSID
                                  REQUIRED_VARS MRSID_LIBRARIES MRSID_INCLUDE_DIR 
                                  VERSION_VAR MRSID_VERSION_STRING)

# Copy the results to the output variables.
if(MRSID_FOUND)
  set(MRSID_LIBRARY ${MRSID_LIBRARIES})
  set(MRSID_INCLUDE_DIRS ${MRSID_INCLUDE_DIR})
endif()

# Hide internal variables
mark_as_advanced(MRSID_INCLUDE_DIRS MRSID_LIBRARIES)
