add_gdal_driver(TARGET gdal_EXR
                SOURCES exrdataset.cpp
                CORE_SOURCES exrdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_EXR)
    return()
endif()

gdal_standard_includes(gdal_EXR)
gdal_target_link_libraries(gdal_EXR PRIVATE OpenEXR::IlmImf OpenEXR::IlmImfUtil OpenEXR::Half OpenEXR::Iex)
