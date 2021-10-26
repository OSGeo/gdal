add_gdal_driver(TARGET gdal_TIL SOURCES tildataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_TIL)
target_include_directories(gdal_TIL PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE tildataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
