add_gdal_driver(TARGET gdal_PCIDSK SOURCES pcidskdataset2.h pcidskdataset2.cpp gdal_edb.cpp vsi_pcidsk_io.cpp
                                           ogrpcidsklayer.cpp)
gdal_standard_includes(gdal_PCIDSK)

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(gdal_PCIDSK PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
else ()
  target_include_directories(gdal_PCIDSK PRIVATE ${JPEG_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_PCIDSK LIBRARIES JPEG::JPEG)
endif ()

if (GDAL_USE_LIBPCIDSK_INTERNAL)
  target_include_directories(gdal_PCIDSK PRIVATE $<TARGET_PROPERTY:pcidsk_sdk,SOURCE_DIR>)
else ()
  target_include_directories(gdal_PCIDSK PRIVATE ${PCIDSK_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_PCIDSK LIBRARIES PCIDSK::PCIDSK)
endif ()
