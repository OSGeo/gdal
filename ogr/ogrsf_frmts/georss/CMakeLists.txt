add_gdal_driver(TARGET ogr_GeoRSS SOURCES ogr_georss.h ogrgeorssdatasource.cpp ogrgeorssdriver.cpp ogrgeorsslayer.cpp
                                          PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_GeoRSS)
if (HAVE_EXPAT)
  target_compile_definitions(ogr_GeoRSS PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(TARGET ogr_GeoRSS LIBRARIES EXPAT::EXPAT)
endif ()
