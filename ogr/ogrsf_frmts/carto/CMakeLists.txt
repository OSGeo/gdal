add_gdal_driver(
  TARGET ogr_Carto
  SOURCES ogr_carto.h
          ogrcartodatasource.cpp
          ogrcartodriver.cpp
          ogrcartolayer.cpp
          ogrcartoresultlayer.cpp
          ogrcartotablelayer.cpp
  CORE_SOURCES
          ogrcartodrivercore.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JSONC_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_Carto)
    return()
endif()

gdal_standard_includes(ogr_Carto)
target_include_directories(ogr_Carto PRIVATE $<TARGET_PROPERTY:ogr_PGDump,SOURCE_DIR>
                                             $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_Carto libjson)
else ()
  gdal_target_link_libraries(ogr_Carto PRIVATE ${JSONC_TARGET})
endif ()
