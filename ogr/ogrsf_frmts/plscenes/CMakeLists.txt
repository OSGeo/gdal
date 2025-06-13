add_gdal_driver(TARGET ogr_PLSCENES
                SOURCES
                    ogr_plscenes.h
                    ogrplscenesdatav1dataset.cpp
                    ogrplscenesdatav1layer.cpp
                    ogrplscenesdriver.cpp
                CORE_SOURCES
                    ogrplscenesdrivercore.cpp
                PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL"
                NO_SHARED_SYMBOL_WITH_CORE
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
if(NOT TARGET ogr_PLSCENES)
    return()
endif()

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/plscenesconf.json
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()

if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_PLSCENES OGR_ENABLE_DRIVER_PLSCENES_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_PLSCENES PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()

gdal_standard_includes(ogr_PLSCENES)

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_PLSCENES libjson)
else ()
  gdal_target_link_libraries(ogr_PLSCENES PRIVATE ${JSONC_TARGET})
endif ()
