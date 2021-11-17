add_gdal_driver(
  TARGET ogr_PDS
  SOURCES ogrpdsdatasource.cpp ogr_pds.h ogrpdsdriver.cpp ogrpdslayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_PDS)
target_include_directories(ogr_PDS PRIVATE $<TARGET_PROPERTY:gdal_PDS,SOURCE_DIR> $<TARGET_PROPERTY:ogr_GML,SOURCE_DIR>)
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(ogr_PDS PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET ogr_PDS LIBRARIES JSONC::JSONC)
endif ()
