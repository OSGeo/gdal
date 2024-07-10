include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_TileDB)
    # The driver depends on some non expored headers. Make sure the version we
    # build against against is the same as this source tree.
    set(STRICT_VERSION_CHECK ON)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesTileDB)
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_TILEDB)
endif()

add_gdal_driver(TARGET gdal_TileDB SOURCES
                tiledbcommon.cpp
                tiledbdense.cpp
                tiledbsparse.cpp
                tiledbmultidim.cpp
                tiledbmultidimattribute.cpp
                tiledbmultidimattributeholder.cpp
                tiledbmultidimgroup.cpp
                tiledbmultidimarray.cpp
                CORE_SOURCES
                    tiledbdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_TileDB_core)
    target_include_directories(gdal_TileDB_core PRIVATE $<TARGET_PROPERTY:TileDB::tiledb_shared,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_TileDB_core PRIVATE $<TARGET_PROPERTY:TileDB::tiledb_shared,INTERFACE_COMPILE_DEFINITIONS>)
    target_compile_definitions(gdal_TileDB_core PRIVATE -DTILEDB_DEPRECATED=)
endif()

if(NOT TARGET gdal_TileDB)
    return()
endif()

gdal_standard_includes(gdal_TileDB)
if (STANDALONE)
target_include_directories(gdal_TileDB PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../gcore
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../mem
)
else()
target_include_directories(gdal_TileDB PRIVATE
                           ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
endif()
gdal_target_link_libraries(gdal_TileDB PRIVATE TileDB::tiledb_shared)
