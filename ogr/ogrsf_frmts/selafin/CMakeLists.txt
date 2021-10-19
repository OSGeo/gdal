add_gdal_driver(
  TARGET ogr_Selafin
  SOURCES io_selafin.cpp
          io_selafin.h
          ogr_selafin.h
          ogrselafindatasource.cpp
          ogrselafindriver.cpp
          ogrselafinlayer.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_Selafin)
target_include_directories(ogr_Selafin PRIVATE $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
