add_gdal_driver(
  TARGET ogr_WFS
  SOURCES ogr_wfs.h
          ogrwfsdatasource.cpp
          ogrwfsdriver.cpp
          ogrwfsjoinlayer.cpp
          ogrwfslayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_WFS)
target_include_directories(ogr_WFS PRIVATE $<TARGET_PROPERTY:gmlutils,SOURCE_DIR>)
