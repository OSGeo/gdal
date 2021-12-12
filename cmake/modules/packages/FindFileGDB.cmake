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
# If it's found it sets FileGDB_FOUND to TRUE
# and following variables are set:
#    FileGDB_INCLUDE_DIR
#    FileGDB_LIBRARY
#    FileGDB_VERSION
#
# Select a FGDB API to use, or disable driver.

if(CMAKE_VERSION VERSION_LESS 3.13)
    set(FileGDB_ROOT CACHE PATH "")
endif()

find_path(FileGDB_INCLUDE_DIR NAMES FileGDBAPI.h
          PATHS /usr/local/filegdb/include "${FileGDB_ROOT}/include")
mark_as_advanced(FileGDB_INCLUDE_DIR)

if(FileGDB_INCLUDE_DIR)
    if(WIN32)
        if(NOT FileGDB_LIBRARY)
          find_library(FileGDB_LIBRARY_RELEASE NAMES
              FileGDBAPI
              PATHS "${FileGDB_ROOT}/lib" "${FileGDB_ROOT}/lib64"
          )
          mark_as_advanced(FileGDB_LIBRARY_RELEASE)

          find_library(FileGDB_LIBRARY_DEBUG NAMES
              FileGDBAPID
              PATHS "${FileGDB_ROOT}/lib" "${FileGDB_ROOT}/lib64"
          )
          mark_as_advanced(FileGDB_LIBRARY_DEBUG)

          include(SelectLibraryConfigurations)
          select_library_configurations(FileGDB)
        endif()
    else()
        mark_as_advanced(FileGDB_LIBRARY)

        find_library(FileGDB_LIBRARY NAMES FileGDBAPI PATHS "${FileGDB_ROOT}/lib" "${FileGDB_ROOT}/lib64")
        include(CheckCXXSourceCompiles)
        cmake_push_check_state(RESET)
        check_cxx_source_compiles("#include <FileGDBAPI.h>\nusing namespace FileGDBAPI;\n
                int main() { Geodatabase oDB; std::wstring osStr; ::OpenGeodatabase(osStr, oDB); return 0; }" TEST_FileGDB_COMPILE)
        cmake_pop_check_state()
        if(NOT TEST_FileGDB_COMPILE)
            set(FileGDB_FOUND FALSE)
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FileGDB
                                  FOUND_VAR FileGDB_FOUND
                                  REQUIRED_VARS FileGDB_LIBRARY FileGDB_INCLUDE_DIR)

if(FileGDB_FOUND)
    set(FileGDB_INCLUDE_DIRS ${FileGDB_INCLUDE_DIR})
    set(FileGDB_LIBRARIES ${FileGDB_LIBRARY})
    if(NOT TARGET FILEGDB::FileGDB)
        add_library(FILEGDB::FileGDB UNKNOWN IMPORTED)
        set_target_properties(FILEGDB::FileGDB PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${FileGDB_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C")

        if(EXISTS "${FileGDB_LIBRARY}")
          set_target_properties(FILEGDB::FileGDB PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${FileGDB_LIBRARY}")
        endif()
        if(FileGDB_LIBRARY_RELEASE)
          set_property(TARGET FILEGDB::FileGDB APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
          set_target_properties(FILEGDB::FileGDB PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION_RELEASE "${FileGDB_LIBRARY_RELEASE}")
        endif()
        if(FileGDB_LIBRARY_DEBUG)
          set_property(TARGET FILEGDB::FileGDB APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
          set_target_properties(FILEGDB::FileGDB PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION_DEBUG "${FileGDB_LIBRARY_DEBUG}")
        endif()

    endif()
endif()
