# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#.rst
# Get from http://www.cmake.org/Wiki/CMakeUserFindMySQL
# - Find mysqlclient
# Find the native MySQL includes and library
#
#  MYSQL_INCLUDE_DIRS- where to find mysql.h, etc.
#  MYSQL_LIBRARIES   - List of libraries when using MySQL.
#  MYSQL_FOUND       - True if MySQL found.

find_path(MYSQL_INCLUDE_DIR
          NAMES mysql.h
          PATH_SUFFIXES mysql
          DOC "MySQL Client library includes")

if( MYSQL_INCLUDE_DIR AND EXISTS "${MYSQL_INCLUDE_DIR}/mysql_version.h" )
    file( STRINGS "${MYSQL_INCLUDE_DIR}/mysql_version.h"
          MYSQL_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
    string( REGEX REPLACE
            "^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING
            "${MYSQL_VERSION_H}" )
endif()

find_library(MYSQL_LIBRARY NAMES mysqlclient mysqlclient_r)

if( NOT CMAKE_C_COMPILER_LOADED )
    message(AUTHOR_WARNING "C language not enabled: Skipping detection of extra link libraries.")
elseif( MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY )
    # On Conda, mysqlclient is a static lib that requires explicit linking to zlib and zstd
    function(check_mysql_test_program_links)
        include(CheckCSourceCompiles)
        include(CMakePushCheckState)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_QUIET "yes")
        set(CMAKE_REQUIRED_INCLUDES ${MYSQL_INCLUDE_DIR})
        set(CMAKE_REQUIRED_LIBRARIES ${MYSQL_LIBRARY})
        check_c_source_compiles("#include <mysql.h>\n; int main () {mysql_library_end (); return 0;}" MYSQL_TEST_PROGRAM_LINKS)
        cmake_pop_check_state()
    endfunction()

    check_mysql_test_program_links()
    if(NOT MYSQL_TEST_PROGRAM_LINKS)
       find_library(_MYSQL_ZLIB_LIBRARY NAMES zlib)
       find_library(_MYSQL_ZSTD_LIBRARY NAMES zstd)
       if(_MYSQL_ZLIB_LIBRARY AND _MYSQL_ZSTD_LIBRARY)
            function(check_mysql_test_program_links_with_zlib_and_zstd)
                include(CheckCSourceCompiles)
                include(CMakePushCheckState)
                cmake_push_check_state(RESET)
                set(CMAKE_REQUIRED_QUIET "yes")
                set(CMAKE_REQUIRED_INCLUDES ${MYSQL_INCLUDE_DIR})
                set(CMAKE_REQUIRED_LIBRARIES ${MYSQL_LIBRARY} ${_MYSQL_ZSTD_LIBRARY} ${_MYSQL_ZLIB_LIBRARY})
                check_c_source_compiles("#include <mysql.h>\n;int main () {mysql_library_end (); return 0;}" MYSQL_TEST_PROGRAM_LINKS_WITH_ZLIB_AND_ZSTD)
                cmake_pop_check_state()
            endfunction()

            check_mysql_test_program_links_with_zlib_and_zstd()
            if(MYSQL_TEST_PROGRAM_LINKS_WITH_ZLIB_AND_ZSTD)
              set(MYSQL_ZSTD_LIBRARY ${_MYSQL_ZSTD_LIBRARY})
              set(MYSQL_ZLIB_LIBRARY ${_MYSQL_ZLIB_LIBRARY})
            endif()
       endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL
                                  FOUND_VAR MYSQL_FOUND
                                  REQUIRED_VARS MYSQL_LIBRARY MYSQL_INCLUDE_DIR
                                  VERSION_VAR MYSQL_VERSION_STRING)
include(FeatureSummary)
set_package_properties(MYSQL PROPERTIES
                       DESCRIPTION "MySQL Client library"
                       URL "https://dev.mysql.com/downloads/c-api/"
                       )

mark_as_advanced(MYSQL_LIBRARY MYSQL_INCLUDE_DIR)

set(MYSQL_LIBRARIES ${MYSQL_LIBRARY} ${MYSQL_ZSTD_LIBRARY} ${MYSQL_ZLIB_LIBRARY})
set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})

