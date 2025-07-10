add_gdal_driver(
  TARGET gdal_SNAP_TIFF
  SOURCES snaptiffdriver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)

gdal_standard_includes(gdal_SNAP_TIFF)

target_include_directories(gdal_SNAP_TIFF PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/libertiff
)
