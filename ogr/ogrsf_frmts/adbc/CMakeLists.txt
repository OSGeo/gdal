add_gdal_driver(
  TARGET ogr_ADBC
  SOURCES
      ogr_adbc.h
      ogradbcdriver.cpp
      ogradbcdataset.cpp
      ogradbclayer.cpp
      ogradbcbigquerylayer.cpp
  CORE_SOURCES ogradbcdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE
)

if(TARGET ogr_ADBC_core AND GDAL_USE_ADBCDRIVERMANAGER AND TARGET AdbcDriverManager::adbc_driver_manager_shared)
    target_compile_definitions(ogr_ADBC_core PRIVATE OGR_ADBC_HAS_DRIVER_MANAGER)
endif()

if(NOT TARGET ogr_ADBC)
    return()
endif()

gdal_standard_includes(ogr_ADBC)
target_include_directories(ogr_ADBC PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

if (GDAL_USE_ADBCDRIVERMANAGER AND TARGET AdbcDriverManager::adbc_driver_manager_shared)
  gdal_target_link_libraries(ogr_ADBC PRIVATE AdbcDriverManager::adbc_driver_manager_shared)
  target_compile_definitions(ogr_ADBC PRIVATE OGR_ADBC_HAS_DRIVER_MANAGER)
endif()
