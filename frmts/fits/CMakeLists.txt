add_gdal_driver(TARGET gdal_FITS
                SOURCES fitsdataset.cpp
                CORE_SOURCES fitsdrivercore.cpp
                PLUGIN_CAPABLE)

if(NOT TARGET gdal_FITS)
    return()
endif()

gdal_standard_includes(gdal_FITS)
gdal_target_link_libraries(gdal_FITS PRIVATE CFITSIO::CFITSIO)
