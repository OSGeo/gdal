# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file gdal/LICENSE.TXT.

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
file(READ ${CMAKE_SOURCE_DIR}/gdal/gcore/gdal_version.h.in GDAL_VERSION_H_CONTENTS)
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
set(GDAL_DEV_REVISION "")
if (GDAL_SHA1SUM AND GDAL_RELEASE_DATE)
else ()
  if (EXISTS ${CMAKE_SOURCE_DIR}/.git)
    find_package(Git)
    include(GetGitRevisionDescription)
    get_git_head_revision(GDAL_GIT_REFSPEC GDAL_GIT_HASH ${CMAKE_SOURCE_DIR})
    include(GetGitHeadDate)
    get_git_head_date(GDAL_GIT_DATE ${CMAKE_SOURCE_DIR})
    git_local_changes(GIT_LOCAL_CHG ${CMAKE_SOURCE_DIR})
    string(SUBSTRING ${GDAL_GIT_HASH} 0 10 REV)
    set(GDAL_DEV_REVISION "dev-${REV}")
    file(READ ${CMAKE_SOURCE_DIR}/gdal/gcore/gdal_version.h.in GDAL_VERSION_H_CONTENTS)
    string(CONCAT
           GDAL_VERSION_H_CONTENTS
           "/* This is a generated file from gdal_version.h.in. DO NOT MODIFY !!!! */\n"
           "${GDAL_VERSION_H_CONTENTS}")
    string(REPLACE "dev" "dev-${REV}"
           GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
    string(REPLACE "20189999" ${GDAL_GIT_DATE}
           GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
    file(GENERATE
         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gcore/gdal_version.h
         CONTENT "${GDAL_VERSION_H_CONTENTS}")
  else ()
    file(READ ${CMAKE_SOURCE_DIR}/gdal/gcore/gdal_version.h.in GDAL_VERSION_H_CONTENTS)
    string(CONCAT
           GDAL_VERSION_H_CONTENTS
           "/* This is a generated file from gdal_version.h.in. DO NOT MODIFY !!!! */\n"
           "${GDAL_VERSION_H_CONTENTS}")
    file(GENERATE
         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gcore/gdal_version.h
         CONTENT "${GDAL_VERSION_H_CONTENTS}")
  endif ()
endif ()

if (NOT GDAL_SOVERSION)
  file(READ ${CMAKE_SOURCE_DIR}/gdal/GDALmake.opt.in GDAL_MAKE_OPT_CONTENT)
  string(REGEX MATCH "^LIBGDAL_CURRENT :=[ /t]+([0-9]+)$" "${GDAL_MAKE_OPT_CONTENT}" GDAL_SOVERSION)
  string(REGEX MATCH "([0-9]+)" "${GDAL_SOVERSION}" GDAL_SOVERSION)
  # when fails to get soversion, fallback to some default
  if (NOT GDAL_SOVERSION)
    set(GDAL_SOVERSION "${GDAL_VERSION_MAJOR}${GDAL_VERSION_MINOR}")
  endif ()
endif ()

# Setup package meta-data
set(GDAL_VERSION ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV}${GDAL_DEV_REVISION})
message(STATUS "gdal version=[${GDAL_VERSION}]")
message(STATUS "SO version=${GDAL_SOVERSION}")
