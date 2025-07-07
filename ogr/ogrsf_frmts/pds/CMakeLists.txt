add_gdal_driver(
  TARGET ogr_PDS
  SOURCES ogrpdsdatasource.cpp ogr_pds.h ogrpdsdriver.cpp ogrpdslayer.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_PDS)
target_include_directories(ogr_PDS PRIVATE $<TARGET_PROPERTY:gdal_PDS,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_PDS libjson)
else ()
  gdal_target_link_libraries(ogr_PDS PRIVATE ${JSONC_TARGET})
endif ()
