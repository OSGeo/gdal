add_library(
  geotiff OBJECT
  cpl_serv.h
  geo_free.c
  geo_new.c
  geo_set.c
  geo_tiffp.h
  geonames.h
  geovalues.h
  gdal_libgeotiff_symbol_rename.h
  geo_get.c
  geo_normalize.c
  geo_simpletags.c
  geo_trans.c
  geotiff.h
  xtiff.c
  geo_config.h
  geo_keyp.h
  geo_normalize.h
  geo_simpletags.h
  geo_write.c
  geotiff_proj4.c
  xtiffio.h
  geo_extra.c
  geo_names.c
  geo_print.c
  geo_tiffp.c
  geokeys.h
  geotiffio.h)
include(GdalStandardIncludes)
gdal_standard_includes(geotiff)
target_compile_options(geotiff PRIVATE ${GDAL_C_WARNING_FLAGS})

if (GDAL_USE_LIBTIFF_INTERNAL)
  target_include_directories(geotiff PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
  if (RENAME_INTERNAL_LIBTIFF_SYMBOLS)
    target_compile_definitions(geotiff PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()
else ()
  target_include_directories(geotiff PRIVATE ${TIFF_INCLUDE_DIRS})
  gdal_add_private_link_libraries(${TIFF_LIBRARIES})
endif ()
if (RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
  target_compile_definitions(geotiff PRIVATE -DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
  target_sources(geotiff PRIVATE gdal_libgeotiff_symbol_rename.h)
endif ()
set_property(TARGET geotiff PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

target_include_directories(geotiff PRIVATE ${PROJ_INCLUDE_DIR})
gdal_add_private_link_libraries(PROJ::PROJ)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:geotiff>)
