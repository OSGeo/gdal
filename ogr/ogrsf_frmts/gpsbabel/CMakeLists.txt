add_gdal_driver(TARGET ogr_GPSBabel SOURCES ogr_gpsbabel.h ogrgpsbabeldatasource.cpp ogrgpsbabeldriver.cpp
                                            ogrgpsbabelwritedatasource.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_GPSBabel)
