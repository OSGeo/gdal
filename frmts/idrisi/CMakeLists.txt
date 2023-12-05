# RST -- idrist RASTER driver referred from ogr_idrisi
add_gdal_driver(
  TARGET gdal_IDRISI
  SOURCES IdrisiDataset.cpp idrisi.h
  BUILTIN) # because OGR IDRISI dependent on us
gdal_standard_includes(gdal_IDRISI)

gdal_target_link_libraries(gdal_IDRISI PRIVATE PROJ::proj)
