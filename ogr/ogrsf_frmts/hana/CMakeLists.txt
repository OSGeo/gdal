add_gdal_driver(
  TARGET ogr_HANA
  SOURCES ogrhanadatasource.cpp
          ogrhanadriver.cpp
          ogrhanafeaturereader.cpp
          ogrhanafeaturewriter.cpp
          ogrhanalayer.cpp
          ogrhanaresultlayer.cpp
          ogrhanatablelayer.cpp
          ogrhanautils.cpp
  CORE_SOURCES
          ogrhanadrivercore.cpp
  PLUGIN_CAPABLE)

if(NOT TARGET ogr_HANA)
    return()
endif()

gdal_standard_includes(ogr_HANA)
gdal_target_link_libraries(ogr_HANA PRIVATE ODBCCPP::ODBCCPP)
