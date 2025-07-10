add_gdal_driver(TARGET ogr_NGW SOURCES ogrngwdriver.cpp ogrngwlayer.cpp ngw_api.cpp gdalngwdataset.cpp ogrngwfielddomain.cpp
                PLUGIN_CAPABLE
                NO_DEPS
                NO_CXX_WFLAGS_EFFCXX
)
gdal_standard_includes(ogr_NGW)
