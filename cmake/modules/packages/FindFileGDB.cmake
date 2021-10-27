# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# Find FGDB - ESRI File Geodatabaselibrary
# ~~~~~~~~~
#
# Copyright (c) 2017,2018 Hiroshi Miura <miurahr@linux.com>
#
# ::
#
# If it's found it sets FILEGDB_FOUND to TRUE
# and following variables are set:
#    FILEGDB_INCLUDE_DIR
#    FILEGDB_LIBRARY
#    FILEGDB_VERSION
#
# Select a FGDB API to use, or disable driver.

if(CMAKE_VERSION VERSION_LESS 3.13)
    set(FILEGDB_ROOT CACHE PATH "")
endif()

find_path(FILEGDB_INCLUDE_DIR NAMES FileGDBAPI.h
          PATHS  /usr/local/filegdb/include
          SUFFIX_PATHS filegdb)

if(FILEGDB_INCLUDE_DIR)
    find_library(FILEGDB_LIBRARY NAMES FileGDBAPI)
    include(CheckCXXSourceCompiles)
    include_directories(FILEGDB_INCLUDE_DIR)
    check_cxx_source_compiles("#include <FileGDBAPI.h>\nusing namespace FileGDBAPI;
            int main() { Geodatabase oDB; std::wstring osStr; ::OpenGeodatabase(osStr, oDB); return 0; }" TEST_FILEGDB_COMPILE)
    if(NOT TEST_FILEGDB_COMPILE)
        set(FILEGDB_FOUND FALSE)
    endif()
    if(UNIX)
        find_library(FILEGDB_RTL_LIBRARY NAMES fgdbunixrtl)
    endif()
endif()
mark_as_advanced(FILEGDB_INCLUDE_DIR FILEGDB_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FileGDB
                                  FOUND_VAR FILEGDB_FOUND
                                  REQUIRED_VARS FILEGDB_LIBRARY FILEGDB_INCLUDE_DIR)

if(FILEGDB_FOUND)
    set(FILEGDB_INCLUDE_DIRS ${FILEGDB_INCLUDE_DIR})
    set(FILEGDB_LIBRARIES ${FILEGDB_LIBRARY} ${FILEGDB_RTL_LIBRARY})
    if(NOT TARGET FILEGDB::FileGDB)
        add_library(FILEGDB::FileGDB UNKNOWN IMPORTED)
        set_target_properties(FILEGDB::FileGDB PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${FILEGDB_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${FILEGDB_LIBRARY}")
        set_property(TARGET FILEGDB::FileGDB APPEND PROPERTY
                     INTERFACE_LINK_LIBRARIES ${FILEGDB_RTL_LIBRARY})
    endif()
endif()