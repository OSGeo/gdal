option(OGR_ENABLE_FILEGDB_PLUGIN "Enable to build FileGDB driver as plugin" ON)
set(_SOURCES FGdbUtils.h FGdbDatasource.cpp FGdbDriver.cpp FGdbLayer.cpp FGdbResultLayer.cpp FGdbUtils.cpp)
if (OGR_ENABLE_FILEGDB_PLUGIN)
  add_gdal_driver(
    TARGET ogr_FileGDB
    SOURCES ${_SOURCES}
    PLUGIN)
else ()
  add_gdal_driver(TARGET ogr_FileGDB SOURCES ${_SOURCES})
endif ()
gdal_standard_includes(ogr_FileGDB)
target_include_directories(ogr_FileGDB PRIVATE ${FILEGDB_INCLUDE_DIR} $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_FileGDB LIBRARIES ${FILEGDB_LIBRARY})
