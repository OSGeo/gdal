add_gdal_driver(TARGET ogr_ODBC SOURCES ogrodbcdatasource.cpp ogrodbcdriver.cpp ogrodbclayer.cpp ogrodbcselectlayer.cpp
                                        ogrodbctablelayer.cpp PLUGIN_CAPABLE)
gdal_standard_includes(ogr_ODBC)
gdal_target_link_libraries(ogr_ODBC PRIVATE ODBC::ODBC)
