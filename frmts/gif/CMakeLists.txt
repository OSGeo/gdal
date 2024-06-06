add_gdal_driver(
  TARGET gdal_GIF
  SOURCES gifabstractdataset.h gifabstractdataset.cpp biggifdataset.cpp gifdataset.cpp
  CORE_SOURCES gifdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_GIF)
    return()
endif()

gdal_standard_includes(gdal_GIF)

if (GDAL_USE_GIF_INTERNAL)
  option(RENAME_INTERNAL_GIF_SYMBOLS "Rename internal giflib symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_GIF_SYMBOLS)
  add_subdirectory(giflib)
endif ()

if (GDAL_ENABLE_DRIVER_GIF_PLUGIN)
  target_compile_definitions(gdal_GIF PRIVATE -DGIF_PLUGIN)
endif ()

if (GDAL_USE_GIF_INTERNAL)
  gdal_add_vendored_lib(gdal_GIF giflib)
else ()
  gdal_target_link_libraries(gdal_GIF PRIVATE GIF::GIF)
endif ()
