include("${SOURCE_DIR}/cmake/modules/init.cmake")

file(READ "${SOURCE_DIR}/gcore/gdal_version.h.in" GDAL_VERSION_H_CONTENTS)
string(CONCAT
       GDAL_VERSION_H_CONTENTS
       "/* This is a generated file from gdal_version.h.in. DO NOT MODIFY !!!! */\n"
       "${GDAL_VERSION_H_CONTENTS}")

if (GDAL_SHA1SUM)
    # Used for GDAL docker builds
    string(REPLACE "dev" "dev-${GDAL_SHA1SUM}"
           GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
    if (GDAL_RELEASE_DATE)
        string(REGEX REPLACE "(define GDAL_RELEASE_DATE[ ]+)([0-9]+)(.*)" "\\1${GDAL_RELEASE_DATE}\\3"
              GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
    endif()

elseif (EXISTS ${SOURCE_DIR}/.git)

    find_package(Git)
    if (GIT_FOUND)
        include(GetGitRevisionDescription)
        get_git_head_revision(GDAL_GIT_REFSPEC GDAL_GIT_HASH "${SOURCE_DIR}")
        if ("${GDAL_GIT_HASH}" STREQUAL "")
            message(WARNING "Cannot determine git hash")
        else()
            include(GetGitHeadDate)
            get_git_head_date(GDAL_GIT_DATE "${SOURCE_DIR}")
            git_local_changes(GIT_LOCAL_CHG "${SOURCE_DIR}")
            string(SUBSTRING "${GDAL_GIT_HASH}" 0 10 REV)
            set(GDAL_DEV_REVISION "dev-${REV}")
            if (GIT_LOCAL_CHG STREQUAL "DIRTY")
              set(GDAL_DEV_REVISION "${GDAL_DEV_REVISION}-dirty")
            elseif (GIT_LOCAL_CHG STREQUAL "UNKNOWN" AND
                    EXISTS "${BINARY_DIR}/gcore/gdal_version.h" AND
                    EXISTS "${BINARY_DIR}/gcore/gdal_version_full/gdal_version.h")
              # Happens typically when running "sudo make install" after "make" as a regular user
              file(READ "${BINARY_DIR}/gcore/gdal_version_full/gdal_version.h" GDAL_FULL_VERSION_H_CONTENTS)
              if( "${GDAL_FULL_VERSION_H_CONTENTS}" MATCHES "${REV}" )
                  # Assume the -dirty state has not been modified. This avoids
                  # rebuilding files
                  return()
              endif()
              # If we get here, the revision number has changed from the latest build
              # but we are not able to determine if there are local changes.
              # Assume none...
            endif()

            string(REPLACE "dev" "${GDAL_DEV_REVISION}"
                   GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
            string(REGEX REPLACE "(define GDAL_RELEASE_DATE[ ]+)([0-9]+)(.*)" "\\1${GDAL_GIT_DATE}\\3"
                  GDAL_VERSION_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
        endif()
    endif ()
endif ()

file(WRITE "${BINARY_DIR}/gcore/gdal_version_full/gdal_version.h.tmp" "${GDAL_VERSION_H_CONTENTS}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BINARY_DIR}/gcore/gdal_version_full/gdal_version.h.tmp"
                "${BINARY_DIR}/gcore/gdal_version_full/gdal_version.h")

# For faster builds, generate a strip down version without GDAL_RELEASE_DATE
# and GDAL_RELEASE_NAME which change quite often in git builds, to save rebuilt
# time
string(REGEX REPLACE "(DO_NOT_DEFINE_GDAL_DATE_NAME\\))(.*)" "\\1\n#endif\n"
          GDAL_VERSION_MINIMUM_H_CONTENTS "${GDAL_VERSION_H_CONTENTS}")
file(WRITE "${BINARY_DIR}/gcore/gdal_version.h.tmp" "${GDAL_VERSION_MINIMUM_H_CONTENTS}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BINARY_DIR}/gcore/gdal_version.h.tmp"
                "${BINARY_DIR}/gcore/gdal_version.h")
