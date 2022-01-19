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

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  add_executable(s57dump EXCLUDE_FROM_ALL s57dump.cpp)
  gdal_standard_includes(s57dump)
  target_include_directories(s57dump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
  target_link_libraries(s57dump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_S57 gdal_iso8211)
endif ()
