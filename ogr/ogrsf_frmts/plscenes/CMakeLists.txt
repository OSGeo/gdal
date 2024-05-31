add_gdal_driver(TARGET ogr_PLSCENES
                SOURCES
                    ogr_plscenes.h
                    ogrplscenesdatav1dataset.cpp
                    ogrplscenesdatav1layer.cpp
                    ogrplscenesdriver.cpp
                CORE_SOURCES
                    ogrplscenesdrivercore.cpp
                PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL"
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_PLSCENES)
    return()
endif()

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/plscenesconf.json
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

gdal_standard_includes(ogr_PLSCENES)
target_include_directories(ogr_PLSCENES PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_PLSCENES libjson)
else ()
  gdal_target_link_libraries(ogr_PLSCENES PRIVATE ${JSONC_TARGET})
endif ()
