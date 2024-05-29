# depend from ogr_PDS
add_gdal_driver(
  TARGET gdal_PDS
  SOURCES isis2dataset.cpp
          isis3dataset.cpp
          pdsdataset.cpp
          pds4dataset.cpp
          pds4vector.cpp
          vicardataset.cpp
          vicarkeywordhandler.cpp
  CORE_SOURCES
          pdsdrivercore.cpp
  PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL\\\;NOT GDAL_USE_GEOTIFF_INTERNAL\\\;NOT GDAL_USE_TIFF_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/pds4_template.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/data/vicar.json
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if(NOT TARGET gdal_PDS)
    return()
endif()

gdal_standard_includes(gdal_PDS)
target_include_directories(
  gdal_PDS
  PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> ${GDAL_RASTER_FORMAT_SOURCE_DIR}/raw
          ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson
          ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/vrt ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff)

# When build as plugin, initialize all drivers from GDALRegister_PDS
if (GDAL_ENABLE_DRIVER_PDS_PLUGIN)
  target_compile_definitions(gdal_PDS PRIVATE -DPDS_PLUGIN)
endif ()

# Internal libs first
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(gdal_PDS libjson)
endif ()
if (GDAL_USE_GEOTIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_PDS geotiff)
endif ()
if (GDAL_USE_TIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_PDS libtiff)
endif ()

# External libs next
if (NOT GDAL_USE_JSONC_INTERNAL)
  gdal_target_link_libraries(gdal_PDS PRIVATE ${JSONC_TARGET})
endif ()
if (NOT GDAL_USE_GEOTIFF_INTERNAL)
  gdal_target_link_libraries(gdal_PDS PRIVATE ${GeoTIFF_TARGET})
endif ()
if (NOT GDAL_USE_TIFF_INTERNAL)
  gdal_target_link_libraries(gdal_PDS PRIVATE TIFF::TIFF)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  # ws2_32 needed for gethostname()
  gdal_target_link_libraries(gdal_PDS PRIVATE ws2_32)
endif ()
