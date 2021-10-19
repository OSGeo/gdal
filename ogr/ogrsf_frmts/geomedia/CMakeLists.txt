add_gdal_driver(
  TARGET ogr_Geomedia
  SOURCES ogr_geomedia.h ogrgeomediadatasource.cpp ogrgeomediadriver.cpp ogrgeomedialayer.cpp ogrgeomediaselectlayer.cpp
          ogrgeomediatablelayer.cpp
  BUILTIN) # because of PGEO dependency
gdal_standard_includes(ogr_Geomedia)
target_include_directories(ogr_Geomedia PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/pgeo ${ODBC_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_Geomedia LIBRARIES ODBC::ODBC)
