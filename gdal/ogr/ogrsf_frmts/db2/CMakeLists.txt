option(OGR_ENABLE_DB2_PLUGIN "ON to enable DB2 driver build as plugin" ON)
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
if (OGR_ENABLE_DB2_PLUGIN)
  add_gdal_driver(
    TARGET ogr_DB2
    SOURCES ${_SOURCES}
    PLUGIN)
else ()
  add_gdal_driver(TARGET ogr_DB2 SOURCES ${_SOURCES})
endif ()
gdal_standard_includes(ogr_DB2)
target_include_directories(ogr_DB2 PRIVATE $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>
                                           ${ODBC_INCLUDE_DIR})
gdal_target_link_libraries(TARGET ogr_DB2 LIBRARIES ODBC::ODBC)
if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(libtiff PRIVATE -D_WIN64)
  endif ()
endif ()
