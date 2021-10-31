if (GDAL_ENABLE_FRMT_WCS)
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

if (GDAL_ENABLE_FRMT_HTTP)
  # Technically we can build the HTTP driver as plugin, but it is desirable that it is built as built-in, to be in the
  # bottom of the drivers that are attempted. If it would be registered before the WMS driver for example, it would
  # prevent it from opening some URLs when not using the WMS prefix.
  add_gdal_driver(
    TARGET gdal_HTTP
    SOURCES httpdriver.cpp
    BUILTIN)
  gdal_standard_includes(gdal_HTTP)
endif ()
