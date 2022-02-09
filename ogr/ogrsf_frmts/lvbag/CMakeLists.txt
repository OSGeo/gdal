add_gdal_driver(TARGET ogr_LVBAG SOURCES ogrlvbagdatasource.cpp ogrlvbagdriver.cpp ogrlvbaglayer.cpp PLUGIN_CAPABLE
                                         NO_DEPS)
gdal_standard_includes(ogr_LVBAG)
target_include_directories(ogr_LVBAG PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_LVBAG PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_LVBAG PRIVATE ${EXPAT_LIBRARIES})
endif ()

if (GDAL_USE_GEOS)
  target_compile_definitions(ogr_LVBAG PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(ogr_LVBAG PRIVATE ${GEOS_LIBRARIES})
endif ()
