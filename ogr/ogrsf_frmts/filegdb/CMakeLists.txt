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
  NO_SHARED_SYMBOL_WITH_CORE
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
if(NOT TARGET ogr_FileGDB)
    return()
endif()

gdal_standard_includes(ogr_FileGDB)
target_include_directories(ogr_FileGDB PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                                ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/openfilegdb)
gdal_target_link_libraries(ogr_FileGDB PRIVATE FILEGDB::FileGDB)
