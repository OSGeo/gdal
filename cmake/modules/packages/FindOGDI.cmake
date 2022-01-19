# FindOGDI
# ~~~~~~~~~
#
# Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# If it's found it sets OGDI_FOUND to TRUE
# and following variables are set:
#    OGDI_INCLUDE_DIRS
#    OGDI_LIBRARIES
#    OGDI_VERSION
#

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OGDI QUIET ogdi)
  set(OGDI_VERSION_STRING ${PC_OGDI_VERSION})
  set(OGDI_INCLUDE_DIRS ${PC_OGDI_INCLUDE_DIRS})
endif()

find_path(OGDI_INCLUDE_DIR ecs.h
          HINTS ${PC_OGDI_INCLUDE_DIRS}
          PATH_SUFFIXES ogdi)
mark_as_advanced(OGDI_INCLUDE_DIR)

find_library(OGDI_LIBRARY NAMES ogdi libogdi)
mark_as_advanced(OGDI_LIBRARY)

if(OGDI_INCLUDE_DIR AND OGDI_LIBRARY)
  find_program(OGDI_CONFIG_EXE ogdi-config)
  mark_as_advanced(OGDI_CONFIG_EXE)
  execute_process(COMMAND ${OGDI_CONFIG_EXE} --version
    OUTPUT_VARIABLE OGDI_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  execute_process(COMMAND ${OGDI_CONFIG_EXE} --cflags
    OUTPUT_VARIABLE _cflags OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  # Collect paths of include directories from CFLAGS
  separate_arguments(_cflags NATIVE_COMMAND "${_cflags}")
  foreach(arg IN LISTS _cflags)
    if("${arg}" MATCHES "^-I(.*)$")
      list(APPEND OGDI_INCLUDE_DIRS "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  unset(_cflags)
endif()

find_package_handle_standard_args(OGDI REQUIRED_VARS OGDI_LIBRARY OGDI_INCLUDE_DIRS
                                  VERSION_VAR OGDI_VERSION)

if(OGDI_FOUND)
    set(OGDI_LIBRARIES ${OGDI_LIBRARY})
    set(OGDI_INCLUDE_DIRS ${OGDI_INCLUDE_DIRS})
    if(NOT TARGET OGDI::OGDI)
        add_library(OGDI::OGDI UNKNOWN IMPORTED)
        set_target_properties(OGDI::OGDI PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${OGDI_INCLUDE_DIRS}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${OGDI_LIBRARY}")
    endif()
endif()
