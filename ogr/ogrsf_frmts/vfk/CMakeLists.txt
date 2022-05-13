add_gdal_driver(
  TARGET ogr_VFK
  SOURCES ogr_vfk.h
          vfkreader.h
          vfkreaderp.h
          ogrvfkdatasource.cpp
          ogrvfkdriver.cpp
          ogrvfklayer.cpp
          vfkdatablock.cpp
          vfkdatablocksqlite.cpp
          vfkproperty.cpp
          vfkpropertydefn.cpp
          vfkreader.cpp
          vfkreadersqlite.cpp
          vfkfeature.cpp
          vfkfeaturesqlite.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_VFK)
if (GDAL_USE_SQLITE3)
  target_include_directories(ogr_VFK PRIVATE ${SQLite3_INCLUDE_DIR})
  gdal_target_link_libraries(ogr_VFK PRIVATE SQLite::SQLite3)
  target_compile_definitions(ogr_VFK PRIVATE -DHAVE_SQLITE)
endif ()
