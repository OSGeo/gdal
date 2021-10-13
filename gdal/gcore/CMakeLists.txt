# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.
add_library(
  gcore OBJECT
  gdalopeninfo.cpp
  gdaldriver.cpp
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
target_include_directories(
  gcore
  PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>
          $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR> $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>
          $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR> $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
set_property(TARGET gcore PROPERTY POSITION_INDEPENDENT_CODE ON)

if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_definitions(gcore PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
endif ()

add_library(gdaldrivermanager OBJECT gdaldrivermanager.cpp)
gdal_standard_includes(gdaldrivermanager)
file(TO_CMAKE_PATH "${CMAKE_PREFIX_PATH}/${CMAKE_INSTALL_DATADIR}" INST_DATA_PATH)
target_compile_definitions(gdaldrivermanager PRIVATE -DINST_DATA="${INST_DATA_PATH}")
set_property(TARGET gdaldrivermanager PROPERTY POSITION_INDEPENDENT_CODE ON)
if (HAVE_XERCESC)
  target_include_directories(gdaldrivermanager PRIVATE ${XercesC_INCLUDE_DIRS} ${XercesC_INCLUDE_DIRS}/xercesc)
  target_compile_definitions(gdaldrivermanager PRIVATE -DHAVE_XERCES)
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE XercesC::XercesC)
endif ()

add_library(rasterio_ssse3 OBJECT rasterio_ssse3.cpp)
gdal_standard_includes(rasterio_ssse3)
target_include_directories(rasterio_ssse3 PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>
                                                  $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
set_property(TARGET rasterio_ssse3 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(gdal PRIVATE $<TARGET_OBJECTS:gcore> $<TARGET_OBJECTS:gdaldrivermanager>
                            $<TARGET_OBJECTS:rasterio_ssse3>)
include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/gdal_version.h
  gdal.h
  gdaljp2metadata.h
  gdal_frmts.h
  gdal_pam.h
  gdal_priv.h
  gdal_proxy.h
  gdal_rat.h
  rawdataset.h
  gdalexif.h
  gdalgeorefpamdataset.h
  gdal_priv_templates.hpp
  gdal_avx2_emulation.hpp
  gdal_mdreader.h
  gdal_pam.h
  gdalsse_priv.h)

if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gcore PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  target_include_directories(gcore PRIVATE ${JSONC_INCLUDE_DIRS})
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE JSONC::JSONC)
endif ()

if (USE_CPL)
  target_compile_definitions(gcore PRIVATE -DUSE_CPL)
endif ()
if (ENABLE_PAM)
  target_compile_definitions(gcore PRIVATE -DPAM_ENABLED)
endif ()

if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_options(rasterio_ssse3 PRIVATE ${GDAL_SSSE3_FLAG})
  target_compile_definitions(rasterio_ssse3 PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
endif ()

add_subdirectory(mdreader)

if (HAVE_SQLITE3)
  target_compile_definitions(gcore PRIVATE -DSQLITE_ENABLED)
  target_include_directories(gcore PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE SQLite::SQLite3)
endif ()
if (HAVE_LIBXML2)
  target_compile_definitions(gcore PRIVATE -DHAVE_LIBXML2)
  target_include_directories(gcore PRIVATE ${LIBXML2_INCLUDE_DIRS})
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE LibXml2::LibXml2)
endif ()
if (HAVE_GEOS)
  target_compile_definitions(gcore PRIVATE -DHAVE_GEOS)
  target_include_directories(gcore PRIVATE ${GEOS_INCLUDE_DIRS})
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE GEOS::GEOS)
endif ()
