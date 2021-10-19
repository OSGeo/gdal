add_gdal_driver(TARGET ogr_NGW SOURCES ogrngwdriver.cpp ogrngwlayer.cpp ngw_api.cpp gdalngwdataset.cpp PLUGIN_CAPABLE
                                       NO_DEPS)
gdal_standard_includes(ogr_NGW)
