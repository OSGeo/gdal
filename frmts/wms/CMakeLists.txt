add_gdal_driver(
  TARGET gdal_WMS
  SOURCES gdalhttp.cpp
          gdalwmsrasterband.cpp
          minidriver_iip.cpp
          minidriver_tiled_wms.cpp
          minidriver_tms.cpp
          minidriver_wms.cpp
          wmsdriver.cpp
          wmsutils.cpp
          gdalhttp.h
          minidriver.cpp
          minidriver_iip.h
          minidriver_tiled_wms.h
          minidriver_tms.h
          minidriver_wms.h
          wmsdriver.h
          gdalwmscache.cpp
          minidriver_arcgis_server.cpp
          minidriver_mrf.cpp
          minidriver_ogcapicoverage.cpp
          minidriver_ogcapimaps.cpp
          minidriver_tileservice.cpp
          minidriver_virtualearth.cpp
          minidriver_worldwind.cpp
          wmsmetadataset.cpp
          gdalwmsdataset.cpp
          minidriver_arcgis_server.h
          minidriver_mrf.h
          minidriver_ogcapicoverage.h
          minidriver_ogcapimaps.h
          minidriver_tileservice.h
          minidriver_virtualearth.h
          minidriver_worldwind.h
          wmsmetadataset.h
          PLUGIN_CAPABLE)
gdal_standard_includes(gdal_WMS)
target_include_directories(gdal_WMS PRIVATE ${CURL_INCLUDE_DIRS} $<TARGET_PROPERTY:alg,SOURCE_DIR>
                                            $<TARGET_PROPERTY:gdal_raw,SOURCE_DIR>)
target_compile_definitions(gdal_WMS PRIVATE -DHAVE_CURL)
gdal_target_link_libraries(TARGET gdal_WMS LIBRARIES CURL::libcurl)
