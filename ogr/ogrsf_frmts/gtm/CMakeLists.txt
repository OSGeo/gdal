add_gdal_driver(
  TARGET ogr_GTM
  SOURCES gtm.cpp
          gtm.h
          gtmtracklayer.cpp
          gtmwaypointlayer.cpp
          ogr_gtm.h
          ogrgtmdatasource.cpp
          ogrgtmdriver.cpp
          ogrgtmlayer.cpp)
gdal_standard_includes(ogr_GTM)
