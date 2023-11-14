add_gdal_driver(TARGET gdal_TileDB SOURCES
                tiledbcommon.cpp
                tiledbdense.cpp
                tiledbsparse.cpp
                tiledbmultidim.cpp
                tiledbmultidimattribute.cpp
                tiledbmultidimattributeholder.cpp
                tiledbmultidimgroup.cpp
                tiledbmultidimarray.cpp
                PLUGIN_CAPABLE)
gdal_standard_includes(gdal_TileDB)
target_include_directories(gdal_TileDB PRIVATE
                           ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
target_compile_definitions(gdal_TileDB PRIVATE -DTILEDB_DEPRECATED=)
gdal_target_link_libraries(gdal_TileDB PRIVATE TileDB::tiledb_shared)
