add_gdal_driver(TARGET gdal_JPEG2000 SOURCES jpeg2000_vsil_io.cpp jpeg2000dataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JPEG2000)
target_include_directories(gdal_JPEG2000 PRIVATE ${JASPER_INCLUDE_DIR})
gdal_target_link_libraries(TARGET gdal_JPEG2000 LIBRARIES ${JASPER_LIBRARIES})

if (HAVE_JASPER_UUID)
  target_compile_definitions(gdal_JPEG2000 PRIVATE -DHAVE_JASPER_UUID)
endif ()
