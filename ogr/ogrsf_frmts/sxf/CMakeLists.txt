add_gdal_driver(TARGET ogr_SXF SOURCES ogr_sxf.h ogrsxfdatasource.cpp ogrsxflayer.cpp ogrsxfdriver.cpp PLUGIN_CAPABLE
                                       NO_DEPS)
target_include_directories(ogr_SXF PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_standard_includes(ogr_SXF)

set(GDAL_DATA_FILES
    LICENSE.TXT
    ${CMAKE_CURRENT_SOURCE_DIR}/data/default.rsc
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
