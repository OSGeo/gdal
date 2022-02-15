add_gdal_driver(
  TARGET ogr_Cloudant
  SOURCES ogr_cloudant.h ogrcloudantdatasource.cpp ogrcloudantdriver.cpp ogrcloudanttablelayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_Cloudant)
target_include_directories(ogr_Cloudant PRIVATE ${CURL_INCLUDE_DIR} $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>
                                                $<TARGET_PROPERTY:ogr_CouchDB,SOURCE_DIR>)
gdal_target_link_libraries(ogr_Cloudant PRIVATE CURL::libcurl)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_Cloudant libjson)
else ()
  gdal_target_link_libraries(ogr_Cloudant PRIVATE ${JSONC_LIBRARIES})
endif ()
