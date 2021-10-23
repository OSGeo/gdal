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
  BUILTIN)
gdal_standard_includes(ogr_AVC)

if (GDAL_USE_SHAPELIB_INTERNAL)
  target_include_directories(ogr_AVC PRIVATE $<TARGET_PROPERTY:ogr_Shape,SOURCE_DIR>)
  if (RENAME_INTERNAL_SHAPELIB_SYMBOLS)
    target_compile_definitions(ogr_AVC PRIVATE -DRENAME_INTERNAL_SHAPELIB_SYMBOLS -DSHPAPI_CALL=)
  endif ()
else ()
  gdal_target_link_libraries(TARGET ogr_AVC LIBRARIES SHAPELIB::shp)
endif ()
