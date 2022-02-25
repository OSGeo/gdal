add_library(
  giflib OBJECT
  dgif_lib.c
  egif_lib.c
  gifalloc.c
  gif_err.c
  gif_hash.c
  gif_hash.h
  gif_lib.h
  gif_lib_private.h)
set_property(TARGET giflib PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:giflib>)
target_compile_definitions(giflib PRIVATE -DHAVE_STDINT_H -DHAVE_INTTYPES_H)
# target_compile_options(giflib PRIVATE ${GDAL_C_WARNING_FLAGS} ${SOFTWARNFLAGS})
if (RENAME_INTERNAL_GIF_SYMBOLS)
  target_compile_definitions(giflib PUBLIC -DRENAME_INTERNAL_GIF_SYMBOLS)
endif ()
