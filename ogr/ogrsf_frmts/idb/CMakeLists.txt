add_gdal_driver(
  TARGET ogr_IDB
  SOURCES ogr_idb.h
          ogridbdatasource.cpp
          ogridbdriver.cpp
          ogridblayer.cpp
          ogridbselectlayer.cpp
          ogridbtablelayer.cpp
  CORE_SOURCES
          ogridbdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_IDB)
    return()
endif()

gdal_standard_includes(ogr_IDB)
target_compile_definitions(ogr_IDB PRIVATE -DIT_DLLIB -DIT_DO_NOT_SIMULATE_BOOL)
gdal_target_link_libraries(ogr_IDB PRIVATE IDB::IDB)
