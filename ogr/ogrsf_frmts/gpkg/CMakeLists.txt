# GeoPackage has raster and vector drivers in single module and referred from mbtiles driver
add_gdal_driver(
  TARGET ogr_GeoPackage
  DEF GPKG_ENABLED
  SOURCES ogr_geopackage.h
          gpkgmbtilescommon.h
          ogrgeopackageutility.h
          ogrgeopackagedatasource.cpp
          ogrgeopackagedriver.cpp
          ogrgeopackagelayer.cpp
          ogrgeopackageselectlayer.cpp
          ogrgeopackagetablelayer.cpp
          ogrgeopackageutility.cpp
          gdalgeopackagerasterband.cpp
  BUILTIN)
gdal_standard_includes(ogr_GeoPackage)
target_include_directories(
  ogr_GeoPackage PRIVATE $<TARGET_PROPERTY:alg,SOURCE_DIR> ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem
                         ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/sqlite)

gdal_target_link_libraries(TARGET ogr_GeoPackage PRIVATE LIBRARIES SQLite::SQLite3)
if (SQLite3_HAS_COLUMN_METADATA)
  target_compile_definitions(ogr_GeoPackage PRIVATE -DSQLITE_HAS_COLUMN_METADATA)
endif ()
