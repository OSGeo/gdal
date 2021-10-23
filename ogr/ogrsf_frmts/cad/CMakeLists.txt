if (GDAL_USE_OPENCAD_INTERNAL)
  add_subdirectory(libopencad)
endif ()

add_gdal_driver(TARGET ogr_CAD SOURCES ogr_cad.h gdalcaddataset.cpp ogrcaddriver.cpp ogrcadlayer.cpp vsilfileio.cpp
                                       vsilfileio.h)
gdal_standard_includes(ogr_CAD)

if (GDAL_USE_OPENCAD_INTERNAL)
  target_include_directories(ogr_CAD PRIVATE $<TARGET_PROPERTY:libopencad,SOURCE_DIR>)
else ()
  target_include_directories(ogr_CAD PRIVATE ${OPENCAD_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET ogr_CAD LIBRARIES ${OPENCAD_LIBRARIES})
endif ()
