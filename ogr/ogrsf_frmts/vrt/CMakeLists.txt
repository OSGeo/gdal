# Depend from ogr_gpkg
add_gdal_driver(
  TARGET ogr_VRT
  SOURCES ogr_vrt.h ogrvrtdatasource.cpp ogrvrtdriver.cpp ogrvrtlayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_VRT)
target_include_directories(ogr_VRT PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ogrvrt.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
