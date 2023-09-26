add_gdal_driver(
  TARGET ogr_MiraMon
  SOURCES ogrmiramondatasource.cpp ogrmiramondriver.cpp ogrmiramonlayer.cpp mm_wrlayr.c mm_gdal_functions.c
  BUILTIN)
gdal_standard_includes(ogr_MiraMon)
target_include_directories(ogr_MiraMon PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
