add_gdal_driver(TARGET ogr_XLS
                SOURCES ogr_xls.h ogrxlsdatasource.cpp ogrxlsdriver.cpp ogrxlslayer.cpp
                CORE_SOURCES ogrxlsdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_XLS)
    return()
endif()

gdal_standard_includes(ogr_XLS)
gdal_target_link_libraries(ogr_XLS PRIVATE FREEXL::freexl)
