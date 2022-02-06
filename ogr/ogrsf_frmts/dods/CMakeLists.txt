add_gdal_driver(
  TARGET ogr_DODS
  SOURCES libdap_headers.h
          ogr_dods.h
          ogrdodsdatasource.cpp
          ogrdodsdriver.cpp
          ogrdodsfielddefn.cpp
          ogrdodsgrid.cpp
          ogrdodslayer.cpp
          ogrdodssequencelayer.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_DODS)
target_include_directories(ogr_DODS PRIVATE ${DAP_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIRS})
gdal_target_link_libraries(ogr_DODS PRIVATE DAP::DAP DAP::CLIENT)
gdal_target_link_libraries(ogr_DODS PRIVATE LibXml2::LibXml2)
if (DAP_VERSION VERSION_GREATER_EQUAL 3.9)
  target_compile_definitions(ogr_DODS PRIVATE -DLIBDAP_39)
  if (DAP_VERSION VERSION_GREATER_EQUAL 3.10)
    target_compile_definitions(ogr_DODS PRIVATE -DLIBDAP_310)
  endif ()
endif ()
