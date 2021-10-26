add_gdal_driver(
  TARGET ogr_MEM
  SOURCES ogrmemdatasource.cpp ogr_mem.h ogrmemdriver.cpp ogrmemlayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_MEM)
