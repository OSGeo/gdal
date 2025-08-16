# depend from frmts/aigrid
add_gdal_driver(
  TARGET ogr_AVC
  DEF AVC_ENABLED
  SOURCES avc_bin.cpp
          avc_rawbin.cpp
          avc_e00gen.cpp
          avc_e00parse.cpp
          avc_e00read.cpp
          avc_mbyte.cpp
          avc_misc.cpp
          ogravcbindatasource.cpp
          ogravcbindriver.cpp
          ogravcbinlayer.cpp
          ogravce00datasource.cpp
          ogravcdatasource.cpp
          ogravce00driver.cpp
          ogravce00layer.cpp
          ogravclayer.cpp
          # avc_binwr.cpp avc_e00write.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_AVC)

if (NOT OGR_ENABLE_DRIVER_SHAPE OR OGR_ENABLE_DRIVER_SHAPE_PLUGIN)
    target_compile_definitions(ogr_AVC PRIVATE -DWITHOUT_SHAPEFILE)
    target_include_directories(ogr_AVC PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shape)
    target_include_directories(ogr_AVC PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shape/shapelib)
else()
    if (GDAL_USE_SHAPELIB_INTERNAL)
      gdal_add_vendored_lib(ogr_AVC shapelib)
      target_include_directories(ogr_AVC PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shape/shapelib)
    else ()
      gdal_target_link_libraries(ogr_AVC PRIVATE SHAPELIB::shp)
    endif ()
endif()
