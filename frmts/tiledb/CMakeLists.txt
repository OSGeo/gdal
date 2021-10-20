add_gdal_driver(TARGET gdal_TILEDB SOURCES tiledbdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_TILEDB)
target_include_directories(gdal_TILEDB PRIVATE $<TARGET_PROPERTY:TileDB::TileDB,INTERFACE_INCLUDE_DIRECTORIES>)
gdal_target_link_libraries(TARGET gdal_TILEDB LIBRARIES TileDB::TileDB)
