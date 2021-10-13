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

find_path(OGDI_INCLUDE_DIR ecs.h PATH_SUFFIXES ogdi)
find_library(OGDI_LIBRARY NAMES ogdi libogdi vpf libvpf)

if(OGDI_INCLUDE_DIR AND OGDI_LIBRARY)
    find_program(OGDI_CONFIG_EXE ogdi-config)
    execute_process(COMMAND ${OGDI_CONFIG_EXE} --version
            OUTPUT_VARIABLE OGDI_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

find_package_handle_standard_args(OGDI REQUIRED_VARS OGDI_LIBRARY OGDI_INCLUDE_DIR
                                  VERSION_VAR OGDI_VERSION)

if(OGDI_FOUND)
    set(OGDI_LIBRARIES ${OGDI_LIBRARY})
    set(OGDI_INCLUDE_DIRS ${OGDI_INCLUDE_DIR})
    if(NOT TARGET OGDI::OGDI)
        add_library(OGDI::OGDI UNKNOWN IMPORTED)
        set_target_properties(OGDI::OGDI PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${OGDI_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${OGDI_LIBRARY}")
    endif()
endif()