if (NOT WIN32)
  set(Oracle_CAN_USE_CLNTSH_AS_MAIN_LIBRARY ON)
endif()
gdal_check_package(Oracle "Enable Oracle OCI and GeoRaster drivers" CAN_DISABLE)
