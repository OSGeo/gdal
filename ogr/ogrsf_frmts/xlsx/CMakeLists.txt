add_gdal_driver(TARGET ogr_XLSX SOURCES ogr_xlsx.h ogrxlsxdatasource.cpp ogrxlsxdriver.cpp
                PLUGIN_CAPABLE
                NO_DEPS
                NO_CXX_WFLAGS_EFFCXX
                NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_XLSX)
if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_XLSX PRIVATE -DHAVE_EXPAT)
  gdal_target_link_libraries(ogr_XLSX PRIVATE ${EXPAT_TARGET})
endif ()
