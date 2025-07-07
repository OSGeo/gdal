add_gdal_driver(
  TARGET ogr_S57
  SOURCES ogr_s57.h
          s57.h
          ogrs57driver.cpp
          ogrs57datasource.cpp
          ogrs57layer.cpp
          s57classregistrar.cpp
          s57reader.cpp
          ddfrecordindex.cpp
          s57featuredefns.cpp
          s57filecollector.cpp
          s57writer.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
target_include_directories(ogr_S57 PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
gdal_standard_includes(ogr_S57)

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
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
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_S57 OGR_ENABLE_DRIVER_S57_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_S57 PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()

add_executable(s57dump EXCLUDE_FROM_ALL s57dump.cpp)
gdal_standard_includes(s57dump)
target_include_directories(s57dump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
target_link_libraries(s57dump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_S57 gdal_iso8211)
