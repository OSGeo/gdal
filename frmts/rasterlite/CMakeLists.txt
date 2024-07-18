add_gdal_driver(TARGET gdal_Rasterlite
                SOURCES
                    rasterlitedataset.h
                    rasterlitecreatecopy.cpp
                    rasterlitedataset.cpp
                    rasterliteoverviews.cpp
                CORE_SOURCES
                    rasterlitedrivercore.cpp
                PLUGIN_CAPABLE NO_DEPS
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_Rasterlite)
    return()
endif()

gdal_standard_includes(gdal_Rasterlite)
target_include_directories(gdal_Rasterlite PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
