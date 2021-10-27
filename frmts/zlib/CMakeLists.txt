# This is common library for other driver.
add_library(
  libz OBJECT
  crc32.h
  deflate.h
  inffast.h
  inffixed.h
  inflate.h
  inftrees.h
  trees.h
  zconf.h
  zlib.h
  zutil.h
  adler32.c
  compress.c
  crc32.c
  deflate.c
  gzio.c
  infback.c
  inffast.c
  inflate.c
  inftrees.c
  trees.c
  uncompr.c
  zutil.c)
set_property(TARGET libz PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libz>)
target_compile_options(libz PRIVATE ${GDAL_C_WARNING_FLAGS})
if (MSVC)
  target_compile_options(libz PRIVATE ${GDAL_SOFTWARNFLAGS} /wd4131)
endif ()
