# gdal core depends on kml driver, so it should not be a plugin.
add_gdal_driver(
  TARGET ogr_KML
  SOURCES kmlutility.h ogr_kml.h ogrkmldriver.cpp ogr2kmlgeometry.cpp ogrkmldatasource.cpp ogrkmllayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_KML)

if (GDAL_USE_EXPAT)
  target_sources(ogr_KML PRIVATE kml.cpp kmlnode.cpp kmlvector.h kml.h kmlnode.h kmlvector.cpp)
  target_compile_definitions(ogr_KML PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_KML PRIVATE ${EXPAT_LIBRARIES})
endif ()
