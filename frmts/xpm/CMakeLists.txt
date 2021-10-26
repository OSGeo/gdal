add_gdal_driver(TARGET gdal_XPM SOURCES xpmdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_XPM)
target_include_directories(gdal_XPM PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE xpmdataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
