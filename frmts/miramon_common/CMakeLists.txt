# This is common library for other driver. It is conditionally built.
# cf gdal.cmake for when it is built.
add_library(
  miramon_common OBJECT
  mm_gdal_functions.h
  mm_gdal_constants.h
  mm_gdal_driver_structs.h
  mm_gdal_structures.h
  mm_constants.h
  mm_gdal_functions.c)
add_dependencies(miramon_common generate_gdal_version_h)
set_property(TARGET miramon_common PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:miramon_common>)
gdal_standard_includes(miramon_common)

# Because linking an OBJECT library
add_executable(mm_gdal_functions EXCLUDE_FROM_ALL mm_gdal_functions.c)
target_include_directories(mm_gdal_functions PRIVATE $<TARGET_PROPERTY:miramon_common,SOURCE_DIR>)
target_link_libraries(mm_gdal_functions PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> miramon_common)

