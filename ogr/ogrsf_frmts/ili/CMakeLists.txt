# ILI has two drivers, ILI1 and ILI2
add_gdal_driver(
  TARGET ogr_ILI
  DEF ILI_ENABLED
  SOURCES ili1reader.cpp
          ogr_ili1.h
          ogrili1driver.cpp
          ili1reader.h
          ogrili1layer.cpp
          ili1readerp.h
          ogrili1datasource.cpp
          ili2handler.cpp
          ili2readerp.h
          ogrili2driver.cpp
          ili2reader.h
          ili2reader.cpp
          ogr_ili2.h
          ogrili2layer.cpp
          ogrili2datasource.cpp
          imdreader.h
          imdreader.cpp
  BUILTIN) # FIXME: could be PLUGIN_CAPABLE NO_DEPS if there was a global GDALRegisterMe() entry point
gdal_standard_includes(ogr_ILI)

if (GDAL_USE_XERCESC)
  target_include_directories(ogr_ILI PRIVATE ${XercesC_INCLUDE_DIRS} ${XercesC_INCLUDE_DIRS}/xercesc)
  target_compile_definitions(ogr_ILI PRIVATE -DHAVE_XERCES)
  gdal_target_link_libraries(TARGET ogr_ILI LIBRARIES XercesC::XercesC)
endif ()
if (GDAL_USE_GEOS)
  target_include_directories(ogr_ILI PRIVATE ${GEOS_INCLUDE_DIRS})
  target_compile_definitions(ogr_ILI PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(TARGET ogr_ILI LIBRARIES GEOS::GEOS)
endif ()
