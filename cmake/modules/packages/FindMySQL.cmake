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

find_library(MYSQL_LIBRARY
						 NAMES mysqlclient mysqlclient_r)

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

set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})

