add_gdal_driver(TARGET gdal_WMTS
                SOURCES wmtsdataset.cpp
                CORE_SOURCES wmtsdrivercore.cpp
                PLUGIN_CAPABLE NO_DEPS
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_WMTS)
    return()
endif()

gdal_standard_includes(gdal_WMTS)
