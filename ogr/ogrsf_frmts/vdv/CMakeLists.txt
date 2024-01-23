add_gdal_driver(TARGET ogr_VDV SOURCES ogr_vdv.h ogrvdvdatasource.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_VDV)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/vdv452.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/data/vdv452.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
