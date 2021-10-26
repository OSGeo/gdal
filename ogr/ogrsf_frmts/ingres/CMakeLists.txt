option(OGR_ENABLE_INGRESS_PLUGIN "Enable plugin for ogr_INGRESS driver" ON)
set(SOURCE
    ogringresdatasource.cpp
    ogringresdriver.cpp
    ogringreslayer.cpp
    ogringresresultlayer.cpp
    ogringresresultlayer.cpp
    ogringresstatement.cpp
    ogringrestablelayer.cpp)
if (OGR_ENABLE_INGRESS_PLUGIN)
  add_gdal_driver(TARGET ogr_INGRESS SOURCE ${SOURCE} PLUGIN)
else ()
  add_gdal_driver(TARGET ogr_INGRESS SOURCE ${SOURCE})
endif ()
gdal_standard_includes(ogr_INGRESS)
target_include_directories(ogr_INGRESS PRIVATE ${INGRESS_INCLUDE_DIR})
gdal_target_link_libraries(ogr_INGRESS LIBRARIES ${INGRESS_LIBRARY})
