add_gdal_driver(TARGET gdal_WEBP
                SOURCES webp_headers.h webpdataset.cpp
                CORE_SOURCES webpdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_WEBP_core)
    target_include_directories(gdal_WEBP_core PRIVATE $<TARGET_PROPERTY:WEBP::WebP,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_WEBP_core PRIVATE $<TARGET_PROPERTY:WEBP::WebP,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_WEBP)
    return()
endif()

gdal_standard_includes(gdal_WEBP)
gdal_target_link_libraries(gdal_WEBP PRIVATE WEBP::WebP)
