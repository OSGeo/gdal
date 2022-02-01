add_gdal_driver(TARGET gdal_JP2KAK SOURCES jp2kakdataset.cpp jp2kak_headers.h jp2kakdataset.h subfile_source.h
                                           vsil_target.h PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JP2KAK)
gdal_target_link_libraries(gdal_JP2KAK PRIVATE KDU::KDU)
target_compile_definitions(gdal_JP2KAK PRIVATE -DKDU_X86_INTRINSICS)
