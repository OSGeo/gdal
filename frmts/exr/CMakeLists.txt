add_gdal_driver(TARGET gdal_EXR SOURCES exrdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_EXR)
gdal_target_link_libraries(TARGET gdal_EXR LIBRARIES OpenEXR::IlmImf OpenEXR::IlmImfUtil OpenEXR::Half OpenEXR::Iex)
