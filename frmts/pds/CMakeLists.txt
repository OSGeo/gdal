# depend from hdf4 and ogr_PDS
add_gdal_driver(
  TARGET gdal_PDS
  SOURCES isis2dataset.cpp
          isis3dataset.cpp
          pdsdataset.cpp
          pds4dataset.cpp
          pds4vector.cpp
          vicardataset.cpp
          vicarkeywordhandler.cpp
  BUILTIN)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE isis2dataset.cpp isis3dataset.cpp pdsdataset.cpp vicardataset.cpp vicarkeywordhandler.cpp
                      nasakeywordhandler.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
gdal_standard_includes(gdal_PDS)
target_include_directories(
  gdal_PDS
  PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> ${GDAL_RASTER_FORMAT_SOURCE_DIR}/raw
          ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson
          ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/vrt ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff)

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
  gdal_target_link_libraries(gdal_PDS PRIVATE ${JSONC_LIBRARIES})
endif ()
if (NOT GDAL_USE_GEOTIFF_INTERNAL)
  gdal_target_link_libraries(gdal_PDS PRIVATE ${GeoTIFF_LIBRARIES})
endif ()
if (NOT GDAL_USE_TIFF_INTERNAL)
  gdal_target_link_libraries(gdal_PDS PRIVATE TIFF::TIFF)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  # ws2_32 needed for gethostname()
  gdal_target_link_libraries(gdal_PDS PRIVATE ws2_32)
endif ()
