add_gdal_driver(
  TARGET gdal_PostGISRaster
  SOURCES postgisraster.h
          postgisrasterdataset.cpp
          postgisrasterdriver.cpp
          postgisrasterrasterband.cpp
          postgisrastertiledataset.cpp
          postgisrastertilerasterband.cpp
          postgisrastertools.cpp
          STRONG_CXX_WFLAGS
          PLUGIN_CAPABLE)
gdal_standard_includes(gdal_PostGISRaster)
target_include_directories(gdal_PostGISRaster PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt ${PostgreSQL_INCLUDE_DIRS})
target_compile_definitions(gdal_PostGISRaster PRIVATE -DHAVE_PG)
gdal_target_link_libraries(TARGET gdal_PostGISRaster LIBRARIES ${PostgreSQL_LIBRARIES})
