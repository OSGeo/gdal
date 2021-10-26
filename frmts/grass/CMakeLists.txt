# FIXME: grass_version
set(GRASS_VERSION 7)
add_gdal_driver(TARGET gdal_GRASS SOURCES grass57dataset.cpp)
gdal_standard_includes(gdal_GRASS)
target_include_directories(gdal_GRASS PRIVATE ${GRASS_INCLUDE_DIR${GRASS_VERSION}})
gdal_target_link_libraries(
  TARGET gdal_GRASS
  LIBRARIES -L${GRASS_PREFIX${GRASS_VERSION}}/lib/
            -lgrass_raster
            -lgrass_imagery
            -lgrass_gproj
            -lgrass_gmath
            -lgrass_gis
            -lgrass_datetime)
target_compile_definitions(gdal_GRASS PRIVATE -DGRASS_VERSION_MAJOR=${GRASS_VERSION}
                                              -DGRASS_GISBASE="${GRASS_PREFIX${GRASS_VERSION}}")
