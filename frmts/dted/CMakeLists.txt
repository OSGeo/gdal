add_gdal_driver(
  TARGET gdal_DTED
  SOURCES dted_api.h
          dted_api.c
          dted_create.c
          dted_ptstream.c
          dteddataset.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_DTED)

add_executable(dted_test EXCLUDE_FROM_ALL dted_test.c)
target_link_libraries(dted_test PRIVATE gdal)
