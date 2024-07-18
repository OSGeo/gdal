add_gdal_driver(TARGET ogr_OGDI
                SOURCES
                    ogrogdi.h
                    ogrogdidatasource.cpp
                    ogrogdidriver.cpp
                    ogrogdilayer.cpp
                CORE_SOURCES
                    ogrogdidrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_OGDI)
    return()
endif()

gdal_standard_includes(ogr_OGDI)
gdal_target_link_libraries(ogr_OGDI PRIVATE PROJ::proj OGDI::OGDI)
