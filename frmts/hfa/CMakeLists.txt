# depend from gcore
add_gdal_driver(
  TARGET gdal_HFA
  SOURCES hfa.h
          hfa_p.h
          hfadataset.h
          hfaband.cpp
          hfacompress.cpp
          hfadataset.cpp
          hfadictionary.cpp
          hfaentry.cpp
          hfafield.cpp
          hfaopen.cpp
          hfatype.cpp
          hfa_overviews.cpp
  BUILTIN)
gdal_standard_includes(gdal_HFA)
target_include_directories(gdal_HFA PRIVATE $<TARGET_PROPERTY:PROJ::proj,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(gdal_HFA PRIVATE $<TARGET_PROPERTY:PROJ::proj,INTERFACE_COMPILE_DEFINITIONS>)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  add_executable(hfatest EXCLUDE_FROM_ALL hfatest.cpp ../../ogr/ogr_proj_p.cpp)
  gdal_standard_includes(hfatest)
  target_link_libraries(hfatest PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_HFA PROJ::proj)
endif ()
