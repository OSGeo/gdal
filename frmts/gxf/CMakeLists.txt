add_gdal_driver(
  TARGET gdal_GXF
  SOURCES gxf_ogcwkt.c
          gxf_proj4.c
          gxfdataset.cpp
          gxfopen.c
          gxfopen.h
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_GXF)
