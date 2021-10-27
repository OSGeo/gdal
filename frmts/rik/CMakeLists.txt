add_gdal_driver(TARGET gdal_RIK SOURCES rikdataset.cpp PLUGIN_CAPABLE_IF "NOT GDAL_USE_LIBZ_INTERNAL" NO_DEPS)
gdal_standard_includes(gdal_RIK)

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_RIK PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  target_include_directories(gdal_RIK PRIVATE $<TARGET_PROPERTY:ZLIB::ZLIB,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_target_link_libraries(TARGET gdal_RIK LIBRARIES ZLIB::ZLIB)
endif ()
