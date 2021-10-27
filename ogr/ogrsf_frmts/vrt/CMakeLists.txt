# Depend from ogr_gpkg
add_gdal_driver(
  TARGET ogr_VRT
  SOURCES ogr_vrt.h ogrvrtdatasource.cpp ogrvrtdriver.cpp ogrvrtlayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_VRT)
target_include_directories(ogr_VRT PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
