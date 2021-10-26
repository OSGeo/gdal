add_gdal_driver(TARGET gdal_ILWIS SOURCES ilwisdataset.h ilwisdataset.cpp ilwiscoordinatesystem.cpp PLUGIN_CAPABLE
                                          NO_DEPS)
gdal_standard_includes(gdal_ILWIS)
