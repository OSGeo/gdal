add_gdal_driver(TARGET ogr_ODBC SOURCES ogrodbcdatasource.cpp ogrodbcdriver.cpp ogrodbclayer.cpp ogrodbcselectlayer.cpp
                                        ogrodbctablelayer.cpp PLUGIN_CAPABLE)
gdal_standard_includes(ogr_ODBC)
target_include_directories(ogr_ODBC PRIVATE ${ODBC_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_ODBC LIBRARIES ODBC::ODBC ODBC::ODBCINST)
