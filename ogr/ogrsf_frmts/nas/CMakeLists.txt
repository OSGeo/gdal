add_gdal_driver(
  TARGET ogr_NAS
  SOURCES nashandler.cpp
          nasreader.cpp
          nasreaderp.h
          ogr_nas.h
          ogrnasdatasource.cpp
          ogrnasdriver.cpp
          ogrnaslayer.cpp
          ogrnasrelationlayer.cpp)
gdal_standard_includes(ogr_NAS)
target_include_directories(ogr_NAS PRIVATE $<TARGET_PROPERTY:ogr_GML,SOURCE_DIR>)
if (HAVE_EXPAT)
  target_include_directories(ogr_NAS PRIVATE ${EXPAT_INCLUDE_DIRS})
  target_compile_definitions(ogr_NAS PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(TARGET ogr_NAS LIBRARIES EXPAT::EXPAT)
endif ()
if (HAVE_XERCESC)
  foreach (d ${XercesC_INCLUDE_DIRS})
    target_include_directories(ogr_NAS PRIVATE ${d} ${d}/xercesc)
  endforeach ()
  gdal_target_link_libraries(TARGET ogr_NAS LIBRARIES XercesC::XercesC)
  target_compile_definitions(ogr_NAS PRIVATE -DHAVE_XERCES)
endif ()
