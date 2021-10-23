# RST -- idrist RASTER driver refered from ogr_idrisi
add_gdal_driver(
  TARGET gdal_IDRISI
  SOURCES IdrisiDataset.cpp idrisi.h
  BUILTIN)
gdal_standard_includes(gdal_IDRISI)

gdal_target_link_libraries(TARGET gdal_IDRISI LIBRARIES PROJ::PROJ)

if (GDAL_ENABLE_PLUGIN)
  target_compile_definitions(gdal_IDRISI PRIVATE -DGDAL_RST_PLUGIN)
endif ()

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE IdrisiDataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
