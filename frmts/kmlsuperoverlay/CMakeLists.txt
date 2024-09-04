add_gdal_driver(TARGET gdal_KMLSUPEROVERLAY
                SOURCES kmlsuperoverlaydataset.cpp
                        kmlsuperoverlaydataset.h
                PLUGIN_CAPABLE
                NO_DEPS
                STRONG_CXX_WFLAGS)
gdal_standard_includes(gdal_KMLSUPEROVERLAY)
