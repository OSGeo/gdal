# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

add_library(
  ogr OBJECT
  ogrgeometryfactory.cpp
  ogrpoint.cpp
  ogrcurve.cpp
  ogrlinestring.cpp
  ogrlinearring.cpp
  ogrpolygon.cpp
  ogrtriangle.cpp
  ogrutils.cpp
  ogrgeometry.cpp
  ogrgeometrycollection.cpp
  ogrmultipolygon.cpp
  ogrsurface.cpp
  ogrpolyhedralsurface.cpp
  ogrtriangulatedsurface.cpp
  ogrmultipoint.cpp
  ogrmultilinestring.cpp
  ogrcircularstring.cpp
  ogrcompoundcurve.cpp
  ogrcurvepolygon.cpp
  ogrcurvecollection.cpp
  ogrmulticurve.cpp
  ogrmultisurface.cpp
  ogr_api.cpp
  ogrfeature.cpp
  ogrfeaturedefn.cpp
  ogrfeaturequery.cpp
  ogrfeaturestyle.cpp
  ogrfielddefn.cpp
  ogrspatialreference.cpp
  ogr_srsnode.cpp
  ogr_fromepsg.cpp
  ogrct.cpp
  ogr_srs_esri.cpp
  ogr_srs_pci.cpp
  ogr_srs_usgs.cpp
  ogr_srs_dict.cpp
  ogr_srs_panorama.cpp
  ogr_srs_ozi.cpp
  ogr_srs_erm.cpp
  swq.cpp
  swq_expr_node.cpp
  swq_parser.cpp
  swq_select.cpp
  swq_op_registrar.cpp
  swq_op_general.cpp
  ogr_srs_xml.cpp
  ograssemblepolygon.cpp
  ogr2gmlgeometry.cpp
  gml2ogrgeometry.cpp
  ogr_expat.cpp
  ogrpgeogeometry.cpp
  ogr_geocoding.cpp
  ogrgeomfielddefn.cpp
  ograpispy.cpp
  ogr_xerces.cpp
  ogr_geo_utils.cpp
  ogr_proj_p.cpp)
add_dependencies(ogr generate_gdal_version_h)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(
    SOURCE ogrfeaturequery.cpp
           ogr_srs_usgs.cpp
           ogr_srs_panorama.cpp
           swq_expr_node.cpp
           swq_op_general.cpp
           swq_parser.cpp
           swq_select.cpp
    PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
include(GdalStandardIncludes)
gdal_standard_includes(ogr)
target_compile_options(ogr PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
target_include_directories(ogr PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
set_property(TARGET ogr PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:ogr>)

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(ogr libz)
endif ()

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr libjson)
endif ()

target_compile_definitions(ogr PRIVATE HAVE_MITAB)

gdal_target_link_libraries(ogr PRIVATE PROJ::proj)

# External libs then
if (GDAL_USE_GEOS)
  target_compile_definitions(ogr PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(ogr PRIVATE ${GEOS_TARGET})
endif ()

if (GDAL_USE_SFCGAL)
  target_compile_definitions(ogr PRIVATE -DHAVE_SFCGAL=1)
  gdal_target_link_libraries(ogr PRIVATE SFCGAL::SFCGAL)
  # work around for SFCGAL 1.3.7 on Ubuntu 20.04/Debian
  if (SFCGAL_VERSION_STRING VERSION_EQUAL "1.3.7")
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      message(
        WARNING
          "Detect SFCGAL v1.3.7 on Linux (maybe Ubuntu 20.04?), known as buggy that cause a link error. Trying a workaround"
        )
      find_library(GMPXX_LIBRARY NAMES gmpxx)
      gdal_target_link_libraries(ogr PRIVATE ${GMPXX_LIBRARY})
    endif ()
  endif ()
endif ()

if (NOT GDAL_USE_ZLIB_INTERNAL)
  target_compile_definitions(ogr PRIVATE -DHAVE_ZLIB_H -DHAVE_ZLIB)
  if (MSVC)
    target_compile_definitions(ogr PRIVATE -DZLIB_DLL)
  endif ()
  gdal_target_link_libraries(ogr PRIVATE ZLIB::ZLIB)
endif ()

if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr PRIVATE ${EXPAT_TARGET})
endif ()

if (GDAL_USE_XERCESC)
  target_sources(ogr PRIVATE ogr_xerces_headers.h)
  target_compile_definitions(ogr PRIVATE -DHAVE_XERCES)
  gdal_target_link_libraries(ogr PRIVATE XercesC::XercesC)
endif ()

if (NOT GDAL_USE_JSONC_INTERNAL)
  gdal_target_link_libraries(ogr PRIVATE ${JSONC_TARGET})
endif ()

include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  ogr_api.h
  ogr_recordbatch.h
  ogr_core.h
  ogr_feature.h
  ogr_featurestyle.h
  ogr_geocoding.h
  ogr_geometry.h
  ogr_p.h
  ogr_spatialref.h
  ogr_swq.h
  ogr_srs_api.h
  ogrsf_frmts/ogrsf_frmts.h)
