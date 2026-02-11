# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.
add_library(
  gcore OBJECT
  gdal_adbc.cpp
  gdalalgorithm.cpp
  gdalalgorithmregistry.cpp
  gdalcomputedrasterband.cpp
  gdalopeninfo.cpp
  gdaldriver.cpp
  gdaldrivermanager.cpp
  gdaldataset.cpp
  gdalrasterband.cpp
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
  gdal_rat_vat_dbf.cpp
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
  gdalrelationship.cpp
  gdalsubdatasetinfo.cpp
  gdalorienteddataset.cpp
  gdalthreadsafedataset.cpp
  geoheif.cpp
  overview.cpp
  rasterio.cpp
  rawdataset.cpp
  gdalmultidim.cpp
  gdalmultidim_gridded.cpp
  gdalmultidim_gltorthorectification.cpp
  gdalmultidim_meshgrid.cpp
  gdalmultidim_subsetdimension.cpp
  gdalmultidim_rat.cpp
  gdalpython.cpp
  gdalpythondriverloader.cpp
  tilematrixset.cpp
  gdal_thread_pool.cpp
  nasakeywordhandler.cpp
  tiff_common.cpp
  enviutils.cpp
)

add_library(gcore_gdal_misc OBJECT gdal_misc.cpp)
gdal_standard_includes(gcore_gdal_misc)
add_dependencies(gcore_gdal_misc generate_gdal_version_h)
set_property(TARGET gcore_gdal_misc PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore_gdal_misc>)
target_link_libraries(gcore_gdal_misc PRIVATE PROJ::proj)
target_link_libraries(gcore PRIVATE PROJ::proj)

gdal_standard_includes(gcore)
target_compile_definitions(gcore PUBLIC $<$<CONFIG:DEBUG>:GDAL_DEBUG>)
if (USE_PRECOMPILED_HEADERS)
    target_precompile_headers(gcore REUSE_FROM gdal_priv_header)
endif()
add_dependencies(gcore generate_gdal_version_h)
target_compile_options(gcore PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX} ${WFLAG_DOUBLE_PROMOTION})
target_include_directories(
  gcore
  PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>
          $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
set_property(TARGET gcore PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

if (GDAL_ENABLE_DRIVER_GTIFF)
  target_include_directories(gcore PRIVATE $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>)
  target_compile_definitions(gcore PRIVATE -DHAVE_TIFF)
endif()

if (NOT DEFINED GDAL_RESOURCE_PATH)
  message(FATAL_ERROR "GDAL_RESOURCE_PATH not set")
endif ()
file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}/${GDAL_RESOURCE_PATH}" INST_DATA_PATH)
set_property(
  SOURCE gdaldrivermanager.cpp
  APPEND
  PROPERTY COMPILE_DEFINITIONS INST_DATA="${INST_DATA_PATH}" INSTALL_PLUGIN_FULL_DIR="${INSTALL_PLUGIN_FULL_DIR}")
if (GDAL_USE_XERCESC)
  target_include_directories(gcore PRIVATE ${XercesC_INCLUDE_DIRS} ${XercesC_INCLUDE_DIRS}/xercesc)
  set_property(
    SOURCE gdaldrivermanager.cpp
    APPEND
    PROPERTY COMPILE_DEFINITIONS HAVE_XERCES)
endif ()

option(GDAL_AUTOLOAD_PLUGINS "Set to OFF to prevent any attempt at loading plugins" ON)
mark_as_advanced(GDAL_AUTOLOAD_PLUGINS)
if (NOT GDAL_AUTOLOAD_PLUGINS)
  set_property(
    SOURCE gdaldrivermanager.cpp
    APPEND
    PROPERTY COMPILE_DEFINITIONS GDAL_NO_AUTOLOAD)
endif ()

if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
  target_compile_definitions(gcore PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME -DUSE_NEON_OPTIMIZATIONS)
  target_sources(gcore PRIVATE rasterio_ssse3.cpp)
elseif (HAVE_SSSE3_AT_COMPILE_TIME)
  add_library(gcore_rasterio_ssse3 OBJECT rasterio_ssse3.cpp)
  add_dependencies(gcore_rasterio_ssse3 generate_gdal_version_h)
  target_compile_definitions(gcore_rasterio_ssse3 PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
  gdal_standard_includes(gcore_rasterio_ssse3)
  set_property(TARGET gcore_rasterio_ssse3 PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore_rasterio_ssse3>)
  set_property(
    SOURCE rasterio_ssse3.cpp
    APPEND
    PROPERTY COMPILE_FLAGS ${GDAL_SSSE3_FLAG})
endif ()

if (EMBED_RESOURCE_FILES)
    add_library(gcore_resources OBJECT embedded_resources.c)
    gdal_standard_includes(gcore_resources)
    set_property(TARGET gcore_resources PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
    target_compile_definitions(gcore PRIVATE EMBED_RESOURCE_FILES)
    target_compile_definitions(gcore_gdal_misc PRIVATE EMBED_RESOURCE_FILES)
    set_target_properties(gcore_resources PROPERTIES C_STANDARD 23)
    target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore_resources>)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(gcore PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(gcore_gdal_misc PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()

target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore>)

if (GDAL_USE_JSONC_INTERNAL)
  target_include_directories(gcore PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
endif ()

if (ENABLE_PAM)
  target_compile_definitions(gcore PRIVATE -DPAM_ENABLED)
endif ()

if (GDAL_USE_MUPARSER)
  target_compile_definitions(gcore PRIVATE -DHAVE_MUPARSER)
endif()

if (NOT GDAL_ENABLE_DRIVER_DERIVED)
  target_compile_definitions(gcore PRIVATE -DWITHOUT_DERIVED)
endif ()

if (NOT GDAL_ENABLE_DRIVER_HFA OR GDAL_ENABLE_DRIVER_HFA_PLUGIN)
  target_compile_definitions(gcore PRIVATE -DNO_HFA_SUPPORT)
endif()

add_subdirectory(mdreader)

# External libs now
if (NOT GDAL_USE_JSONC_INTERNAL)
  gdal_target_link_libraries(gcore PRIVATE ${JSONC_TARGET})
endif ()

if (NOT DEFINED OGR_ENABLE_DRIVER_SQLITE)
  message(FATAL_ERROR "OGR_ENABLE_DRIVER_SQLITE should be set to ON or OFF")
endif ()
if (OGR_ENABLE_DRIVER_SQLITE)
  target_compile_definitions(gcore PRIVATE -DSQLITE_ENABLED)
endif ()
if (GDAL_USE_LIBXML2)
  target_compile_definitions(gcore PRIVATE -DHAVE_LIBXML2)
  gdal_target_link_libraries(gcore PRIVATE LibXml2::LibXml2)

  # Some libxml2 builds may require Iconv (libxml2/libxml/encoding.h includes iconv.h if LIBXML_ICONV_ENABLED is
  # defined)
  if (GDAL_USE_ICONV)
    if (NOT Iconv_IS_BUILT_IN)
      target_include_directories(gcore PRIVATE $<TARGET_PROPERTY:Iconv::Iconv,INTERFACE_INCLUDE_DIRECTORIES>)
    endif ()
  endif ()

endif ()
if (GDAL_USE_GEOS)
  target_compile_definitions(gcore PRIVATE -DHAVE_GEOS)
  gdal_target_link_libraries(gcore PRIVATE ${GEOS_TARGET})
  target_compile_definitions(gcore_gdal_misc PRIVATE -DHAVE_GEOS)
  gdal_target_link_libraries(gcore_gdal_misc PRIVATE ${GEOS_TARGET})
endif ()

if (GDAL_USE_CURL)
  # Just for gdal_misc.cpp in fact
  target_compile_definitions(gcore_gdal_misc PRIVATE -DHAVE_CURL)
  gdal_target_link_libraries(gcore_gdal_misc PRIVATE CURL::libcurl)
endif ()

# Windows(Mingw/MSVC) link libraries
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  # psapi needed for gdalpython.cpp (EnumProcessModules)
  gdal_target_link_libraries(gcore PRIVATE psapi)
endif ()

if (GDAL_USE_OPENMP AND OpenMP_C_LIBRARIES)
    gdal_target_link_libraries(gcore PRIVATE ${OpenMP_C_LIBRARIES})
endif()

# Build some files with AVX2 and FMA, if those are not by default enabled,
# and detect at runtime if we can use them
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" AND
    (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM" OR
     CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9)) AND
    HAVE_AVX2_AT_COMPILE_TIME AND
    HAVE_FMA_AT_COMPILE_TIME AND
    (NOT HAVE_AVX2_FMA_WITHOUT_FLAG) AND
    (NOT "${GDAL_AVX2_FLAG}" STREQUAL "") AND (NOT "${GDAL_FMA_FLAG}" STREQUAL ""))

  target_compile_definitions(gcore PRIVATE CAN_DETECT_AVX2_FMA_AT_RUNTIME)

  add_library(gcore_avx2_fma OBJECT gdal_matrix_avx2_fma.cpp)
  add_dependencies(gcore_avx2_fma generate_gdal_version_h)
  target_compile_options(gcore_avx2_fma PRIVATE ${WFLAG_DOUBLE_PROMOTION})
  gdal_standard_includes(gcore_avx2_fma)
  set_property(TARGET gcore_avx2_fma PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  set_property(TARGET gcore_avx2_fma PROPERTY COMPILE_FLAGS ${GDAL_AVX2_FLAG})
  set_property(TARGET gcore_avx2_fma PROPERTY COMPILE_FLAGS ${GDAL_FMA_FLAG})
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore_avx2_fma>)
endif ()

include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/gdal_version_full/gdal_version.h
  gdal.h
  gdal_raster_cpp.h
  gdal_multidim_cpp.h
  gdal_vector_cpp.h
  gdal_multidomainmetadata.h
  gdal_majorobject.h
  gdal_defaultoverviews.h
  gdal_openinfo.h
  gdal_gcp.h
  gdal_geotransform.h
  gdal_dataset.h
  gdal_rasterblock.h
  gdal_colortable.h
  gdal_rasterband.h
  gdal_computedrasterband.h
  gdal_maskbands.h
  gdal_driver.h
  gdal_drivermanager.h
  gdal_asyncreader.h
  gdal_multidim.h
  gdal_relationship.h
  gdal_cpp_functions.h
  gdal_fwd.h
  gdalalgorithm.h
  gdalalgorithm_c.h
  gdalalgorithm_cpp.h
  gdaljp2metadata.h
  gdaljp2abstractdataset.h
  gdal_frmts.h
  gdal_pam.h
  gdal_pam_multidim.h
  gdal_priv.h
  gdal_proxy.h
  gdal_rat.h
  gdalcachedpixelaccessor.h
  rawdataset.h
  gdalgeorefpamdataset.h
  gdal_mdreader.h
  gdalsubdatasetinfo.h
  gdal_typetraits.h
  gdal_adbc.h
  gdal_minmax_element.hpp
  gdal_priv_templates.hpp  # Required by gdal_minmax_element.hpp
  geoheif.h
  gdal_thread_pool.h
)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/tms_LINZAntarticaMapTileGrid.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/tms_MapML_APSTILE.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/tms_MapML_CBMTILE.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/tms_NZTM2000.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/template_tiles.mapml
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
