# Cannot be plugin due to dependency to symbols in mvt/
add_gdal_driver(TARGET ogr_PMTiles
                SOURCES
                  ogrpmtilesdriver.cpp
                  ogrpmtilesdataset.cpp
                  ogrpmtilesvectorlayer.cpp
                  ogrpmtilestileiterator.cpp
                  ogrpmtilesfrommbtiles.cpp
                  vsipmtiles.cpp
                BUILTIN
                STRONG_CXX_WFLAGS)
gdal_standard_includes(ogr_PMTiles)
target_include_directories(ogr_PMTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mvt)
