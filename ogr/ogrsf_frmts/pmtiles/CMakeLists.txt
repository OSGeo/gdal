# Cannot be plugin due to dependency to symbols in mvt/
add_gdal_driver(TARGET ogr_PMTiles
                SOURCES
                  ogrpmtilesdriver.cpp
                  ogrpmtilesdataset.cpp
                  ogrpmtilesvectorlayer.cpp
                  ogrpmtilestileiterator.cpp
                  ogrpmtilesfrommbtiles.cpp
                  ogrpmtileswriterdataset.cpp
                  vsipmtiles.cpp
                BUILTIN
                STRONG_CXX_WFLAGS)
gdal_standard_includes(ogr_PMTiles)
target_include_directories(ogr_PMTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mvt)

# Needed for Create() mode
if (GDAL_USE_GEOS)
  target_compile_definitions(ogr_PMTiles PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(ogr_PMTiles PRIVATE ${GEOS_TARGET})
endif ()

# Needed for Create() mode
if (GDAL_USE_SQLITE3)
  target_compile_definitions(ogr_PMTiles PRIVATE -DHAVE_SQLITE)
  gdal_target_link_libraries(ogr_PMTiles PRIVATE SQLite::SQLite3)
endif ()
