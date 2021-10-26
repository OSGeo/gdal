# FIXME currently only version 7 is supported
set(GRASS_VERSION 7)
add_gdal_driver(TARGET ogr_GRASS SOURCES ogrgrass.h ogrgrassdatasource.cpp ogrgrassdriver.cpp ogrgrasslayer.cpp)
gdal_standard_includes(ogr_GRASS)
target_compile_definitions(ogr_GRASS PRIVATE -DGRASS_GISBASE="${GRASS_PREFIX${GRASS_VERSION}}"
                                             -DGRASS_VERSION_MAJOR=${GRASS_VERSION})

target_compile_definitions(ogr_GRASS PRIVATE -DUSE_CPL) # is this needed ?

target_include_directories(ogr_GRASS PRIVATE ${GRASS_INCLUDE_DIR${GRASS_VERSION}} ${PostgreSQL_INCLUDE_DIRS})
gdal_target_link_libraries(
  TARGET ogr_GRASS
  LIBRARIES -L${GRASS_PREFIX${GRASS_VERSION}}/lib
            -lgrass_vector
            -lgrass_dig2
            -lgrass_dgl
            -lgrass_rtree
            -lgrass_linkm
            -lgrass_dbmiclient
            -lgrass_dbmibase
            -lgrass_gproj
            -lgrass_gmath
            -lgrass_gis
            -lgrass_datetime
            ${PostgreSQL_LIBRARIES})
