if (GDAL_ENABLE_DRIVER_WCS)
  add_gdal_driver(
    TARGET gdal_WCS
    SOURCES gmlcoverage.cpp
            gmlcoverage.h
            wcsdataset.cpp
            wcsdataset.h
            wcsdataset100.cpp
            wcsdataset110.cpp
            wcsdataset201.cpp
            wcsrasterband.cpp
            wcsrasterband.h
            wcsutils.cpp
            PLUGIN_CAPABLE
            NO_DEPS)
  gdal_standard_includes(gdal_WCS)
  target_include_directories(gdal_WCS PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/gml)
endif ()

if (GDAL_ENABLE_DRIVER_HTTP)
  add_gdal_driver(TARGET gdal_HTTP SOURCES httpdriver.cpp PLUGIN_CAPABLE NO_DEPS)
  gdal_standard_includes(gdal_HTTP)
endif ()
