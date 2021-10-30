add_gdal_driver(TARGET ogr_JML SOURCES ogr_jml.h ogrjmldataset.cpp ogrjmllayer.cpp ogrjmlwriterlayer.cpp PLUGIN_CAPABLE
                                       NO_DEPS)
gdal_standard_includes(ogr_JML)
target_include_directories(ogr_JML PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mem)
if (HAVE_EXPAT)
  target_compile_definitions(ogr_JML PRIVATE -DHAVE_EXPAT=1)
  gdal_target_link_libraries(TARGET ogr_JML LIBRARIES EXPAT::EXPAT)
endif ()
