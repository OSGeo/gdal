add_gdal_driver(
  TARGET ogr_PG
  SOURCES ogrpgdatasource.cpp
          ogrpgdriver.cpp
          ogrpglayer.cpp
          ogrpgresultlayer.cpp
          ogrpgtablelayer.cpp
          ogrpgutility.cpp
  CORE_SOURCES
          ogrpgdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_PG)
    return()
endif()

gdal_standard_includes(ogr_PG)
target_include_directories(ogr_PG PRIVATE ${PostgreSQL_INCLUDE_DIRS} $<TARGET_PROPERTY:ogr_PGDump,SOURCE_DIR>)
gdal_target_link_libraries(ogr_PG PRIVATE PostgreSQL::PostgreSQL)

if (OGR_ENABLE_DRIVER_PG_PLUGIN)
    if(NOT MSVC AND CMAKE_THREAD_LIBS_INIT)
        gdal_target_link_libraries(ogr_PG PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    endif()
endif()
