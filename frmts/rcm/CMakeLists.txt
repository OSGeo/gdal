add_gdal_driver(TARGET gdal_RCM
                SOURCES rcmdataset.cpp rcmdataset.h
                CORE_SOURCES rcmdrivercore.cpp rcmdrivercore.h
                PLUGIN_CAPABLE
                NO_DEPS
                NO_SHARED_SYMBOL_WITH_CORE)
gdal_standard_includes(gdal_RCM)

if(NOT TARGET gdal_RCM)
    return()
endif()
