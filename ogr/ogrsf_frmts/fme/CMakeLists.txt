add_gdal_driver(
  TARGET ogr_FME
  SOURCES fme2ogr.h
          ogrfmecacheindex.cpp
          ogrfmedatasource.cpp
          ogrfmedriver.cpp
          ogrfmelayer.cpp
          ogrfmelayercached.cpp
          ogrfmelayerdb.cpp
          fme2ogr_utils.cpp)
gdal_standard_includes(ogr_FME)
target_include_directories(ogr_FME PRIVATE ${FME_INCLUDE_DIR} $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
target_compile_definitions(ogr_FME PRIVATE -DSUPPORT_INDIRECT_FMEDLL -DSUPPORT_PERSISTENT_CACHE)
gdal_target_link_libraries(ogr_FME PRIVATE ${FME_LIBRARY})
