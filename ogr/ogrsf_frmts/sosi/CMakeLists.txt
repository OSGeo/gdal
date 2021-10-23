add_gdal_driver(
  TARGET ogr_SOSI
  SOURCES ogr_sosi.h
          ogrsosidatatypes.h
          fyba_melding.cpp
          ogrsosidatasource.cpp
          ogrsosidatatypes.cpp
          ogrsosidriver.cpp
          ogrsosilayer.cpp)
target_include_directories(ogr_SOSI $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_target_link_libraries(TARGET ogr_SOSI LIBRARIES SOSI::SOSI SOSI::FYGM SOSI::FYUT)

if (CMAKE_HOST_UNIX)
  target_compile_definitions(ogr_SOSI PRIVATE -DUNIX)
endif ()
if (CMAKE_SYSTEM_NAME EQUAL "Linux")
  target_compile_definitions(ogr_SOSI PRIVATE -DLINUX)
endif ()
