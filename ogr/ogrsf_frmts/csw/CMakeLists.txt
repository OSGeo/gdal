add_gdal_driver(
  TARGET ogr_CSW
  SOURCES ogrcswdataset.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_CSW)
target_include_directories(ogr_CSW PRIVATE $<TARGET_PROPERTY:gmlutils,SOURCE_DIR>)
