add_gdal_driver(TARGET gdal_CALS SOURCES calsdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_CALS)

if (GDAL_USE_TIFF_INTERNAL)
  target_include_directories(gdal_CALS PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
else ()
  target_include_directories(gdal_CALS PRIVATE ${TIFF_INCLUDE_DIRS})
endif ()
