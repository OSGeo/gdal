option(OGR_ENABLE_DWG_PLUGIN "Enable plugin for dwg driver" ON)
set(SOURCE
    ogrdgnv8datasource.cpp
    ogrdgnv8driver.cpp
    ogrdgnv8layer.cpp
    ogrdwg_blockmap.cpp
    ogrdwg_hatch.cpp
    ogrdwgblockslayer.cpp
    ogrdwgdatasource.cpp
    ogrdwgdriver.cpp)
if (OGR_ENABLE_DWG_PLUGIN)
  add_gdal_driver(
    TARGET ogr_DWG
    SOURCES ${SOURCE}
    PLUGIN)
else ()
  add_gdal_driver(TARGET ogr_DWG SOURCES ${SOURCE})
endif ()
gdal_standard_includes(ogr_DWG)
target_compile_definitions(ogr_DWG PRIVATE -DDO_NOT_USE_DEBUG_BOOL)
target_include_directories(ogr_DWG PRIVATE $<TARGET_PROPERTY:ogr_DXF,SOURCE_DIR> ${TEIGHA_INCLUDE_DIR})
gdal_target_link_libraries(ogr_DWG LIBRARIES ${TEIGHA_LIBRARIES})
