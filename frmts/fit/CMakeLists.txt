add_gdal_driver(
  TARGET gdal_FIT
  SOURCES fit.cpp
          fit.h
          fitdataset.cpp
          gstEndian.h
          gstTypes.h
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_FIT)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE fit.cpp fitdataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
