add_gdal_driver(TARGET ogr_PLSCENES SOURCES ogr_plscenes.h ogrplscenesdatav1dataset.cpp ogrplscenesdatav1layer.cpp
                                            ogrplscenesdriver.cpp PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL")
gdal_standard_includes(ogr_PLSCENES)
target_include_directories(ogr_PLSCENES PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_PLSCENES libjson)
else ()
  gdal_target_link_libraries(ogr_PLSCENES PRIVATE ${JSONC_LIBRARIES})
endif ()
