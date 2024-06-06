add_gdal_driver(
  TARGET ogr_GMLAS
  SOURCES ogr_gmlas.h
          ogrgmlasdatasource.cpp
          ogrgmlaslayer.cpp
          ogrgmlasutils.cpp
          ogrgmlasxpatchmatcher.cpp
          ogr_gmlas_consts.h
          ogrgmlasdriver.cpp
          ogrgmlasreader.cpp
          ogrgmlaswriter.cpp
          ogrgmlasxsdcache.cpp
          ogrgmlasconf.cpp
          ogrgmlasfeatureclass.cpp
          ogrgmlasschemaanalyzer.cpp
          ogrgmlasxlinkresolver.cpp
          xercesc_headers.h
  CORE_SOURCES
          ogrgmlasdrivercore.cpp
  STRONG_CXX_WFLAGS
  PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_GMLAS)
    return()
endif()

gdal_standard_includes(ogr_GMLAS)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gmlasconf.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gmlasconf.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

target_include_directories(
  ogr_GMLAS PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mem
                    ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/pgdump)

# Internal libs first
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_GMLAS libjson)
endif ()

# External libs then
if (NOT GDAL_USE_JSONC_INTERNAL)
  gdal_target_link_libraries(ogr_GMLAS PRIVATE ${JSONC_TARGET})
endif ()

target_compile_definitions(ogr_GMLAS PRIVATE -DHAVE_XERCES=1)
gdal_target_link_libraries(ogr_GMLAS PRIVATE XercesC::XercesC)
