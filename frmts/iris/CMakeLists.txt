add_gdal_driver(TARGET gdal_IRIS SOURCES irisdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_IRIS)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE irisdataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
