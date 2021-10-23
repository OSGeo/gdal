add_library(
  giflib STATIC
  dgif_lib.c
  egif_lib.c
  gifalloc.c
  gif_err.c
  gif_hash.c
  gif_hash.h
  gif_lib.h
  gif_lib_private.h)
set_property(TARGET giflib PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET giflib PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE giflib)
