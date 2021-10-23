add_library(
  libjson STATIC
  arraylist.h
  bits.h
  config.h
  debug.h
  json.h
  json_c_version.h
  json_config.h
  json_inttypes.h
  json_object.h
  json_object_private.h
  json_tokener.h
  json_util.h
  linkhash.h
  json_object_iterator.h
  printbuf.h
  symbol_renames.h
  arraylist.c
  debug.c
  json_c_version.c
  json_object.c
  json_object_iterator.c
  json_tokener.c
  json_util.c
  linkhash.c
  printbuf.c)
# -D_XOPEN_SOURCE=500 to enable strdup() definition in C11 mode
target_compile_definitions(libjson PRIVATE -D_XOPEN_SOURCE=500)
target_include_directories(libjson PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)
set_target_properties(libjson PROPERTIES CXX_STANDARD 11)
set_property(TARGET libjson PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libjson PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE libjson)
