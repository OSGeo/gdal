add_gdal_driver(TARGET gdal_ERS SOURCES ershdrnode.h ersdataset.cpp ershdrnode.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
target_include_directories(gdal_ERS PRIVATE $<TARGET_PROPERTY:gdal_raw,SOURCE_DIR>)
gdal_standard_includes(gdal_ERS)
