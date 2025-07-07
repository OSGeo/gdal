add_gdal_driver(TARGET ogr_GeoRSS SOURCES ogr_georss.h ogrgeorssdatasource.cpp ogrgeorssdriver.cpp ogrgeorsslayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_GeoRSS)
if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_GeoRSS PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_GeoRSS PRIVATE ${EXPAT_TARGET})
endif ()
