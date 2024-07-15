# Distributed under the GDAL/OGR MIT License.  See accompanying file LICENSE.TXT.
# This file is included by drivers that want to be built as plugin against an
# installed GDAL library (and thus not requiring to build libgdal itself)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/modules/init.cmake")

# Hint used to alter the behavior of a number of .cmake files
set(STANDALONE ON)

# Detect installed GDAL
find_package(GDAL REQUIRED)
set(GDAL_VERSION_IMPORTED ${GDAL_VERSION})
set(GDAL_LIB_TARGET_NAME GDAL::GDAL)

# Check that we build the plugin against a GDAL version that matches the one
# of the sources
include(GdalVersion)
set(GDAL_VERSION_MAJOR_SOURCE ${GDAL_VERSION_MAJOR})
set(GDAL_VERSION_MINOR_SOURCE ${GDAL_VERSION_MINOR})
set(GDAL_VERSION_REV_SOURCE ${GDAL_VERSION_REV})
if(NOT "${GDAL_VERSION_IMPORTED}" MATCHES "${GDAL_VERSION_MAJOR_SOURCE}.${GDAL_VERSION_MINOR_SOURCE}.${GDAL_VERSION_REV_SOURCE}")
    if (STRICT_VERSION_CHECK)
        message(FATAL_ERROR "Building plugin against GDAL sources ${GDAL_VERSION_MAJOR_SOURCE}.${GDAL_VERSION_MINOR_SOURCE}.${GDAL_VERSION_REV_SOURCE} whereas linked GDAL library is at version ${GDAL_VERSION_IMPORTED}. This is not supported by this driver which expects strict version matching.")
    elseif (NOT IGNORE_GDAL_VERSION_MISMATCH)
        message(FATAL_ERROR "Building plugin against GDAL sources ${GDAL_VERSION_MAJOR_SOURCE}.${GDAL_VERSION_MINOR_SOURCE}.${GDAL_VERSION_REV_SOURCE} whereas linked GDAL library is at version ${GDAL_VERSION_IMPORTED}. This is not a nominally supported configuration. You can bypass this check by setting the IGNORE_GDAL_VERSION_MISMATCH variable.")
    endif()
endif()

include(GdalCAndCXXStandards)
include(GdalStandardIncludes)

include(CheckDependentLibrariesCommon)

include(GdalCompilationFlags)

set(GDAL_ENABLE_PLUGINS ON)
set(GDAL_BUILD_OPTIONAL_DRIVERS ON)
set(OGR_ENABLE_PLUGINS ON)
set(OGR_BUILD_OPTIONAL_DRIVERS ON)
include(GdalDriverHelper)

include(GNUInstallDirs)
# Used by GdalDriverHelper's add_gdal_driver()
set(INSTALL_PLUGIN_DIR
  "${CMAKE_INSTALL_LIBDIR}/gdalplugins"
  CACHE PATH "Installation sub-directory for plugins")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

macro(standalone_driver_finalize VAR)
    include(SystemSummary)
    include(driver_declaration.cmake)
    if (NOT ${VAR})
        message(FATAL_ERROR "${VAR} is not set, due to missing build requirements")
    endif()
    system_summary(DESCRIPTION "${PROJECT_NAME} is now configured on")
    feature_summary(DESCRIPTION "Enabled drivers and features and found dependency packages" WHAT ALL)
endmacro()
