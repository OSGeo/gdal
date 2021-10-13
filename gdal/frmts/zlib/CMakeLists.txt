# This is common library for other driver.
add_library(
  libz STATIC
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
set_property(TARGET libz PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libz PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE libz)
