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
