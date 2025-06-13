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
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_HFA)

gdal_target_link_libraries(gdal_HFA PRIVATE PROJ::proj)

if (NOT GDAL_ENABLE_DRIVER_HFA_PLUGIN)
    add_executable(hfatest EXCLUDE_FROM_ALL hfatest.cpp ../../ogr/ogr_proj_p.cpp)
    gdal_standard_includes(hfatest)
    target_link_libraries(hfatest PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_HFA PROJ::proj)
endif()
