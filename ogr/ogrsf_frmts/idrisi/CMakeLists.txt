add_gdal_driver(
  TARGET ogr_Idrisi
  SOURCES ogr_idrisi.h ogridrisidatasource.cpp ogridrisidriver.cpp ogridrisilayer.cpp
  BUILTIN # Because of GDAL IDRISI dependency
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)

target_include_directories(ogr_Idrisi PRIVATE $<TARGET_PROPERTY:gdal_IDRISI,SOURCE_DIR>)
gdal_standard_includes(ogr_Idrisi)
