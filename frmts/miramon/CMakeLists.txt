# depend from gcore
add_gdal_driver(
  TARGET gdal_MiraMon
  SOURCES miramon_band.h
          miramon_rasterband.h
          miramon_rel.h
          miramon_dataset.h
          miramon_palettes.h
          miramon_band.cpp
          miramon_rasterband.cpp
          miramon_rel.cpp
          miramon_dataset.cpp
          miramon_palettes.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)
gdal_standard_includes(gdal_MiraMon)

# Because linking an OBJECT library
#add_executable(mm_gdal_functions EXCLUDE_FROM_ALL mm_gdal_functions.cpp)
#target_include_directories(mm_gdal_functions PRIVATE $<TARGET_PROPERTY:miramon_common,SOURCE_DIR>)
#target_link_libraries(mm_gdal_functions PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> miramon_common)
