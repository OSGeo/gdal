# depends from frmts/aigrid, avc
add_gdal_driver(
  TARGET ogr_Shape
  SOURCES shape2ogr.cpp shp_vsi.c ogrshapedatasource.cpp ogrshapedriver.cpp ogrshapelayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_Shape)
target_include_directories(ogr_Shape PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

# shapelib
if (GDAL_USE_SHAPELIB_INTERNAL)
  # ogrshape.h shapefil.h dbfopen.c sbnsearch.c shpopen.c shptree.c
  add_library(shapelib OBJECT sbnsearch_wrapper.c shpopen_wrapper.c shptree_wrapper.c dbfopen_wrapper.c)
  gdal_standard_includes(shapelib)
  target_compile_definitions(shapelib PUBLIC -DSAOffset=vsi_l_offset)
  target_compile_options(shapelib PRIVATE ${GDAL_C_WARNING_FLAGS})
  if (RENAME_INTERNAL_SHAPELIB_SYMBOLS)
    target_compile_definitions(shapelib PUBLIC -DRENAME_INTERNAL_SHAPELIB_SYMBOLS -DSHPAPI_CALL=)
  endif ()
  target_compile_definitions(shapelib PUBLIC -DUSE_CPL)
  set_property(TARGET shapelib PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:shapelib>)
  gdal_add_vendored_lib(ogr_Shape shapelib)
elseif (HAVE_SHAPELIB)
  gdal_target_link_libraries(ogr_Shape PRIVATE SHAPELIB::shp)
endif ()
