# RST -- idrist RASTER driver referred from ogr_idrisi
add_gdal_driver(
  TARGET gdal_IDRISI
  SOURCES IdrisiDataset.cpp idrisi.h
  BUILTIN  # because OGR IDRISI dependent on us
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_IDRISI)

gdal_target_link_libraries(gdal_IDRISI PRIVATE PROJ::proj)
