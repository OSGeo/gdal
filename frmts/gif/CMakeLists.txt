add_gdal_driver(
  TARGET gdal_GIF
  SOURCES gifabstractdataset.h gifabstractdataset.cpp biggifdataset.cpp gifdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_GIF)
target_compile_definitions(gdal_GIF PRIVATE -DHAVE_GIF)

if (GDAL_USE_GIF_INTERNAL)
  gdal_add_vendored_lib(gdal_GIF giflib)
else ()
  gdal_target_link_libraries(gdal_GIF PRIVATE GIF::GIF)
endif ()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE gifdataset.cpp gifabstractdataset.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
