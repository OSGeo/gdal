add_gdal_driver(
  TARGET gdal_JP2Lura
  SOURCES jp2luracallbacks.h
          jp2luradataset.h
          jp2lurarasterband.h
          jp2luracallbacks.cpp
          jp2luradataset.cpp
          jp2lurarasterband.cpp
          PLUGIN_CAPABLE)

gdal_standard_includes(gdal_JP2Lura)
gdal_target_link_libraries(TARGET gdal_JP2Lura LIBRARIES LURATECH::LURATECH)
