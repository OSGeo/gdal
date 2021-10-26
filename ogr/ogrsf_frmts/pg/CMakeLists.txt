add_gdal_driver(
  TARGET ogr_PG
  SOURCES ogrpgdatasource.cpp
          ogrpgdriver.cpp
          ogrpglayer.cpp
          ogrpgresultlayer.cpp
          ogrpgtablelayer.cpp
          ogrpgutility.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_PG)
target_include_directories(ogr_PG PRIVATE ${PostgreSQL_INCLUDE_DIRS} $<TARGET_PROPERTY:ogr_PGDump,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_PG LIBRARIES PostgreSQL::PostgreSQL)
