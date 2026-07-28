add_gdal_driver(
  TARGET gdal_LERC
  SOURCES lercdriver.cpp
  CORE_SOURCES
          lercdrivercore.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_LERC_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
)

if(NOT TARGET gdal_LERC)
    if (GDAL_USE_LERC_INTERNAL)
        target_compile_definitions(gdal_LERC_core PRIVATE GDAL_USE_LERC_INTERNAL)
    endif()
    return()
endif()

gdal_standard_includes(gdal_LERC)

if (GDAL_USE_LERC_INTERNAL)
  gdal_add_vendored_lib(gdal_LERC lerc)
  target_compile_definitions(gdal_LERC PRIVATE GDAL_USE_LERC_INTERNAL)
else()
  gdal_target_link_libraries(gdal_LERC PRIVATE LERC::LERC)
endif ()
