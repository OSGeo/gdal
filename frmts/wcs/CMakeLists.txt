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
    CORE_SOURCES
            wcsdrivercore.cpp
    PLUGIN_CAPABLE NO_DEPS
    NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_WCS)
    return()
endif()

gdal_standard_includes(gdal_WCS)
target_include_directories(gdal_WCS PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/gml)
