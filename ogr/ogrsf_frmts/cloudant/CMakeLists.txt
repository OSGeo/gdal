add_gdal_driver(
  TARGET ogr_Cloudant
  SOURCES ogr_cloudant.h ogrcloudantdatasource.cpp ogrcloudantdriver.cpp ogrcloudanttablelayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_Cloudant)
target_include_directories(ogr_Cloudant PRIVATE ${CURL_INCLUDE_DIR} $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>
                                                $<TARGET_PROPERTY:ogr_CouchDB,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_Cloudant LIBRARIES CURL::libcurl)
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(ogr_Cloudant PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  target_include_directories(ogr_Cloudant PRIVATE ${JSONC_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET ogr_Cloudant LIBRARIES JSONC::JSONC)
endif ()
