# depends from gpkg, osm
add_gdal_driver(
  TARGET ogr_SQLite
  BUILTIN
  SOURCES ogr_sqlite.h
          ogrsqlite3ext.h
          ogrsqliteexecutesql.h
          ogrsqlitesqlfunctions.h
          ogrsqlitevirtualogr.h
          ogrsqliteutility.h
          ogrsqlitedatasource.cpp
          ogrsqlitedriver.cpp
          ogrsqlitelayer.cpp
          ogrsqliteselectlayer.cpp
          ogrsqlitesinglefeaturelayer.cpp
          ogrsqlitetablelayer.cpp
          ogrsqlitevfs.cpp
          ogrsqliteviewlayer.cpp
          ogrsqliteexecutesql.cpp
          ogrsqlitevirtualogr.cpp
          ogrsqliteutility.cpp
          ogrsqliteapiroutines.c
          rasterlite2_header.h
          rasterlite2.cpp)
gdal_standard_includes(ogr_SQLite)

target_compile_definitions(ogr_SQLite PRIVATE -DHAVE_SQLITE)
gdal_target_link_libraries(TARGET ogr_SQLite LIBRARIES SQLite::SQLite3)
gdal_target_link_libraries(TARGET ogr_SQLite LIBRARIES PROJ::PROJ)

if (SQLITE_HAS_PCRE AND GDAL_USE_PCRE)
  target_include_directories(ogr_SQLite PRIVATE ${PCRE_INCLUDE_DIRS})
  target_compile_definitions(ogr_SQLite PRIVATE -DHAVE_PCRE)
  gdal_target_link_libraries(TARGET ogr_SQLite LIBRARIES PCRE::PCRE)
endif ()

if (SQLITE_HAS_COLUMN_METADATA)
  target_compile_definitions(ogr_SQLite PRIVATE -DSQLITE_HAS_COLUMN_METADATA)
endif ()

if (GDAL_USE_SPATIALITE)
  get_filename_component(SPATIALITE_SONAME ${SPATIALITE_LIBRARY} NAME CACHE)
  target_compile_definitions(ogr_SQLite PRIVATE -DHAVE_SPATIALITE)
  gdal_target_link_libraries(TARGET ogr_SQLite LIBRARIES SPATIALITE::SPATIALITE)
  if (SPATIALITE_VERSION_STRING VERSION_GREATER 4.1.2 OR SPATIALITE_VERSION_STRING VERSION_EQUAL 4.1.2)
    target_compile_definitions(ogr_SQLite PRIVATE -DSPATIALITE_412_OR_LATER)
  endif ()
endif ()
if (GDAL_USE_RASTERLITE2)
  target_compile_definitions(ogr_SQLite PRIVATE -DHAVE_RASTERLITE2)
  gdal_target_link_libraries(TARGET ogr_SQLite LIBRARIES RASTERLITE2::RASTERLITE2)
endif ()

# Test utility
add_executable(test_load_virtual_ogr EXCLUDE_FROM_ALL test_load_virtual_ogr.c)
gdal_standard_includes(test_load_virtual_ogr)
target_link_libraries(test_load_virtual_ogr PRIVATE SQLite::SQLite3)
if (GDAL_USE_SPATIALITE)
  target_include_directories(test_load_virtual_ogr PRIVATE ${SPATIALITE_INCLUDE_DIRS})
  target_compile_definitions(test_load_virtual_ogr PRIVATE -DHAVE_SPATIALITE)
  target_link_libraries(test_load_virtual_ogr PRIVATE SPATIALITE::SPATIALITE)
  if (SPATIALITE_AMALGAMATION)
    target_compile_definitions(test_load_virtual_ogr PRIVATE -DSPATIALITE_AMALGAMATION)
  endif ()
endif ()
