set(_SOURCES
    ogr_db2.h
    gdaldb2rasterband.cpp
    ogrdb2cli.cpp
    ogrdb2datasource.cpp
    ogrdb2datasourcemd.cpp
    ogrdb2driver.cpp
    # ogrdb2geometryvalidator.cpp
    ogrdb2layer.cpp
    ogrdb2selectlayer.cpp
    ogrdb2tablelayer.cpp)

add_gdal_driver(
  TARGET ogr_DB2
  SOURCES ${_SOURCES}
  BUILTIN)

gdal_standard_includes(ogr_DB2)
target_include_directories(ogr_DB2 PRIVATE $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_DB2 LIBRARIES ODBC::ODBC)
