# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.
add_library(
  gcore OBJECT
  gdalopeninfo.cpp
  gdaldriver.cpp
  gdaldrivermanager.cpp
  gdaldataset.cpp
  gdalrasterband.cpp
  gdal_misc.cpp
  gdalrasterblock.cpp
  gdalcolortable.cpp
  gdalmajorobject.cpp
  gdaldefaultoverviews.cpp
  gdalpamdataset.cpp
  gdalpamrasterband.cpp
  gdaljp2metadata.cpp
  gdaljp2box.cpp
  gdalmultidomainmetadata.cpp
  gdal_rat.cpp
  gdalpamproxydb.cpp
  gdalallvalidmaskband.cpp
  gdalnodatamaskband.cpp
  gdalnodatavaluesmaskband.cpp
  gdalproxydataset.cpp
  gdalproxypool.cpp
  gdaldefaultasync.cpp
  gdaldllmain.cpp
  gdalexif.cpp
  gdalgeorefpamdataset.cpp
  gdaljp2abstractdataset.cpp
  gdalvirtualmem.cpp
  gdaloverviewdataset.cpp
  gdalrescaledalphaband.cpp
  gdaljp2structure.cpp
  gdal_mdreader.cpp
  gdaljp2metadatagenerator.cpp
  gdalabstractbandblockcache.cpp
  gdalarraybandblockcache.cpp
  gdalhashsetbandblockcache.cpp
  overview.cpp
  rasterio.cpp
  rawdataset.cpp
  gdalmultidim.cpp
  gdalpython.cpp
  gdalpythondriverloader.cpp
  tilematrixset.cpp
  gdal_thread_pool.cpp
  nasakeywordhandler.cpp)
gdal_standard_includes(gcore)
add_dependencies(gcore generate_gdal_version_h)
target_compile_options(gcore PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
target_include_directories(
  gcore
  PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>
          $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR> $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>
          $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR> $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
set_property(TARGET gcore PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

if (NOT DEFINED GDAL_RESOURCE_PATH)
  message(FATAL_ERROR "GDAL_RESOURCE_PATH not set")
endif ()
file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}/${GDAL_RESOURCE_PATH}" INST_DATA_PATH)
set_property(
  SOURCE gdaldrivermanager.cpp
  APPEND
  PROPERTY COMPILE_DEFINITIONS INST_DATA="${INST_DATA_PATH}")
if (GDAL_USE_XERCESC)
  target_include_directories(gcore PRIVATE ${XercesC_INCLUDE_DIRS} ${XercesC_INCLUDE_DIRS}/xercesc)
  set_property(
    SOURCE gdaldrivermanager.cpp
    APPEND
    PROPERTY COMPILE_DEFINITIONS HAVE_XERCES)
endif ()

if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_definitions(gcore PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
  target_sources(gcore PRIVATE rasterio_ssse3.cpp)
  set_property(
    SOURCE rasterio_ssse3.cpp
    APPEND
    PROPERTY COMPILE_FLAGS ${GDAL_SSSE3_FLAG})
endif ()

target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore>)

if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gcore PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
endif ()

if (ENABLE_PAM)
  target_compile_definitions(gcore PRIVATE -DPAM_ENABLED)
endif ()

add_subdirectory(mdreader)

# External libs now
if (NOT GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gcore PRIVATE ${JSONC_INCLUDE_DIRS})
  gdal_add_private_link_libraries(JSONC::JSONC)
endif ()

if (NOT DEFINED OGR_ENABLE_SQLITE)
  message(FATAL_ERROR "OGR_ENABLE_SQLITE should be set to ON or OFF")
endif ()
if (OGR_ENABLE_SQLITE)
  target_compile_definitions(gcore PRIVATE -DSQLITE_ENABLED)
endif ()
if (GDAL_USE_LIBXML2)
  target_compile_definitions(gcore PRIVATE -DHAVE_LIBXML2)
  target_include_directories(gcore PRIVATE ${LIBXML2_INCLUDE_DIRS})
  gdal_add_private_link_libraries(LibXml2::LibXml2)
endif ()
if (GDAL_USE_GEOS)
  target_compile_definitions(gcore PRIVATE -DHAVE_GEOS)
  target_include_directories(gcore PRIVATE ${GEOS_INCLUDE_DIRS})
  gdal_add_private_link_libraries(GEOS::GEOS)
endif ()

include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/gdal_version_full/gdal_version.h
  gdal.h
  gdaljp2metadata.h
  gdaljp2abstractdataset.h
  gdal_frmts.h
  gdal_pam.h
  gdal_priv.h
  gdal_proxy.h
  gdal_rat.h
  rawdataset.h
  gdalgeorefpamdataset.h
  gdal_mdreader.h
  gdal_pam.h)
