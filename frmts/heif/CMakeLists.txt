add_gdal_driver(TARGET gdal_HEIF
                SOURCES heifdataset.cpp
                CORE_SOURCES heifdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_HEIF_core)
    target_include_directories(gdal_HEIF_core PRIVATE $<TARGET_PROPERTY:HEIF::HEIF,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_HEIF_core PRIVATE $<TARGET_PROPERTY:HEIF::HEIF,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_HEIF)
    return()
endif()

gdal_standard_includes(gdal_HEIF)
gdal_target_link_libraries(gdal_HEIF PRIVATE HEIF::HEIF)
