add_library(
  libjson OBJECT
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
# -D_XOPEN_SOURCE=700 to enable strdup() definition in C11 mode
target_compile_definitions(libjson PRIVATE -D_XOPEN_SOURCE=700)
target_include_directories(libjson PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)
set_target_properties(libjson PROPERTIES CXX_STANDARD 11)
set_property(TARGET libjson PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_compile_options(libjson PRIVATE ${GDAL_C_WARNING_FLAGS})
if (MSVC)
  target_compile_options(libjson PRIVATE ${GDAL_SOFTWARNFLAGS} /wd4306)
endif ()
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libjson>)
