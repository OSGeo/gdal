add_gdal_driver(
  TARGET gdal_FIT
  SOURCES fit.cpp
          fit.h
          fitdataset.cpp
          gstEndian.h
          gstTypes.h
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_FIT)
