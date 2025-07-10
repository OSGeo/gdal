# referred from DWG driver
add_gdal_driver(
  TARGET ogr_DXF
  SOURCES ogr_autocad_services.h
          ogr_dxf.h
          ogrdxf_polyline_smooth.h
          intronurbs.cpp
          ogrdxfblockswriterlayer.cpp
          ogrdxf_feature.cpp
          ogrdxf_polyline_smooth.cpp
          ogr_autocad_services.cpp
          ogrdxfdatasource.cpp
          ogrdxf_hatch.cpp
          ogrdxfreader.cpp
          ogrdxf_blockmap.cpp
          ogrdxf_dimension.cpp
          ogrdxflayer.cpp
          ogrdxfwriterds.cpp
          ogrdxfblockslayer.cpp
          ogrdxfdriver.cpp
          ogrdxf_leader.cpp
          ogrdxfwriterlayer.cpp
          ogrdxf_ocstransformer.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_DXF)

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/header.dxf
        ${CMAKE_CURRENT_SOURCE_DIR}/data/trailer.dxf
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_DXF OGR_ENABLE_DRIVER_DXF_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_DXF PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()
