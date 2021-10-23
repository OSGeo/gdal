add_gdal_driver(TARGET ogr_NGW SOURCES ogrngwdriver.cpp ogrngwlayer.cpp ngw_api.cpp gdalngwdataset.cpp)
gdal_standard_includes(ogr_NGW)
