# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindSQLite3
-----------
Find the SQLite libraries, v3
IMPORTED targets
^^^^^^^^^^^^^^^^
This module defines the following :prop_tgt:`IMPORTED` target:
``SQLite::SQLite3``
Result variables
^^^^^^^^^^^^^^^^
This module will set the following variables if found:
``SQLite3_INCLUDE_DIRS``
  where to find sqlite3.h, etc.
``SQLite3_LIBRARIES``
  the libraries to link against to use SQLite3.
``SQLite3_VERSION``
  version of the SQLite3 library found
``SQLite3_FOUND``
  TRUE if found

  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
  Copyright (c) 2016 NextGIS <info@nextgis.com>
  Copyright (c) 2018,2021 Hiroshi Miura
  Copyright (c) 2019 Chuck Atkins
#]=======================================================================]


if(SQLite3_INCLUDE_DIR AND SQLIite3_LIBRARY)
  set(SQLite3_FIND_QUIETLY TRUE)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_SQLITE3 QUIET sqlite3)
endif()

find_path(SQLite3_INCLUDE_DIR
          NAMES  sqlite3.h
          HINTS ${PC_SQLITE3_INCLUDE_DIRS})
find_library(SQLite3_LIBRARY
             NAMES sqlite3 sqlite3_i
             HINTS ${PC_SQLITE3_LIBRARY_DIRS})

# Extract version information from the header file
if(SQLite3_INCLUDE_DIR)
    file(STRINGS ${SQLite3_INCLUDE_DIR}/sqlite3.h _ver_line
         REGEX "^#define SQLITE_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           SQLite3_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)
    get_filename_component(SQLite3_LIBRARY_DIR ${SQLite3_LIBRARY} DIRECTORY)
    find_path(SQLite3_PCRE_LIBRARY
              NAMES pcre.${CMAKE_SHARED_LIBRARY_SUFFIX}
              SUFFIX_PATHS sqlite3
              PATHS /usr/lib
              HINTS ${SQLite3_LIBRARY_DIR})
    if(EXISTS ${SQLite3_PCRE_LIBRARY})
        set(SQLite_HAS_PCRE ON)
    else()
        set(SQLite_HAS_PCRE OFF)
    endif()
    # check column metadata
    set(SQLITE_COL_TEST_CODE "#ifdef __cplusplus
extern \"C\"
#endif
char sqlite3_column_table_name ();
int
main ()
{
return sqlite3_column_table_name ();
  return 0;
}
")
    check_c_source_compiles("${SQLITE_COL_TEST_CODE}"  SQLite_HAS_COLUMN_METADATA)
    set(SQLite_HAS_COLUMN_METADATA ${SQLite_HAS_COLUMN_METADATA})
endif()
mark_as_advanced(SQLite3_LIBRARY SQLite3_INCLUDE_DIR SQLite_HAS_PCRE SQLite_HAS_COLUMN_METADATA)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3
                                  FOUND_VAR SQLite3_FOUND
                                  REQUIRED_VARS SQLite3_LIBRARY SQLite3_INCLUDE_DIR
                                  VERSION_VAR SQLite3_VERSION)

if(SQLite3_FOUND)
  set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
  set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
  if(NOT TARGET SQLite::SQLite3)
    add_library(SQLite::SQLite3 UNKNOWN IMPORTED)
    set_target_properties(SQLite::SQLite3 PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                          IMPORTED_LOCATION "${SQLite3_LIBRARY}")
    if(SQLite_HAS_PCRE)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY
                     INTERFACE_COMPILE_DEFINITIONS "SQLite_HAS_PCRE")
    endif()
    if(SQLite_HAS_COLUMN_METADATA)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY
                     INTERFACE_COMPILE_DEFINITIONS "SQLite_HAS_COLUMN_METADATA")
    endif()
  endif()
endif()