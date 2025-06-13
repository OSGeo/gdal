add_gdal_driver(
  TARGET ogr_MiraMon
  SOURCES ogrmiramondatasource.cpp ogrmiramondriver.cpp ogrmiramonlayer.cpp mm_wrlayr.c mm_gdal_functions.c mm_rdlayr.c
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_MiraMon)
target_include_directories(ogr_MiraMon PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/MM_m_idofic.csv
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_MiraMon OGR_ENABLE_DRIVER_MIRAMON_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_MiraMon PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()
