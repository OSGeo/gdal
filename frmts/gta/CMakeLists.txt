add_gdal_driver(TARGET gdal_GTA
                SOURCES gta_headers.h gtadataset.cpp
                CORE_SOURCES gtadrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_GTA)
    return()
endif()

gdal_standard_includes(gdal_GTA)
target_include_directories(gdal_GTA PRIVATE ${GTA_INCLUDE_DIRS})
gdal_target_link_libraries(gdal_GTA PRIVATE ${GTA_LIBRARIES})
