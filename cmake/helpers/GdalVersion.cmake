# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
GdalVersion
-----------

 Retrieve GDAL version number from header file and set it to GDAL_VERSION
 cmake variable.

 Make compilation more ccache friendly now that GDAL_RELEASE_NAME embeds sha1sum
 Generate gcore/gdal_version.h from git date and sha for a dev version
 If gcore/gdal_version.h.in contains 'dev' in GDAL_RELEASE_NAME then include
 the sha of the latest commit in it. And replace GDAL_RELEASE_DATE with the
 date of the commit.

#]=======================================================================]

# parse the version number from gdal_version.h and include in GDAL_MAJOR_VERSION and GDAL_MINOR_VERSION
file(READ ${PROJECT_SOURCE_DIR}/gcore/gdal_version.h.in GDAL_VERSION_H_CONTENTS)
string(REGEX MATCH "GDAL_VERSION_MAJOR[ \t]+([0-9]+)"
       GDAL_VERSION_MAJOR ${GDAL_VERSION_H_CONTENTS})
string(REGEX MATCH "([0-9]+)"
       GDAL_VERSION_MAJOR ${GDAL_VERSION_MAJOR})
string(REGEX MATCH "GDAL_VERSION_MINOR[ \t]+([0-9]+)"
       GDAL_VERSION_MINOR ${GDAL_VERSION_H_CONTENTS})
string(REGEX MATCH "([0-9]+)"
       GDAL_VERSION_MINOR ${GDAL_VERSION_MINOR})
string(REGEX MATCH "GDAL_VERSION_REV[ \t]+([0-9]+)"
       GDAL_VERSION_REV ${GDAL_VERSION_H_CONTENTS})
string(REGEX MATCH "([0-9]+)"
       GDAL_VERSION_REV ${GDAL_VERSION_REV})
string(REGEX MATCH "GDAL_VERSION_BUILD[ \t]+([0-9]+)"
       GDAL_VERSION_BUILD ${GDAL_VERSION_H_CONTENTS})
string(REGEX MATCH "([0-9]+)"
       GDAL_VERSION_BUILD ${GDAL_VERSION_BUILD})

if ((EXISTS "${PROJECT_SOURCE_DIR}/gcore/gdal_version.h") AND NOT ("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}"))
    # Try to detect issues when building with cmake out of source tree, but against a previous build done in source tree
    message(FATAL_ERROR "${PROJECT_SOURCE_DIR}/gcore/gdal_version.h was found, and likely conflicts with ${PROJECT_BINARY_DIR}/gcore/gdal_version.h")
endif ()

if (EXISTS ${PROJECT_SOURCE_DIR}/.git)
    set(GDAL_DEV_SUFFIX "dev")
else()
    set(GDAL_DEV_SUFFIX "")
endif()

# Used for GDAL docker builds
set(GDAL_SHA1SUM "$ENV{GDAL_SHA1SUM}")
set(GDAL_RELEASE_DATE "$ENV{GDAL_RELEASE_DATE}")

add_custom_target(generate_gdal_version_h
                  COMMAND ${CMAKE_COMMAND}
                    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
                    "-DBINARY_DIR=${PROJECT_BINARY_DIR}"
                    "-DGDAL_SHA1SUM=${GDAL_SHA1SUM}"
                    "-DGDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}"
                    -P "${PROJECT_SOURCE_DIR}/cmake/helpers/generate_gdal_version_h.cmake"
                  VERBATIM)

if (WIN32)
  set(GDAL_SOVERSION "")
  set(GDAL_ABI_FULL_VERSION "${GDAL_VERSION_MAJOR}${GDAL_VERSION_MINOR}")
else()
  file(READ ${PROJECT_SOURCE_DIR}/GDALmake.opt.in GDAL_MAKE_OPT_CONTENT)
  # Once we have a CMake-only build, those variables will have to be manually set
  string(REGEX MATCH "LIBGDAL_CURRENT[ \t]*:=[ \t]*([0-9]+)" GDAL_LIBTOOL_CURRENT "${GDAL_MAKE_OPT_CONTENT}")
  string(REGEX MATCH "LIBGDAL_REVISION[ \t]*:=[ \t]*([0-9]+)" GDAL_LIBTOOL_REVISION "${GDAL_MAKE_OPT_CONTENT}")
  string(REGEX MATCH "LIBGDAL_AGE[ \t]*:=[ \t]*([0-9]+)" GDAL_LIBTOOL_AGE "${GDAL_MAKE_OPT_CONTENT}")
  string(REGEX MATCH "([0-9]+)" GDAL_LIBTOOL_CURRENT "${GDAL_LIBTOOL_CURRENT}")
  string(REGEX MATCH "([0-9]+)" GDAL_LIBTOOL_REVISION "${GDAL_LIBTOOL_REVISION}")
  string(REGEX MATCH "([0-9]+)" GDAL_LIBTOOL_AGE "${GDAL_LIBTOOL_AGE}")
  if( (GDAL_LIBTOOL_CURRENT STREQUAL "") OR (GDAL_LIBTOOL_REVISION STREQUAL "") OR (GDAL_LIBTOOL_AGE STREQUAL "") )
      message(FATAL_ERROR "Missing libtool numbers")
  endif()

  math(EXPR GDAL_SOVERSION "${GDAL_LIBTOOL_CURRENT} - ${GDAL_LIBTOOL_AGE}")
  set(GDAL_ABI_FULL_VERSION "${GDAL_SOVERSION}.${GDAL_LIBTOOL_AGE}.${GDAL_LIBTOOL_REVISION}")
endif ()

# Setup package meta-data
set(GDAL_VERSION_NO_DEV_SUFFIX ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV})
set(GDAL_VERSION ${GDAL_VERSION_NO_DEV_SUFFIX}${GDAL_DEV_SUFFIX})

set(${PROJECT_NAME}_VERSION ${GDAL_VERSION})
set(${PROJECT_NAME}_VERSION_MAJOR ${GDAL_VERSION_MAJOR})
set(${PROJECT_NAME}_VERSION_MINOR ${GDAL_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION_PATCH ${GDAL_VERSION_REV})
set(${PROJECT_NAME}_VERSION_TWEAK ${GDAL_VERSION_BUILD})

message(STATUS "GDAL_VERSION          = ${GDAL_VERSION}")
message(STATUS "GDAL_ABI_FULL_VERSION = ${GDAL_ABI_FULL_VERSION}")
message(STATUS "GDAL_SOVERSION        = ${GDAL_SOVERSION}")
