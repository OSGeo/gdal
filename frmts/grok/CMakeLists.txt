include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_JP2Grok)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesGrok )
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_JP2GROK)
endif()

add_gdal_driver(TARGET gdal_JP2Grok
                SOURCES grokdataset.cpp
                CORE_SOURCES jp2grokdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)

if(TARGET gdal_JP2Grok_core)
    target_include_directories(gdal_JP2Grok_core PRIVATE $<TARGET_PROPERTY:GROK::grokj2k,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_JP2Grok_core PRIVATE $<TARGET_PROPERTY:GROK::grokj2k,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_JP2Grok)
    return()
endif()

gdal_standard_includes(gdal_JP2Grok)
target_include_directories(gdal_JP2Grok PRIVATE ../opjlike)
gdal_target_link_libraries(gdal_JP2Grok PRIVATE GROK::grokj2k)

if (GDAL_USE_CURL)
    target_compile_definitions(gdal_JP2Grok PRIVATE -DHAVE_CURL)
endif()