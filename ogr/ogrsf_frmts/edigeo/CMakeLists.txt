add_gdal_driver(TARGET ogr_EDIGEO SOURCES ogredigeodatasource.cpp ogredigeodriver.cpp ogr_edigeo.h ogredigeolayer.cpp)
gdal_standard_includes(ogr_EDIGEO)
