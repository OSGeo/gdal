# GeoPackage has raster and vector drivers in single module and referred from mbtiles driver
add_gdal_driver(
  TARGET ogr_GeoPackage
  DEF GPKG_ENALBED
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
if (HAVE_SQLITE3)
  gdal_target_link_libraries(TARGET ogr_GeoPackage PRIVATE LIBRARIES SQLite::SQLite3)
  if (SQLite_HAS_COLUMN_METADATA)
    target_compile_definitions(ogr_GeoPackage PRIVATE -DSQLITE_HAS_COLUMN_METADATA)
  endif ()
endif ()
if (HAVE_SPATIALITE)
  gdal_target_link_libraries(TARGET ogr_GeoPackage PRIVATE LIBRARIES SPATIALITE::SPATIALITE)
  target_compile_definitions(ogr_GeoPackage PRIVATE -DSPATIALITE_412_OR_LATER)
  if (SPATIALITE_AMALGAMATION)
    target_compile_definitions(ogr_GeoPackage PRIVATE -DSPATIALITE_AMALGAMATION)
  endif ()
endif ()
if (HAVE_RASTERLITE2)
  target_compile_definitions(ogr_GeoPackage PRIVATE -DHAVE_RASTERLITE2)
  gdal_target_link_libraries(TARGET ogr_GeoPackage LIBRARIES RASTERLITE2::RASTERLITE2)
endif ()
