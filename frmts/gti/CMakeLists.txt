add_gdal_driver(
  TARGET gdal_GTI
  PLUGIN_CAPABLE
  NO_DEPS
  SOURCES gdaltileindexdataset.cpp
)
gdal_standard_includes(gdal_GTI)
target_include_directories(gdal_GTI PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt
                                            $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdaltileindex.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if (GDAL_USE_GEOS)
  gdal_target_link_libraries(gdal_GTI PRIVATE ${GEOS_TARGET})
  target_compile_definitions(gdal_GTI PRIVATE -DHAVE_GEOS=1)
endif ()

if (GDAL_ENABLE_DRIVER_GTI_PLUGIN)
    target_compile_definitions(gdal_GTI PRIVATE -DBUILT_AS_PLUGIN)
endif()

if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
  target_compile_definitions(gdal_GTI PRIVATE -DUSE_NEON_OPTIMIZATIONS)
endif()

if (GDAL_ENABLE_ALGORITHMS)
  target_compile_definitions(gdal_GTI PRIVATE -DGDAL_ENABLE_ALGORITHMS)
endif()
