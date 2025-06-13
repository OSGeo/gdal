add_gdal_driver(
  TARGET gdal_GDALG
  PLUGIN_CAPABLE
  NO_DEPS
  SOURCES gdalgdriver.cpp
)

gdal_standard_includes(gdal_GDALG)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdalg.schema.json
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
