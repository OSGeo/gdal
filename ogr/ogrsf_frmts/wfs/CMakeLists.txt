add_gdal_driver(
  TARGET ogr_WFS
  SOURCES ogr_wfs.h
          ogrwfsdatasource.cpp
          ogrwfsdriver.cpp
          ogrwfsjoinlayer.cpp
          ogrwfslayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)
gdal_standard_includes(ogr_WFS)
target_include_directories(ogr_WFS PRIVATE $<TARGET_PROPERTY:gmlutils,SOURCE_DIR>)
