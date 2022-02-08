add_gdal_driver(
  TARGET gdal_raw
  SOURCES atlsci_spheroid.h
          ace2dataset.cpp
          atlsci_spheroid.cpp
          btdataset.cpp
          cpgdataset.cpp
          ctable2dataset.cpp
          dipxdataset.cpp
          doq1dataset.cpp
          doq2dataset.cpp
          ehdrdataset.cpp
          eirdataset.cpp
          envidataset.cpp
          fastdataset.cpp
          genbindataset.cpp
          gscdataset.cpp
          gtxdataset.cpp
          hkvdataset.cpp
          krodataset.cpp
          iscedataset.cpp
          landataset.cpp
          lcpdataset.cpp
          loslasdataset.cpp
          mffdataset.cpp
          ndfdataset.cpp
          ntv2dataset.cpp
          pauxdataset.cpp
          pnmdataset.cpp
          roipacdataset.cpp
          rrasterdataset.cpp
          snodasdataset.cpp
          byndataset.cpp
          byndataset.cpp
          envidataset.cpp
  BUILTIN CXX_WFLAGS_EFFCXX)
gdal_standard_includes(gdal_raw)
target_include_directories(gdal_raw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(
    SOURCE doq1dataset.cpp
           doq2dataset.cpp
           envidataset.cpp
           genbindataset.cpp
           atlsci_spheroid.cpp
           hkvdataset.cpp
           mffdataset.cpp
    PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
