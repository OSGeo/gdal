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
