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
  NO_DEPS
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_DGN)

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        LICENSE.TXT
        ${CMAKE_CURRENT_SOURCE_DIR}/data/seed_2d.dgn
        ${CMAKE_CURRENT_SOURCE_DIR}/data/seed_3d.dgn
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_DGN OGR_ENABLE_DRIVER_DGN_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_DGN PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()

if (NOT OGR_ENABLE_DRIVER_DGN_PLUGIN)
  add_executable(dgnwritetest EXCLUDE_FROM_ALL dgnwritetest.cpp)
  target_include_directories(dgnwritetest PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
  target_link_libraries(dgnwritetest PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)

  add_executable(dgndump EXCLUDE_FROM_ALL dgndump.cpp)
  target_include_directories(dgndump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
  target_link_libraries(dgndump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_DGN)
endif ()
