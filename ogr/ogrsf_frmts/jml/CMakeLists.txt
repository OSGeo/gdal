add_gdal_driver(TARGET ogr_JML SOURCES ogr_jml.h ogrjmldataset.cpp ogrjmllayer.cpp ogrjmlwriterlayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_JML)
if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_JML PRIVATE -DHAVE_EXPAT=1)
  gdal_target_link_libraries(ogr_JML PRIVATE ${EXPAT_TARGET})
endif ()
