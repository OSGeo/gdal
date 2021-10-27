add_gdal_driver(TARGET ogr_WAsP SOURCES ogrwasp.h ogrwaspdatasource.cpp ogrwaspdriver.cpp ogrwasplayer.cpp
                                        PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_WAsP)
