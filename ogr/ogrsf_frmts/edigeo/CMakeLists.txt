add_gdal_driver(TARGET ogr_EDIGEO SOURCES ogredigeodatasource.cpp ogredigeodriver.cpp ogr_edigeo.h ogredigeolayer.cpp
                                          PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_EDIGEO)
