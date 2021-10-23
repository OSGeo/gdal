add_gdal_driver(
  TARGET gdal_PCRaster
  SOURCES pcrasterdataset.h
          pcrasterrasterband.h
          pcrasterutil.h
          pcrasterdataset.cpp
          pcrastermisc.cpp
          pcrasterrasterband.cpp
          pcrasterutil.cpp)
gdal_standard_includes(gdal_PCRaster)

if (GDAL_USE_LIBCSF_INTERNAL)
  target_include_directories(gdal_PCRaster PRIVATE $<TARGET_PROPERTY:csf,SOURCE_DIR>)
  target_compile_definitions(gdal_PCRaster PRIVATE -DUSE_IN_GDAL)
else ()
  target_include_directories(gdal_PCRaster PRIVATE ${LIBCSF_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_PCRaster LIBRARIES ${LIBCSF_LIBRARIES})
endif ()
