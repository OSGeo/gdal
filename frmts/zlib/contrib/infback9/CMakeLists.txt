add_library(
  infback9 OBJECT
  infback9.c
  inftree9.c
  minified_zutil.c)
set_property(TARGET infback9 PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:infback9>)
target_compile_options(infback9 PRIVATE ${GDAL_C_WARNING_FLAGS})
if (MSVC)
  target_compile_options(infback9 PRIVATE ${GDAL_SOFTWARNFLAGS} /wd4131)
endif ()

include(GdalDriverHelper)
if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(infback9 libz)
else()
  gdal_target_link_libraries(infback9 PRIVATE ZLIB::ZLIB)
endif ()
