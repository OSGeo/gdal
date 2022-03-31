#.rst:
# FindEXPAT
# ---------
#
# Find expat
#
# Find the native EXPAT headers and libraries.
#
# ::
#
#   EXPAT_INCLUDE_DIRS - where to find expat.h, etc.
#   EXPAT_LIBRARIES    - List of libraries when using expat.
#   EXPAT_FOUND        - True if expat found.

#=============================================================================
# Copyright 2006-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# Look for the header file.
find_path(EXPAT_INCLUDE_DIR NAMES expat.h)

# Look for the library.
find_library(EXPAT_LIBRARY NAMES expat libexpat)

if (EXPAT_INCLUDE_DIR AND EXISTS "${EXPAT_INCLUDE_DIR}/expat.h")
    file(READ ${EXPAT_INCLUDE_DIR}/expat.h _VERSION_H_CONTENTS)

    string(REGEX MATCH "XML_MAJOR_VERSION[ \t]+([0-9]+)"
      XML_MAJOR_VERSION ${_VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      XML_MAJOR_VERSION ${XML_MAJOR_VERSION})
    string(REGEX MATCH "XML_MINOR_VERSION[ \t]+([0-9]+)"
      XML_MINOR_VERSION ${_VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      XML_MINOR_VERSION ${XML_MINOR_VERSION})
    string(REGEX MATCH "XML_MICRO_VERSION[ \t]+([0-9]+)"
      XML_MICRO_VERSION ${_VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      XML_MICRO_VERSION ${XML_MICRO_VERSION})
      
    set(EXPAT_VERSION_STRING "${XML_MAJOR_VERSION}.${XML_MINOR_VERSION}.${XML_MICRO_VERSION}")
endif ()

# handle the QUIETLY and REQUIRED arguments and set EXPAT_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EXPAT
                                  REQUIRED_VARS EXPAT_LIBRARY EXPAT_INCLUDE_DIR
                                  VERSION_VAR EXPAT_VERSION_STRING)

# Copy the results to the output variables.
if(EXPAT_FOUND)
  set(EXPAT_LIBRARIES ${EXPAT_LIBRARY})
  set(EXPAT_INCLUDE_DIRS ${EXPAT_INCLUDE_DIR})
endif()

mark_as_advanced(EXPAT_INCLUDE_DIR EXPAT_LIBRARY)
