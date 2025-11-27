add_library(
  gmlutils OBJECT
  gmlfeature.cpp
  gmlfeatureclass.cpp
  gmlpropertydefn.cpp
  gmlutils.cpp
  parsexsd.h
  parsexsd.cpp
  ogrwfsfilter.cpp
)
gdal_standard_includes(gmlutils)
add_dependencies(gmlutils generate_gdal_version_h)
set_property(TARGET gmlutils PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gmlutils>)
target_compile_options(gmlutils PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})

target_compile_definitions(gmlutils PRIVATE OGR_P_WITH_SRS_CACHE)
