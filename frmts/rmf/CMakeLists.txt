add_gdal_driver(
  TARGET gdal_RMF
  SOURCES rmfdataset.h
          rmfdataset.cpp
          rmfdem.cpp
          rmflzw.cpp
          rmfjpeg.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_RMF)

if (HAVE_JPEG OR GDAL_USE_JPEG_INTERNAL)
  target_compile_definitions(gdal_RMF PRIVATE -DHAVE_LIBJPEG)
endif ()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE rmfdataset.cpp rmfdem.cpp rmflzw.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
