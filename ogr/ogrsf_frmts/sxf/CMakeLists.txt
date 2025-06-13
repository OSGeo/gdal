add_gdal_driver(TARGET ogr_SXF SOURCES ogr_sxf.h ogrsxfdatasource.cpp ogrsxflayer.cpp ogrsxfdriver.cpp PLUGIN_CAPABLE
  NO_DEPS
  NO_WFLAG_OLD_STYLE_CAST
)
target_include_directories(ogr_SXF PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_standard_includes(ogr_SXF)

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        LICENSE.TXT
        ${CMAKE_CURRENT_SOURCE_DIR}/data/default.rsc
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_SXF OGR_ENABLE_DRIVER_SXF_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_SXF PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()
