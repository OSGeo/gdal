add_gdal_driver(
  TARGET ogr_FileGDB
  DEF FGDB_ENABLED
  SOURCES FGdbUtils.h
          FGdbDatasource.cpp
          FGdbDriver.cpp
          FGdbLayer.cpp
          FGdbResultLayer.cpp
          FGdbUtils.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_FileGDB)
target_include_directories(ogr_FileGDB PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_OpenFileGDB,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_FileGDB LIBRARIES FILEGDB::FileGDB)
