add_gdal_driver(
  TARGET gdal_INGR
  SOURCES IngrTypes.cpp
          IngrTypes.h
          IntergraphBand.cpp
          IntergraphBand.h
          IntergraphDataset.cpp
          IntergraphDataset.h
          JpegHelper.cpp
          JpegHelper.h
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_TIFF_INTERNAL"
          NO_DEPS)
gdal_standard_includes(gdal_INGR)
target_include_directories(gdal_INGR PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff)

if (GDAL_USE_TIFF_INTERNAL)
  target_include_directories(gdal_INGR PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
  if (RENAME_INTERNAL_TIFF_SYMBOLS)
    target_compile_definitions(gdal_INGR PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()
else ()
  gdal_target_link_libraries(gdal_INGR PRIVATE TIFF::TIFF)
endif ()
