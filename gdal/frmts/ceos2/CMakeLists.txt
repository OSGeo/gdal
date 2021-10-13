add_gdal_driver(
  TARGET gdal_SAR_CEOS
  DEF FRMT_ceos2
  SOURCES ceos.c ceos.h ceosrecipe.c ceossar.c link.c sar_ceosdataset.cpp)
gdal_standard_includes(gdal_SAR_CEOS)
target_include_directories(gdal_SAR_CEOS PRIVATE $<TARGET_PROPERTY:gdal_raw,SOURCE_DIR>)
