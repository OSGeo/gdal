###############################################################################
# CMake module to search for ECW library - Enhanced Compression Wavelets for JPEG2000.
#
# Sets
#   ECW_FOUND.  If false, don't try to use ecw
#   ECW_INCLUDE_DIR
#   ECW_LIBRARY
#   ECW_VERSION
#
#   Imported target
#   ECW::libecwj2
#
# Author:   Alexander Lisovenko, alexander.lisovenko@gmail.com
# Author:   Dmitry Baryshnikov, bishop.dev@gmail.com, Hiroshi Miura
# Copyright (C) 2016, NextGIS <info@nextgis.com>
# Copyright (C) 2017,2018 Hiroshi Miura
################################################################################

find_path(ECW_INCLUDE_DIR NCSECWClient.h)

if (ECW_INCLUDE_DIR)
    set(MAJOR_VERSION 0)
    set(MINOR_VERSION 0)
    set(SRV_VERSION 0)
    set(BLD_VERSION 0)

    if (EXISTS "${ECW_INCLUDE_DIR}/ECWJP2BuildNumber.h")
        file(READ "${ECW_INCLUDE_DIR}/ECWJP2BuildNumber.h" VERSION_H_CONTENTS)
        string(REGEX MATCH "_VER_MAJOR[ \t]+([0-9]+)" MAJOR_VERSION ${VERSION_H_CONTENTS})
        string(REGEX MATCH "([0-9]+)" MAJOR_VERSION ${MAJOR_VERSION})
        string(REGEX MATCH "_VER_MINOR[ \t]+([0-9]+)" MINOR_VERSION ${VERSION_H_CONTENTS})
        string(REGEX MATCH "([0-9]+)" MINOR_VERSION ${MINOR_VERSION})
        string(REGEX MATCH "_VER_SERVICE[ \t]+([0-9]+)" SRV_VERSION ${VERSION_H_CONTENTS})
        string(REGEX MATCH "([0-9]+)" SRV_VERSION ${SRV_VERSION})
        string(REGEX MATCH "_VER_SERVICE[ \t]+([0-9]+)" BLD_VERSION ${VERSION_H_CONTENTS})
        string(REGEX MATCH "([0-9]+)" BLD_VERSION ${BLD_VERSION})
        unset(VERSION_H_CONTENTS)
    endif()

    if(EXISTS "${ECW_INCLUDE_DIR}/NCSBuildNumber.h")
        file(READ "${ECW_INCLUDE_DIR}/NCSBuildNumber.h" VERSION_H_CONTENTS)
        string(REGEX MATCH "NCS_VERSION_NUMBER[ \t]+([0-9,]+)" ECW_VERSION_NUMBER ${VERSION_H_CONTENTS})
        string(REGEX MATCH "([0-9]+),[0-9]+,[0-9]+,[0-9]+" MAJOR_VERSION ${ECW_VERSION_NUMBER})
        string(REGEX MATCH "[0-9]+,([0-9]+),[0-9]+,[0-9]+" MINOR_VERSION ${ECW_VERSION_NUMBER})
        string(REGEX MATCH "[0-9]+,[0-9]+,([0-9]+),[0-9]+" SRV_VERSION ${ECW_VERSION_NUMBER})
        string(REGEX MATCH "[0-9]+,[0-9]+,[0-9]+,([0-9]+)" BLD_VERSION ${ECW_VERSION_NUMBER})
        string(REGEX REPLACE "," "." ECW_VERSION_STRING ${ECW_VERSION_NUMBER})
        string(REGEX MATCH "NCS_ECWSDK_VERSION_NUMBER[ \t]+([0-9.]+)" ECWSDK_VERSION_NUMBER ${VERSION_H_CONTENTS})
        unset(VERSION_H_CONTENTS)
    endif()
endif()

find_library(ECW_LIBRARY NCSEcw)
find_library(ECWnet_LIBRARY NCSCnet)
find_library(ECWC_LIBRARY NCSEcwC)
find_library(NCSUtil_LIBRARY NCSUtil)
mark_as_advanced(ECW_INCLUDE_DIR ECW_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ECW
                                  REQUIRED_VARS ECW_LIBRARY ECWnet_LIBRARY ECWC_LIBRARY NCSUtil_LIBRARY ECW_INCLUDE_DIR
                                  VERSION_VAR ECW_VERSION_STRING)

if(ECW_FOUND)
    set(ECW_LIBRARIES ${ECW_LIBRARY} ${ECWnet_LIBRARY} ${ECWC_LIBRARY} ${NCSUtil_LIBRARY})
    set(ECW_INCLUDE_DIRS ${ECW_INCLUDE_DIR})
    if(NOT TARGET ECW::ECW)
        add_library(ECW::ECW UNKNOWN IMPORTED)
        set_target_properties(ECW::ECW PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${ECW_INCLUDE_DIRS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${ECW_LIBRARY}")
    endif()
    if(NOT TARGET ECW::ECWC)
        add_library(ECW::ECWC UNKNOWN IMPORTED)
        set_target_properties(ECW::ECWC PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${ECWC_LIBRARY}")
    endif()
    if(NOT TARGET ECW::ECWnet)
        add_library(ECW::ECWnet UNKNOWN IMPORTED)
        set_target_properties(ECW::ECWnet PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${ECWnet_LIBRARY}")
    endif()
    if(NOT TARGET ECW::NCSUtil)
        add_library(ECW::NCSUtil UNKNOWN IMPORTED)
        set_target_properties(ECW::NCSUtil PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${NCSUtil_LIBRARY}")
    endif()
endif()

