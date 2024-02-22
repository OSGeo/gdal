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

set(GDAL_DATA_FILES
    LICENSE.TXT
    ${CMAKE_CURRENT_SOURCE_DIR}/data/seed_2d.dgn
    ${CMAKE_CURRENT_SOURCE_DIR}/data/seed_3d.dgn
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if (NOT OGR_ENABLE_DRIVER_DGN_PLUGIN)
  add_executable(dgnwritetest EXCLUDE_FROM_ALL dgnwritetest.cpp)
  target_include_directories(dgnwritetest PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
  target_link_libraries(dgnwritetest PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)

  add_executable(dgndump EXCLUDE_FROM_ALL dgndump.cpp)
  target_include_directories(dgndump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
  target_link_libraries(dgndump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)
endif ()
