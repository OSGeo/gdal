add_gdal_driver(
  TARGET ogr_Carto
  SOURCES ogr_carto.h
          ogrcartodatasource.cpp
          ogrcartodriver.cpp
          ogrcartolayer.cpp
          ogrcartoresultlayer.cpp
          ogrcartotablelayer.cpp
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_LIBJSONC_INTERNAL")
gdal_standard_includes(ogr_Carto)
target_include_directories(ogr_Carto PRIVATE $<TARGET_PROPERTY:ogr_PGDump,SOURCE_DIR>
                                             $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(ogr_Carto PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET ogr_Carto LIBRARIES JSONC::JSONC)
endif ()
