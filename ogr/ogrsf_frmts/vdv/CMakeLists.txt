add_gdal_driver(TARGET ogr_VDV SOURCES ogr_vdv.h ogrvdvdatasource.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_VDV)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/vdv452.xsd
)
if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    list(APPEND GDAL_DATA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/vdv452.xml
    )
endif()
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_VDV OGR_ENABLE_DRIVER_VDV_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_VDV PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()
