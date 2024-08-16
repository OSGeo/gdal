include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_JP2OpenJPEG)
    # The driver depends on some non exported headers. Make sure the version we
    # build against against is the same as this source tree.
    set(STRICT_VERSION_CHECK ON)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesOpenJPEG)
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_JP2OPENJPEG)
endif()

add_gdal_driver(
  TARGET gdal_JP2OpenJPEG
  SOURCES openjpegdataset.cpp
  CORE_SOURCES openjpegdrivercore.cpp
  DEF FRMT_openjpeg PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_JP2OpenJPEG_core)
    target_include_directories(gdal_JP2OpenJPEG_core PRIVATE $<TARGET_PROPERTY:${OpenJPEG_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_JP2OpenJPEG_core PRIVATE $<TARGET_PROPERTY:${OpenJPEG_TARGET},INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_JP2OpenJPEG)
    return()
endif()

gdal_standard_includes(gdal_JP2OpenJPEG)
if (STANDALONE)
target_include_directories(gdal_JP2OpenJPEG PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../port
                                                    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
endif()
target_include_directories(gdal_JP2OpenJPEG PRIVATE ../opjlike)
gdal_target_link_libraries(gdal_JP2OpenJPEG PRIVATE ${OpenJPEG_TARGET})
