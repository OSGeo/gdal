# Distributed under the GDAL/OGR MIT style License.  See accompanying
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

set(GDAL_ROOT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")

# parse the version number from gdal_version.h and include in GDAL_MAJOR_VERSION and GDAL_MINOR_VERSION
file(READ ${GDAL_ROOT_SOURCE_DIR}/gcore/gdal_version.h.in GDAL_VERSION_H_CONTENTS)
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

if (STANDALONE)
    return()
endif()

if ((EXISTS "${GDAL_ROOT_SOURCE_DIR}/gcore/gdal_version.h") AND NOT ("${GDAL_ROOT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}"))
    # Try to detect issues when building with cmake out of source tree, but against a previous build done in source tree
    message(FATAL_ERROR "${GDAL_ROOT_SOURCE_DIR}/gcore/gdal_version.h was found, and likely conflicts with ${PROJECT_BINARY_DIR}/gcore/gdal_version.h")
endif ()

if (EXISTS ${GDAL_ROOT_SOURCE_DIR}/.git)
    set(GDAL_DEV_SUFFIX "dev")

    # Try look in the Git tags to see if it is a special version
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" "tag" "--points-at" "HEAD"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            RESULT_VARIABLE git_result
            OUTPUT_VARIABLE git_tags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(git_result EQUAL 0)
            # First replaces the line breaks by a ";", so we can iterate over it
            string(REGEX REPLACE "\r?\n" ";" git_tags "${git_tags}")
            foreach(git_tag ${git_tags})
                # GDAL is currently not fully compatible with Semantic Versioning 2.0.0, as there would have to be a minus before the pre-release tag, so we allow it to be omitted.
                if (git_tag MATCHES "^v${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV}-?(.*)")
                    # ${CMAKE_MATCH_1} contains the pre-release suffix
                    if (NOT CMAKE_MATCH_1)
                        # In case there is no pre-release suffix, it is a release version.
                        set(GDAL_DEV_SUFFIX "")
                        # Ignore further suffixes (e.g., a RC tag)
                        break()
                    else()
                        # Take the pre-release suffix as dev suffix. Normally there should only be one pre-release tag,
                        # so we don't need any logic here to compare the pre-release tags with each other. However,
                        # as there may still be a release tag without a pre-release suffix, we do not break the loop.
                        set(GDAL_DEV_SUFFIX "${CMAKE_MATCH_1}")
                    endif()
                endif()
            endforeach()
        endif()
    endif()
else()
    set(GDAL_DEV_SUFFIX "")
endif()

# Used for GDAL docker builds
set(GDAL_SHA1SUM "$ENV{GDAL_SHA1SUM}")
set(GDAL_RELEASE_DATE "$ENV{GDAL_RELEASE_DATE}")

add_custom_target(generate_gdal_version_h
                  COMMAND ${CMAKE_COMMAND}
                    "-DSOURCE_DIR=${GDAL_ROOT_SOURCE_DIR}"
                    "-DBINARY_DIR=${PROJECT_BINARY_DIR}"
                    "-DGDAL_SHA1SUM=${GDAL_SHA1SUM}"
                    "-DGDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}"
                    -P "${GDAL_ROOT_SOURCE_DIR}/cmake/helpers/generate_gdal_version_h.cmake"
                  VERBATIM)

if (WIN32 AND NOT MINGW)
  set(GDAL_SOVERSION "")
  set(GDAL_ABI_FULL_VERSION "${GDAL_VERSION_MAJOR}${GDAL_VERSION_MINOR}")
else()
  set(GDAL_ABI_FULL_VERSION "${GDAL_SOVERSION}.${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV}")
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
