# depends from ogr/sdts
add_gdal_driver(
  TARGET gdal_SDTS
  SOURCES sdts_al.h
          sdtsiref.cpp
          sdtscatd.cpp
          sdtslinereader.cpp
          sdtslib.cpp
          sdtspointreader.cpp
          sdtsattrreader.cpp
          sdtstransfer.cpp
          sdtspolygonreader.cpp
          sdtsxref.cpp
          sdtsrasterreader.cpp
          sdtsindexedreader.cpp
          sdtsdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_SDTS)
target_include_directories(gdal_SDTS PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)

if (BROKEN)
  add_executable(sdts2shp EXCLUDE_FROM_ALL sdts2shp.cpp)
  gdal_standard_includes(sdts2shp)
  target_include_directories(sdts2shp PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>
                                              $<TARGET_PROPERTY:ogr_Shape,SOURCE_DIR>)
  target_link_libraries(sdts2shp PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_SDTS ogr_Shape gdal_iso8211)
endif ()
