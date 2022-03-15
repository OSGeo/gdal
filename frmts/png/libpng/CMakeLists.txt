include(GdalDriverHelper)

add_library(
  libpng OBJECT
  png.c
  pngmem.c
  pngread.c
  pngrtran.c
  pngset.c
  pngwrite.c
  pngwutil.c
  pngerror.c
  pngget.c
  pngpread.c
  pngrio.c
  pngrutil.c
  pngtrans.c
  pngwio.c
  pngwtran.c)
if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(libpng libz)
else ()
  gdal_target_link_libraries(libpng PRIVATE ZLIB::ZLIB)
endif ()
set_property(TARGET libpng PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libpng>)
target_compile_definitions(libpng PRIVATE -DPNG_ARM_NEON_IMPLEMENTATION=0 -DPNG_ARM_NEON_OPT=0)
if (RENAME_INTERNAL_PNG_SYMBOLS)
  target_compile_definitions(libpng PUBLIC -DRENAME_INTERNAL_PNG_SYMBOLS)
endif ()
