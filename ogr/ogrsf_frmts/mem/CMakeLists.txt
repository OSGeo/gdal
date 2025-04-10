add_gdal_driver(
  TARGET ogr_MEM
  SOURCES ogrmemdriver.cpp
  BUILTIN)
gdal_standard_includes(ogr_MEM)
