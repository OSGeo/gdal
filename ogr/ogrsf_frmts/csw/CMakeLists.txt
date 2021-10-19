add_gdal_driver(
  TARGET ogr_CSW
  SOURCES ogrcswdataset.cpp
  BUILTIN) # BUILTIN because of GML and WFS dependency
gdal_standard_includes(ogr_CSW)
target_include_directories(ogr_CSW PRIVATE $<TARGET_PROPERTY:ogr_GML,SOURCE_DIR> $<TARGET_PROPERTY:ogr_WFS,SOURCE_DIR>)
