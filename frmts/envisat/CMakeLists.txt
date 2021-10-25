add_gdal_driver(
  TARGET gdal_Envisat
  SOURCES EnvisatFile.h
          records.h
          adsrange.hpp
          timedelta.hpp
          EnvisatFile.c
          records.c
          adsrange.cpp
          unwrapgcps.cpp
          envisatdataset.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_Envisat)
target_include_directories(gdal_Envisat PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/raw)

add_executable(dumpgeo EXCLUDE_FROM_ALL dumpgeo.c EnvisatFile.c EnvisatFile.h)
target_include_directories(dumpgeo PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR>)
target_link_libraries(dumpgeo PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> m dl)
add_executable(envisat_dump EXCLUDE_FROM_ALL envisat_dump.c EnvisatFile.c EnvisatFile.h)
target_link_libraries(envisat_dump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> m dl)
