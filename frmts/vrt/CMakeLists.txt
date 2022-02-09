add_gdal_driver(
  TARGET gdal_vrt
  BUILTIN
  SOURCES gdal_vrt.h
          vrtdataset.h
          vrtderivedrasterband.cpp
          vrtdriver.cpp
          vrtfilters.cpp
          vrtrasterband.cpp
          vrtrawrasterband.cpp
          vrtsourcedrasterband.cpp
          vrtsources.cpp
          vrtwarped.cpp
          vrtdataset.cpp
          pixelfunctions.cpp
          vrtpansharpened.cpp
          vrtmultidim.cpp
          STRONG_CXX_WFLAGS)
gdal_standard_includes(gdal_vrt)
target_include_directories(gdal_vrt PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/raw)

if (GDAL_USE_GEOS)
  gdal_target_link_libraries(gdal_vrt PRIVATE ${GEOS_LIBRARIES})
  target_compile_definitions(gdal_vrt PRIVATE -DHAVE_GEOS=1)
endif ()

include(TargetPublicHeader)
target_public_header(TARGET gdal HEADERS vrtdataset.h gdal_vrt.h)
