add_gdal_driver(
  TARGET ogr_OpenFileGDB
  SOURCES filegdbindex.cpp
          filegdbtable.cpp
          filegdbtable.h
          filegdbtable_priv.h
          ogr_openfilegdb.h
          ogropenfilegdbdatasource.cpp
          ogropenfilegdbdriver.cpp
          ogropenfilegdblayer.cpp
  BUILTIN) # should be  PLUGIN_CAPABLE NO_DEPS, but for a stupid reason ogrpgeodatasource.cpp has a dependency to us
           # that could be easily removed
gdal_standard_includes(ogr_OpenFileGDB)
target_include_directories(ogr_OpenFileGDB PRIVATE $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
