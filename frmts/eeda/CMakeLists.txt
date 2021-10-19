add_gdal_driver(
  TARGET gdal_EEDA
  SOURCES eeda.h
          eedaidataset.cpp
          eedadataset.cpp
          eedacommon.cpp
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_LIBJSONC_INTERNAL"
          NO_DEPS)
gdal_standard_includes(gdal_EEDA)
target_include_directories(gdal_EEDA PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)

if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gdal_EEDA PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  target_include_directories(gdal_EEDA PRIVATE ${JSONC_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_EEDA LIBRARIES ${JSONC_LIBRARIES})
endif ()
