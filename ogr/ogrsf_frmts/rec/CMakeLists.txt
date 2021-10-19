add_gdal_driver(
  TARGET ogr_REC
  SOURCES ogr_rec.h
          ogrrecdatasource.cpp
          ogrrecdriver.cpp
          ogrreclayer.cpp
          ll_recio.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_REC)
