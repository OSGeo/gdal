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
if (HAVE_EXPAT)
  target_include_directories(ogr_GML PRIVATE ${EXPAT_INCLUDE_DIRS})
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(TARGET ogr_GML LIBRARIES EXPAT::EXPAT)
endif ()
if (GDAL_USE_XERCESC)
  target_include_directories(ogr_GML PRIVATE ${XercesC_INCLUDE_DIRS} ${XercesC_INCLUDE_DIRS}/xercesc)
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_XERCES)
  gdal_target_link_libraries(TARGET ogr_GML LIBRARIES XercesC::XercesC)
endif ()
if (GDAL_USE_SQLITE3)
  target_include_directories(ogr_GML PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_compile_definitions(ogr_GML PRIVATE -DHAVE_SQLITE)
  gdal_target_link_libraries(TARGET ogr_GML LIBRARIES SQLite::SQLite3)
endif ()
