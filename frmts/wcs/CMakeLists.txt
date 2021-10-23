add_gdal_driver(
  TARGET gdal_WCS
  SOURCES gmlcoverage.cpp
          gmlcoverage.h
          httpdriver.cpp
          wcsdataset.cpp
          wcsdataset.h
          wcsdataset100.cpp
          wcsdataset110.cpp
          wcsdataset201.cpp
          wcsrasterband.cpp
          wcsrasterband.h
          wcsutils.cpp)
gdal_standard_includes(gdal_WCS)
target_include_directories(gdal_WCS PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/gml)
