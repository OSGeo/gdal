add_gdal_driver(TARGET gdal_TILEDB SOURCES tiledbdataset.cpp)
gdal_standard_includes(gdal_TILEDB)
gdal_target_link_libraries(TARGET gdal_TILEDB LIBRARIES TileDB::tiledb_shared)
