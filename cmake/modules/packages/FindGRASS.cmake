# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# Find GRASS
# ~~~~~~~~~~
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>

# Macro that checks for extra include directories set during GRASS compilation.
# This helps for platforms where GRASS is built against dependencies in
# non-standard locations; like on Mac, where the system gettext is too old and
# GRASS is built off of gettext in /usr/local/opt, or some other custom prefix.
# Such includes may need found again when including some GRASS headers.

macro (CHECK_GRASS_EXTRA_INCLUDE_DIRS GRASS_VERSION)
    set(GRASS_EXTRA_INCLUDE_DIRS${GRASS_VERSION} ""
            CACHE STRING "Extra includes string used for GRASS${GRASS_VERSION}")

    if(UNIX AND EXISTS ${GRASS_INCLUDE_DIR${GRASS_VERSION}}/Make/Platform.make
            AND "${GRASS${GRASS_VERSION}_EXTRA_INCLUDE_DIRS}" STREQUAL "")

        file(READ ${GRASS_INCLUDE_DIR${GRASS_VERSION}}/Make/Platform.make _platformfile)
        string(REGEX MATCH "INCLUDE_DIRS *= *[^\n]*" _config_includes "${_platformfile}")
        set(_extra_includes "")
        if(NOT "${_config_includes}" STREQUAL "")
            string(REGEX REPLACE "INCLUDE_DIRS *= *([^\n]*)" "\\1" _extra_includes "${_config_includes}")
        endif()
        if(NOT "${_extra_includes}" STREQUAL "")
            set(GRASS_EXTRA_INCLUDE_DIRS${GRASS_VERSION} ${_extra_includes}
                    CACHE STRING "Extra includes string used for GRASS${GRASS_VERSION}" FORCE)
        endif()
    endif()

    mark_as_advanced (GRASS_EXTRA_INCLUDE_DIRS${GRASS_VERSION})
endmacro (CHECK_GRASS_EXTRA_INCLUDE_DIRS GRASS_VERSION)

# macro that checks for grass installation in specified directory

macro (CHECK_GRASS G_PREFIX)
    #MESSAGE(STATUS "Find GRASS ${GRASS_FIND_VERSION} in ${G_PREFIX}")

    find_path(GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION} grass/version.h ${G_PREFIX}/include DOC "Path to GRASS ${GRASS_FIND_VERSION} include directory")

    #MESSAGE(STATUS "GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION} = ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}")

    if(GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION} AND EXISTS ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}/grass/version.h)
        file(READ ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}/grass/version.h VERSIONFILE)
        # We can avoid the following block using version_less version_equal and
        # version_greater. Are there compatibility problems?
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[^ ]+" GRASS_VERSION${GRASS_FIND_VERSION} ${VERSIONFILE})
        string(REGEX REPLACE "^([0-9]*)\\.[0-9]*\\..*$" "\\1" GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} ${GRASS_VERSION${GRASS_FIND_VERSION}})
        string(REGEX REPLACE "^[0-9]*\\.([0-9]*)\\..*$" "\\1" GRASS_MINOR_VERSION${GRASS_FIND_VERSION} ${GRASS_VERSION${GRASS_FIND_VERSION}})
        string(REGEX REPLACE "^[0-9]*\\.[0-9]*\\.(.*)$" "\\1" GRASS_MICRO_VERSION${GRASS_FIND_VERSION} ${GRASS_VERSION${GRASS_FIND_VERSION}})
        # Add micro version too?
        # How to numerize RC versions?
        math( EXPR GRASS_NUM_VERSION${GRASS_FIND_VERSION} "${GRASS_MAJOR_VERSION${GRASS_FIND_VERSION}}*10000 + ${GRASS_MINOR_VERSION${GRASS_FIND_VERSION}}*100")

        #MESSAGE(STATUS "GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} = ${GRASS_MAJOR_VERSION${GRASS_FIND_VERSION}}")
        if(GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} EQUAL GRASS_FIND_VERSION)
            set(GRASS_LIBRARIES_FOUND${GRASS_FIND_VERSION} TRUE)
            set(GRASS_LIB_NAMES${GRASS_FIND_VERSION} gis dig2 dbmiclient dbmibase shape dgl rtree datetime linkm gproj)
            if(GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} LESS 7 )
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} vect)
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} form)
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} I)
            else(GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} LESS 7 )
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} vector)
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} raster)
                list(APPEND GRASS_LIB_NAMES${GRASS_FIND_VERSION} imagery)
            endif(GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} LESS 7 )

            foreach(LIB ${GRASS_LIB_NAMES${GRASS_FIND_VERSION}})
                mark_as_advanced ( GRASS_LIBRARY${GRASS_FIND_VERSION}_${LIB} )

                set(LIB_PATH NOTFOUND)
                # FIND_PATH and FIND_LIBRARY normally search standard locations
                # before the specified paths. To search non-standard paths first,
                # FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
                # and then again with no specified paths to search the default
                # locations. When an earlier FIND_* succeeds, subsequent FIND_*s
                # searching for the same item do nothing.
                find_library(LIB_PATH NAMES grass_${LIB} PATHS ${G_PREFIX}/lib NO_DEFAULT_PATH)
                find_library(LIB_PATH NAMES grass_${LIB} PATHS ${G_PREFIX}/lib)

                if(LIB_PATH)
                    set(GRASS_LIBRARY${GRASS_FIND_VERSION}_${LIB} ${LIB_PATH})
                else(LIB_PATH)
                    set(GRASS_LIBRARY${GRASS_FIND_VERSION}_${LIB} NOTFOUND)
                    set(GRASS_LIBRARIES_FOUND${GRASS_FIND_VERSION} FALSE)
                endif (LIB_PATH)
            endforeach(LIB)

            # LIB_PATH is only temporary variable, so hide it (is it possible to delete a variable?)
            unset(LIB_PATH CACHE)

            # Find off_t size
            if( (GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} EQUAL 7) AND (GRASS_MINOR_VERSION${GRASS_FIND_VERSION} GREATER 0) )
                set(GRASS_TEST_MAPSET ${CMAKE_BINARY_DIR}/grass-location/PERMANENT)
                file(MAKE_DIRECTORY ${GRASS_TEST_MAPSET})
                file(WRITE ${GRASS_TEST_MAPSET}/DEFAULT_WIND "")
                file(WRITE ${GRASS_TEST_MAPSET}/WIND "")
                # grass command is not in G_PREFIX but in some bin dir, so it must be in PATH
                set(GRASS_EXE grass7${GRASS_MINOR_VERSION${GRASS_FIND_VERSION}})
                #MESSAGE(STATUS "GRASS_EXE = ${GRASS_EXE}")
                execute_process(COMMAND ${GRASS_EXE} ${GRASS_TEST_MAPSET} --exec g.version -g
                        COMMAND grep build_off_t_size
                        COMMAND sed "s/.*\\([0-9]\\).*/\\1/"
                        ERROR_VARIABLE GRASS_TMP_ERROR
                        OUTPUT_VARIABLE GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}
                        )
                if ( NOT ${GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}} STREQUAL "" )
                    STRING(STRIP ${GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}} GRASS_OFF_T_SIZE${GRASS_FIND_VERSION})
                endif()
                #MESSAGE(STATUS "GRASS_OFF_T_SIZE${GRASS_FIND_VERSION} = ${GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}}")
            endif( (GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} EQUAL 7) AND (GRASS_MINOR_VERSION${GRASS_FIND_VERSION} GREATER 0) )

            if ( "${GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}}" STREQUAL "" )
                if(EXISTS ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}/Make/Platform.make)
                    file(READ ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}/Make/Platform.make PLATFORMFILE)
                    string(REGEX MATCH "LFS_CFLAGS *=[^\n]*" PLATFORM_LFS_CFLAGS ${PLATFORMFILE})
                    if ( NOT "${PLATFORM_LFS_CFLAGS}" STREQUAL "" )
                        string(REGEX MATCH "_FILE_OFFSET_BITS=.." FILE_OFFSET_BITS ${PLATFORM_LFS_CFLAGS})
                        #MESSAGE(STATUS "FILE_OFFSET_BITS = ${FILE_OFFSET_BITS}")
                        if ( NOT "${FILE_OFFSET_BITS}" STREQUAL "" )
                            string(REGEX MATCH "[0-9][0-9]" FILE_OFFSET_BITS ${FILE_OFFSET_BITS})
                            #MESSAGE(STATUS "FILE_OFFSET_BITS = ${FILE_OFFSET_BITS}")
                            if ( "${FILE_OFFSET_BITS}" STREQUAL "32" )
                                SET( GRASS_OFF_T_SIZE${GRASS_FIND_VERSION} 4 )
                            elseif( "${FILE_OFFSET_BITS}" STREQUAL "64" )
                                SET( GRASS_OFF_T_SIZE${GRASS_FIND_VERSION} 8 )
                            ENDIF()
                        ENDIF()
                    ENDIF()
                ENDIF()
            ENDIF()

            IF(GRASS_LIBRARIES_FOUND${GRASS_FIND_VERSION})
                SET(GRASS_FOUND${GRASS_FIND_VERSION} TRUE)
                SET(GRASS_FOUND TRUE) # GRASS_FOUND is true if at least one version was found
                SET(GRASS_PREFIX${GRASS_CACHE_VERSION} ${G_PREFIX})
                CHECK_GRASS_EXTRA_INCLUDE_DIRS(${GRASS_FIND_VERSION})
            ENDIF(GRASS_LIBRARIES_FOUND${GRASS_FIND_VERSION})
        ENDIF(GRASS_MAJOR_VERSION${GRASS_FIND_VERSION} EQUAL GRASS_FIND_VERSION)
    ENDIF(GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION} AND EXISTS ${GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION}}/grass/version.h)

    MARK_AS_ADVANCED ( GRASS_INCLUDE_DIR${GRASS_CACHE_VERSION} )
ENDMACRO (CHECK_GRASS)

###################################
# search for grass installations

MESSAGE(STATUS "GRASS_FIND_VERSION = ${GRASS_FIND_VERSION}")

# list of paths which to search - user's choice as first
SET (GRASS_PATHS ${GRASS_PREFIX${GRASS_CACHE_VERSION}} /usr/lib/grass /opt/grass $ENV{GRASS_PREFIX${GRASS_CACHE_VERSION}})

# os specific paths
IF (WIN32)
    LIST(APPEND GRASS_PATHS c:/msys/local)
ENDIF (WIN32)

IF (UNIX)
    IF (GRASS_FIND_VERSION EQUAL 7)
        LIST(APPEND GRASS_PATHS /usr/lib64/grass70 /usr/lib/grass70 /usr/lib64/grass71 /usr/lib/grass71 /usr/lib64/grass72 /usr/lib/grass72)
    ENDIF ()
ENDIF (UNIX)

IF (APPLE)
    IF (GRASS_FIND_VERSION EQUAL 7)
        LIST(APPEND GRASS_PATHS
                /Applications/GRASS-7.0.app/Contents/MacOS
                /Applications/GRASS-7.1.app/Contents/MacOS
                /Applications/GRASS-7.2.app/Contents/MacOS
                )
    ENDIF ()
    LIST(APPEND GRASS_PATHS /Applications/GRASS.app/Contents/Resources)
ENDIF (APPLE)

IF (WITH_GRASS${GRASS_CACHE_VERSION})
    FOREACH (G_PREFIX ${GRASS_PATHS})
        IF (NOT GRASS_FOUND${GRASS_FIND_VERSION})
            CHECK_GRASS(${G_PREFIX})
        ENDIF (NOT GRASS_FOUND${GRASS_FIND_VERSION})
    ENDFOREACH (G_PREFIX)
ENDIF (WITH_GRASS${GRASS_CACHE_VERSION})

###################################

IF (GRASS_FOUND${GRASS_FIND_VERSION})
    IF (NOT GRASS_FIND_QUIETLY)
        MESSAGE(STATUS "Found GRASS ${GRASS_FIND_VERSION}: ${GRASS_PREFIX${GRASS_CACHE_VERSION}} (${GRASS_VERSION${GRASS_FIND_VERSION}}, off_t size = ${GRASS_OFF_T_SIZE${GRASS_FIND_VERSION}})")
    ENDIF (NOT GRASS_FIND_QUIETLY)

ELSE (GRASS_FOUND${GRASS_FIND_VERSION})

    IF (WITH_GRASS${GRASS_CACHE_VERSION})

        IF (GRASS_FIND_REQUIRED)
            MESSAGE(FATAL_ERROR "Could not find GRASS ${GRASS_FIND_VERSION}")
        ELSE (GRASS_FIND_REQUIRED)
            MESSAGE(STATUS "Could not find GRASS ${GRASS_FIND_VERSION}")
        ENDIF (GRASS_FIND_REQUIRED)

    ENDIF (WITH_GRASS${GRASS_CACHE_VERSION})

ENDIF (GRASS_FOUND${GRASS_FIND_VERSION})