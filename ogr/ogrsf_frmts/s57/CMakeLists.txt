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

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/s57agencies.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/s57attributes.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/s57expectedinput.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/s57objectclasses.csv
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

add_executable(s57dump EXCLUDE_FROM_ALL s57dump.cpp)
gdal_standard_includes(s57dump)
target_include_directories(s57dump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
target_link_libraries(s57dump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_S57 gdal_iso8211)
