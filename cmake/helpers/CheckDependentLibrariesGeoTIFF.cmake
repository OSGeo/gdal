gdal_check_package(GeoTIFF "libgeotiff library (external)" CAN_DISABLE RECOMMENDED
  NAMES GeoTIFF
  TARGETS geotiff_library GEOTIFF::GEOTIFF
)
gdal_internal_library(GEOTIFF)
