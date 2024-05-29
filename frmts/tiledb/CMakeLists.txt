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
    target_compile_features(gdal_TileDB_core PRIVATE cxx_std_17)
endif()

if(NOT TARGET gdal_TileDB)
    return()
endif()

gdal_standard_includes(gdal_TileDB)
target_include_directories(gdal_TileDB PRIVATE
                           ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
gdal_target_link_libraries(gdal_TileDB PRIVATE TileDB::tiledb_shared)
