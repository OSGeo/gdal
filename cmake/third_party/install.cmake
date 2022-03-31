################################################################################
# Project:  CMake4GDAL
# Purpose:  CMake build scripts
# Author:   Dmitry Baryshnikov, polimax@mail.ru
################################################################################
# Copyright (C) 2015-2016, NextGIS <info@nextgis.com>
# Copyright (C) 2012-2014 Dmitry Baryshnikov
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
################################################################################

set(PACKAGE_NAME ${PROJECT_NAME})
string(TOUPPER ${PACKAGE_NAME} PACKAGE_UPPER_NAME)
#install lib and bin

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    install(TARGETS ${INSTALL_TARGETS}
        EXPORT ${PACKAGE_UPPER_NAME}Targets
        RUNTIME DESTINATION ${INSTALL_BIN_DIR} COMPONENT libraries
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
        LIBRARY DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
        INCLUDES DESTINATION ${INSTALL_SHORT_INC_DIR}
        FRAMEWORK DESTINATION ${INSTALL_LIB_DIR}
    )
endif()

if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${INSTALL_SHARE_DIR} COMPONENT libraries FILES_MATCHING PATTERN "*.*")
    if(UNIX AND NOT OSX_FRAMEWORK)
        install(FILES ${CMAKE_BINARY_DIR}/gdal.pc DESTINATION ${INSTALL_PKGCONFIG_DIR} COMPONENT libraries)
    endif()
endif()

if(NOT DEFINED PACKAGE_VENDOR)
    set(PACKAGE_VENDOR GDAL)
endif()

if(NOT DEFINED PACKAGE_NAME)
    set(PACKAGE_NAME GDAL)
endif()

if(NOT DEFINED PACKAGE_BUGREPORT)
    set(PACKAGE_BUGREPORT gdal-dev@lists.osgeo.org)
endif()

if(NOT DEFINED PACKAGE_URL)
    set(PACKAGE_URL "http://gdal.org/")
endif()

# if(NOT DEFINED PACKAGE_INSTALL_DIRECTORY)
#     set(PACKAGE_INSTALL_DIRECTORY ${PACKAGE_VENDOR})
# endif()
#
# set (CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
# set (CPACK_PACKAGE_VENDOR "${PACKAGE_VENDOR}")
# set (CPACK_PACKAGE_VERSION "${VERSION}")
# set (CPACK_PACKAGE_VERSION_MAJOR "${GDAL_MAJOR_VERSION}")
# set (CPACK_PACKAGE_VERSION_MINOR "${GDAL_MINOR_VERSION}")
# set (CPACK_PACKAGE_VERSION_PATCH "${GDAL_REV_VERSION}")
# set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE_NAME} Installation")
# set (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/README")
# set (CPACK_PACKAGE_INSTALL_DIRECTORY "${PACKAGE_INSTALL_DIRECTORY}")
# set (CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${PACKAGE_NAME}-${VERSION}-${LIB_TYPE}")
# set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/docs/LICENSE.TXT")
# set (CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/docs/NEWS")
# set (CPACK_PACKAGE_RELOCATABLE TRUE)
# #set (CPACK_PACKAGE_ICON ${CMAKE_SOURCE_DIR}/docs/data/gdalicon.png) # http://stackoverflow.com/a/28768495
# set (CPACK_ARCHIVE_COMPONENT_INSTALL ON)
#
# if (WIN32)
# #  set (CPACK_SET_DESTDIR FALSE)
# #  set (CPACK_PACKAGING_INSTALL_PREFIX "/opt")
#
#   set(scriptPath ${CMAKE_SOURCE_DIR}/cmake/EnvVarUpdate.nsh)
#   file(TO_NATIVE_PATH ${scriptPath} scriptPath )
#   string(REPLACE "\\" "\\\\" scriptPath  ${scriptPath} )
#   set (CPACK_NSIS_ADDITIONAL_INCLUDES "!include \\\"${scriptPath}\\\" \\n")
#
#   set (CPACK_GENERATOR "NSIS;ZIP")
#   set (CPACK_MONOLITHIC_INSTALL ON)
#   set (CPACK_NSIS_DISPLAY_NAME "${PACKAGE_NAME}")
#   set (CPACK_NSIS_COMPONENT_INSTALL ON)
#   set (CPACK_NSIS_CONTACT "${PACKAGE_BUGREPORT}")
#   set (CPACK_NSIS_MODIFY_PATH OFF)
#   set (CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME} ${VERSION}")
#   set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
#
#   string (REPLACE "/" "\\\\" NSIS_INSTALL_SHARE_DIR "${INSTALL_SHARE_DIR}")
#   string (REPLACE "/" "\\\\" NSIS_INSTALL_LIB_DIR "${INSTALL_LIB_DIR}")
#   string (REPLACE "/" "\\\\" NSIS_INSTALL_BIN_DIR "${INSTALL_BIN_DIR}")
#
#
#   set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS
#        "  Push 'GDAL_DATA'
#     Push 'A'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_SHARE_DIR}'
#     Call EnvVarUpdate
#     Pop  '$0' ")
#
#   set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
#        "  Push 'GDAL_DATA'
#     Push 'R'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_SHARE_DIR}'
#     Call un.EnvVarUpdate
#     Pop  '$0' ")
#
#
#   set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS
#        "  Push 'PATH'
#     Push 'A'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_BIN_DIR}'
#     Call EnvVarUpdate
#     Pop  '$0' ")
#
#   set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
#        "  Push 'PATH'
#     Push 'R'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_BIN_DIR}'
#     Call un.EnvVarUpdate
#     Pop  '$0' ")
#
#
#   # https://docs.python.org/3/install/
#   find_package(PythonInterp REQUIRED)
#   if(PYTHONINTERP_FOUND)
#     set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS ${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}
#          "  Push 'PYTHONPATH'
#     Push 'A'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_LIB_DIR}\\\\Python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}\\\\site-packages'
#     Call EnvVarUpdate
#     Pop  '$0' ")
#
#     set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS ${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}
#        "  Push 'PYTHONPATH'
#     Push 'R'
#     Push 'HKCU'
#     Push '$INSTDIR\\\\${NSIS_INSTALL_LIB_DIR}\\\\Python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}\\\\site-packages'
#     Call un.EnvVarUpdate
#     Pop  '$0' ")
#   endif()
#   string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}")
#   string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}")
#
# else ()
#   set (CPACK_PROJECT_CONFIG_FILE ${CMAKE_SOURCE_DIR}/cmake/CPackConfig.cmake)
#   set (CPACK_GENERATOR "DEB;RPM;TGZ;ZIP")
#
#   set (CPACK_COMPONENTS_ALL applications libraries headers documents)
#   set (CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE ON)
#
#   set (CPACK_DEBIAN_COMPONENT_INSTALL ON)
#   set (CPACK_DEBIAN_PACKAGE_SECTION "Libraries")
#   set (CPACK_DEBIAN_PACKAGE_MAINTAINER "${PACKAGE_BUGREPORT}")
#   set (CPACK_DEBIAN_PRE_INSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_DEBIAN_PRE_UNINSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_DEBIAN_POST_INSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_DEBIAN_POST_UNINSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_DEBIAN_PACKAGE_DEPENDS "zlib1g, libjpeg, libpng, libgeos, libcurl4-gnutls | libcurl-ssl, libexpat1, libproj, libxml2, liblzma, libarmadillo4 | libarmadillo5 | libarmadillo6, libtiff5, libgeotiff, libjson-c, libsqlite3, python2.7, python-numpy, libpcre3, libspatialite, libpq")
#
#   set (CPACK_RPM_COMPONENT_INSTALL ON)
#   set (CPACK_RPM_PACKAGE_GROUP "Development/Tools")
#   set (CPACK_RPM_PACKAGE_LICENSE "X/MIT")
#   set (CPACK_RPM_PACKAGE_URL "${PACKAGE_URL}")
#   set (CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_RPM_POST_INSTALL_SCRIPT_FILE "/sbin/ldconfig")
#   set (CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "/sbin/ldconfig")
# endif ()
#
# include(InstallRequiredSystemLibraries)
#
# include (CPack)

# Export package ===============================================================

# Add path to includes to build-tree export
target_include_directories(${LIB_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/port>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/ogr>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/ogr/ogrsf_frmts>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/gcore>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/core/gcore>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/alg>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/apps>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

# Add all targets to the build-tree export set
export(TARGETS ${LIB_NAME}
    FILE ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Targets.cmake)

if(REGISTER_PACKAGE)
    # Export the package for use from the build-tree
    # (this registers the build-tree with a global CMake-registry)
    export(PACKAGE ${PACKAGE_UPPER_NAME})
endif()

# Create the <Package>Config.cmake file
configure_file(cmake/PackageConfig.cmake.in
    ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Config.cmake @ONLY)

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    # Install the <Package>Config.cmake
    install(FILES
      ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Config.cmake
      DESTINATION ${INSTALL_CMAKECONF_DIR} COMPONENT dev)

    # Install the export set for use with the install-tree
    install(EXPORT ${PACKAGE_UPPER_NAME}Targets DESTINATION ${INSTALL_CMAKECONF_DIR} COMPONENT dev)
endif()

# Archiving ====================================================================

set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
set(CPACK_PACKAGE_VENDOR "${PACKAGE_VENDOR}")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE_NAME} Installation")
set(CPACK_PACKAGE_RELOCATABLE TRUE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_GENERATOR "ZIP")
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_STRIP_FILES TRUE)

# Get cpack zip archive name
get_cpack_filename(${VERSION} PROJECT_CPACK_FILENAME)
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_CPACK_FILENAME})

include(CPack)

#-----------------------------------------------------------------------------
# Now list the cpack commands
#-----------------------------------------------------------------------------
cpack_add_component (applications
    DISPLAY_NAME "${PACKAGE_NAME} utility programs"
    DEPENDS libraries
    GROUP Applications
)
cpack_add_component (libraries
    DISPLAY_NAME "${PACKAGE_NAME} libraries"
    GROUP Runtime
)
cpack_add_component (headers
    DISPLAY_NAME "${PACKAGE_NAME} headers"
    DEPENDS libraries
    GROUP Development
)
cpack_add_component (documents
    DISPLAY_NAME "${PACKAGE_NAME} documents"
    GROUP Documents
)
