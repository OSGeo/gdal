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
  BUILTIN)
gdal_standard_includes(ogr_DXF)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/header.dxf
    ${CMAKE_CURRENT_SOURCE_DIR}/data/trailer.dxf
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
