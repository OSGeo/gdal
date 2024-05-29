add_gdal_driver(
  TARGET ogr_FileGDB
  DEF FGDB_ENABLED
  SOURCES FGdbUtils.h
          FGdbDatasource.cpp
          FGdbDriver.cpp
          FGdbLayer.cpp
          FGdbResultLayer.cpp
          FGdbUtils.cpp
  CORE_SOURCES
          FGdbDriverCore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_FileGDB)
    return()
endif()

gdal_standard_includes(ogr_FileGDB)
target_include_directories(ogr_FileGDB PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_OpenFileGDB,SOURCE_DIR>)
gdal_target_link_libraries(ogr_FileGDB PRIVATE FILEGDB::FileGDB)
