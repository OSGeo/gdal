add_gdal_driver(
  TARGET gdal_GXF
  SOURCES gxf_ogcwkt.c
          gxf_proj4.c
          gxfdataset.cpp
          gxfopen.c
          gxfopen.h
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_GXF)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE gxfdataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
