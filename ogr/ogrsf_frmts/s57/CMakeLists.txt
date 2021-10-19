add_gdal_driver(
  TARGET ogr_S57
  SOURCES ogr_s57.h
          s57.h
          s57tables.h
          ogrs57driver.cpp
          ogrs57datasource.cpp
          ogrs57layer.cpp
          s57classregistrar.cpp
          s57reader.cpp
          ddfrecordindex.cpp
          s57featuredefns.cpp
          s57filecollector.cpp
          s57writer.cpp
  BUILTIN)
target_include_directories(ogr_S57 PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
gdal_standard_includes(ogr_S57)
add_executable(s57dump EXCLUDE_FROM_ALL s57dump.cpp)
