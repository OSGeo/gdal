add_gdal_driver(
  TARGET ogr_SDTS
  SOURCES ogr_sdts.h ogrsdtsdatasource.cpp ogrsdtsdriver.cpp ogrsdtslayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_SDTS)
target_include_directories(ogr_SDTS PRIVATE $<TARGET_PROPERTY:gdal_SDTS,SOURCE_DIR>
                                            $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
