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
  ogrgeomcoordinateprecision.cpp
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
  ogr_srs_cf1.cpp
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
  ogr_proj_p.cpp
  ogr_wkb.cpp)
add_dependencies(ogr generate_gdal_version_h)

set(GDAL_DATA_FILES
    LICENSE.TXT
    ${CMAKE_CURRENT_SOURCE_DIR}/data/cubewerx_extra.wkt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ecw_cs.wkt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/epsg.wkt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/esri_StatePlane_extra.wkt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ozi_datum.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ozi_ellips.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/pci_datum.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/pci_ellips.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/stateplane.csv
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

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
  if (MSVC AND NOT ZLIB_IS_STATIC)
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
  ogr_geomcoordinateprecision.h
  ogr_p.h
  ogr_spatialref.h
  ogr_swq.h
  ogr_srs_api.h
  ogrsf_frmts/ogrsf_frmts.h)

###########################################################
# Targets to refresh swq_parser.cpp
###########################################################

# Those targets need to be run manually each time swq_parser.y
# is modified.
# We could of course run them automatically, but that would make building
# GDAL harder.

# This target checks that swq_parser.y md5sum has not changed
# If it has, then it should be updated and the generate_swq_parser target
# should be manually run
add_custom_target(check_swq_parser_md5 ALL
                  COMMAND ${CMAKE_COMMAND}
                      "-DIN_FILE=swq_parser.y"
                      "-DTARGET=generate_swq_parser"
                      "-DEXPECTED_MD5SUM=f7faffefe55128fc8951556909523ef7"
                      "-DFILENAME_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
                      -P "${PROJECT_SOURCE_DIR}/cmake/helpers/check_md5sum.cmake"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/swq_parser.y"
                  VERBATIM)

find_package(BISON)
add_custom_target(generate_swq_parser
                  COMMAND ${CMAKE_COMMAND}
                      "-DBISON_FOUND=${BISON_FOUND}"
                      "-DBISON_EXECUTABLE=${BISON_EXECUTABLE}"
                      -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_swq_parser.cmake"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  VERBATIM)
