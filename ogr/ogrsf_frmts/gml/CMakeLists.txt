add_gdal_driver(
  TARGET ogr_GML
  SOURCES gfstemplate.cpp
          gmlpropertydefn.cpp
          gmlreadstate.cpp
          gmlutils.h
          ogrgmldriver.cpp
          resolvexlinks.cpp
          gmlfeature.cpp
          gmlreader.cpp
          gmlregistry.cpp
          hugefileresolver.cpp
          ogrgmllayer.cpp
          xercesc_headers.h
          gmlfeatureclass.cpp
          gmlreader.h
          gmlregistry.h
          ogr_gml.h
          parsexsd.cpp
          gmlhandler.cpp
          gmlreaderp.h
          gmlutils.cpp
          ogrgmldatasource.cpp
          parsexsd.h
  BUILTIN)
gdal_standard_includes(ogr_GML)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gfs.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gml_registry.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gml_registry.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/data/inspire_cp_BasicPropertyUnit.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/inspire_cp_CadastralBoundary.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/inspire_cp_CadastralParcel.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/inspire_cp_CadastralZoning.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_AdmArea.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_AdmBdry.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_AdmPt.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_BldA.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_BldL.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_Cntr.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_CommBdry.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_CommPt.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_Cstline.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_ElevPt.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_GCP.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_LeveeEdge.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RailCL.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdASL.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdArea.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdCompt.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdEdg.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdMgtBdry.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RdSgmtA.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_RvrMgtBdry.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_SBAPt.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_SBArea.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_SBBdry.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_WA.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_WL.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_WStrA.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/jpfgdgml_WStrL.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ruian_vf_ob_v1.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ruian_vf_st_uvoh_v1.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ruian_vf_st_v1.gfs
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ruian_vf_v1.gfs
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_GML PRIVATE ${EXPAT_TARGET})
endif ()
if (GDAL_USE_XERCESC)
  target_include_directories(ogr_GML PRIVATE ${XercesC_INCLUDE_DIRS}/xercesc)
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_XERCES)
  gdal_target_link_libraries(ogr_GML PRIVATE XercesC::XercesC)
endif ()
if (GDAL_USE_SQLITE3)
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_SQLITE)
  gdal_target_link_libraries(ogr_GML PRIVATE SQLite::SQLite3)
endif ()
