add_gdal_driver(
  TARGET ogr_MiraMon
  SOURCES ogrmiramondatasource.cpp ogrmiramondriver.cpp ogrmiramonlayer.cpp mm_wrlayr.c mm_gdal_functions.c mm_rdlayr.c
  PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_MiraMon)
target_include_directories(ogr_MiraMon PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/MM_m_idofic.csv
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
