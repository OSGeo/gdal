add_gdal_driver(
  TARGET gdal_JP2Lura
  SOURCES jp2luracallbacks.h
          jp2luradataset.h
          jp2lurarasterband.h
          jp2luracallbacks.cpp
          jp2luradataset.cpp
          jp2lurarasterband.cpp
  CORE_SOURCES
          jp2luradrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_JP2Lura)
    return()
endif()

gdal_standard_includes(gdal_JP2Lura)
gdal_target_link_libraries(gdal_JP2Lura PRIVATE LURATECH::LURATECH)
