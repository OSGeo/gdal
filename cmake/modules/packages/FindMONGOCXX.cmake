# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindMONGOCXX
# -----------
#
# Find the MONGOCXX library
#
# Once done this will define
#
#  MONGOCXX_FOUND - System has MONGOCXX
#  MONGOCXX_INCLUDE_DIR - The MONGOCXX include directory
#  MONGOCXX_LIBRARIES - The libraries needed to use MONGOCXX

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_MONGOCXX QUIET libmongocxx)
    set(MONGOCXX_VERSION_STRING ${PC_MONGOCXX_VERSION})
endif()

find_path(MONGOCXX_INCLUDE_DIR mongocxx/client.hpp
          HINTS ${PC_MONGOCXX_INCLUDE_DIRS}
          PATH_SUFFIXES mongocxx/v_noabi)
find_path(BSONCXX_INCLUDE_DIR bsoncxx/config/version.hpp
          HINTS ${PC_MONGOCXX_INCLUDE_DIRS}
          PATH_SUFFIXES bsoncxx/v_noabi)
find_library(MONGOCXX_LIBRARY NAMES mongocxx
             HINTS ${PC_MONGOCXX_LIBRARY_DIRS})
find_library(BSONCXX_LIBRARY NAMES bsoncxx
             HINTS ${PC_MONGOCXX_LIBRARY_DIRS})
mark_as_advanced(MONGOCXX_INCLUDE_DIR BSONCXX_INCLUDE_DIR MONGOCXX_LIBRARY BSONCXX_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MONGOCXX
                                  REQUIRED_VARS MONGOCXX_INCLUDE_DIR BSONCXX_INCLUDE_DIR MONGOCXX_LIBRARY BSONCXX_LIBRARY
                                  VERSION_VAR MONGOCXX_VERSION_STRING)

if(MONGOCXX_FOUND)
    set(MONGOCXX_LIBRARIES ${MONGOCXX_LIBRARY} ${BSONCXX_LIBRARY})
    set(MONGOCXX_INCLUDE_DIRS ${MONGOCXX_INCLUDE_DIR} ${BSONCXX_INCLUDE_DIR})
    if(NOT TARGET MONGOCXX::MONGOCXX)
        add_library(MONGOCXX::MONGOCXX UNKNOWN IMPORTED)
        set_target_properties(MONGOCXX::MONGOCXX PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${MONGOCXX_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${MONGOCXX_LIBRARY})
    endif()
    if(NOT TARGET MONGOCXX::BSONCXX)
        add_library(MONGOCXX::BSONCXX UNKNOWN IMPORTED)
        set_target_properties(MONGOCXX::BSONCXX PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${BSONCXX_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${BSONCXX_LIBRARY})
    endif()
endif()
