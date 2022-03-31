# - Find PostgreSQL
# Find the PostgreSQL includes and client library
# This module defines
#  POSTGRESQL_INCLUDE_DIR, where to find libpq-fe.h
#  POSTGRESQL_LIBRARIES, libraries needed to use PostgreSQL
#  POSTGRESQL_VERSION, if found, version of PostgreSQL
#  POSTGRESQL_FOUND, if false, do not try to use PostgreSQL
#
# Copyright (c) 2010, Mateusz Loskot, <mateusz@loskot.net>
# Copyright (c) 2006, Jaroslaw Staniek, <js@iidea.pl>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

find_program(PG_CONFIG NAMES pg_config
  PATHS
  /usr/bin
  /usr/local/bin
  $ENV{ProgramFiles}/PostgreSQL/*/bin
  $ENV{SystemDrive}/PostgreSQL/*/bin
  DOC "Path to pg_config utility")

if(PG_CONFIG)
  exec_program(${PG_CONFIG}
    ARGS "--version"
    OUTPUT_VARIABLE PG_CONFIG_VERSION)

  if(${PG_CONFIG_VERSION} MATCHES "^[A-Za-z]+[ ](.*)$")
    string(REGEX REPLACE "^[A-Za-z]+[ ](.*)$" "\\1" POSTGRESQL_VERSION "${PG_CONFIG_VERSION}")
  endif()

  exec_program(${PG_CONFIG}
    ARGS "--includedir"
    OUTPUT_VARIABLE PG_CONFIG_INCLUDEDIR)

  exec_program(${PG_CONFIG}
    ARGS "--libdir"
    OUTPUT_VARIABLE PG_CONFIG_LIBDIR)
else()
  set(POSTGRESQL_VERSION "unknown")
endif()

find_path(POSTGRESQL_INCLUDE_DIR libpq-fe.h
  PATHS
  ${PG_CONFIG_INCLUDEDIR}
  /usr/include
  /usr/local/include
  /usr/include/postgresql/*/
  $ENV{ProgramFiles}/PostgreSQL/*/include
  $ENV{SystemDrive}/PostgreSQL/*/include
  PATH_SUFFIXES
  server
  pgsql/server
  postgresql
  libpq
)

find_library(POSTGRESQL_LIBRARIES NAMES pq libpq
  PATHS
  ${PG_CONFIG_LIBDIR}
  /usr/lib
  /usr/local/lib
  /usr/lib/postgresql
  /usr/lib64
  /usr/local/lib64
  /usr/lib64/postgresql
  $ENV{ProgramFiles}/PostgreSQL/*/lib
  $ENV{SystemDrive}/PostgreSQL/*/lib
  $ENV{ProgramFiles}/PostgreSQL/*/lib/ms
  $ENV{SystemDrive}/PostgreSQL/*/lib/ms)

if(POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIBRARIES)
  set(POSTGRESQL_FOUND TRUE)
else()
  set(POSTGRESQL_FOUND FALSE)
endif()

set(POSTGRESQL_VERSION_STRING "${POSTGRESQL_VERSION}")

# Handle the QUIETLY and REQUIRED arguments and set POSTGRESQL_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PostgreSQL
                                  REQUIRED_VARS POSTGRESQL_LIBRARIES POSTGRESQL_INCLUDE_DIR
                                  VERSION_VAR POSTGRESQL_VERSION)

IF(POSTGRESQL_FOUND)
  set(POSTGRESQL_INCLUDE_DIRS ${POSTGRESQL_INCLUDE_DIR})
ENDIF()

mark_as_advanced(POSTGRESQL_INCLUDE_DIRS POSTGRESQL_LIBRARIES)
