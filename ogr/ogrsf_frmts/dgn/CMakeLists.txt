add_gdal_driver(
  TARGET ogr_DGN
  SOURCES dgnhelp.cpp
          dgnopen.cpp
          dgnread.cpp
          dgnwrite.cpp
          dgnstroke.cpp
          ogrdgndriver.cpp
          ogrdgnlayer.cpp
          ogrdgndatasource.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_DGN)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  if (NOT OGR_ENABLE_DGN_PLUGIN)
    add_executable(dgnwritetest EXCLUDE_FROM_ALL dgnwritetest.cpp)
    target_include_directories(dgnwritetest PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
    target_link_libraries(dgnwritetest PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)

    add_executable(dgndump EXCLUDE_FROM_ALL dgndump.cpp)
    target_include_directories(dgndump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
    target_link_libraries(dgndump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)
  endif ()
endif ()
