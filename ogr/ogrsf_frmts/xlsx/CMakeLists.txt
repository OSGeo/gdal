add_gdal_driver(TARGET ogr_XLSX SOURCES ogr_xlsx.h ogrxlsxdatasource.cpp ogrxlsxdriver.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_XLSX)
target_include_directories(
  ogr_XLSX PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>
                   $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_XLSX PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_XLSX PRIVATE ${EXPAT_LIBRARIES})
endif ()
