add_gdal_driver(TARGET gdal_MRSID_Lidar SOURCES mg4lidar_headers.h gdal_MG4Lidar.cpp)
gdal_standard_includes(gdal_MRSID_Lidar)
gdal_target_link_libraries(gdal_MRSID_Lidar PRIVATE MRSID::LIDAR)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE gdal_MG4Lidar.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
