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

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  if (NOT GDAL_ENABLE_FRMT_DTED_PLUGIN)
    add_executable(dted_test EXCLUDE_FROM_ALL dted_test.c)
    target_include_directories(dted_test PRIVATE $<TARGET_PROPERTY:gdal_DTED,SOURCE_DIR>)
    target_link_libraries(dted_test PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_DTED)
  endif ()
endif ()
