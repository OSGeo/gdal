add_gdal_driver(TARGET gdal_KMLSUPEROVERLAY SOURCES kmlsuperoverlaydataset.cpp kmlsuperoverlaydataset.h PLUGIN_CAPABLE
                                                    NO_DEPS)
gdal_standard_includes(gdal_KMLSUPEROVERLAY)
