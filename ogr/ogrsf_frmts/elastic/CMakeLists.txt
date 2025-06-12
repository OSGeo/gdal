add_gdal_driver(
  TARGET ogr_Elastic
  SOURCES ogr_elastic.h
          ogrelasticdatasource.cpp
          ogrelasticdriver.cpp
          ogrelasticlayer.cpp
          ogrelasticaggregationlayer.cpp
  CORE_SOURCES
          ogrelasticdrivercore.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JSONC_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
if(NOT TARGET ogr_Elastic)
    return()
endif()

gdal_standard_includes(ogr_Elastic)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_Elastic libjson)
else ()
  gdal_target_link_libraries(ogr_Elastic PRIVATE ${JSONC_TARGET})
endif ()
