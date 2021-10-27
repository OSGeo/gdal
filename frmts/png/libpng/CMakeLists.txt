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
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(libpng PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  target_include_directories(libpng PRIVATE ${ZLIB_INCLUDE_DIRS})
  gdal_add_private_link_libraries(${ZLIB_LIBRARIES})
endif ()
set_property(TARGET libpng PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libpng>)
