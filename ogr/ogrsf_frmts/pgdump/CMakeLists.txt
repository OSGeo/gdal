# depend from pg
add_gdal_driver(
  TARGET ogr_PGDump
  SOURCES ogr_pgdump.h ogrpgdumpdriver.cpp ogrpgdumpdatasource.cpp ogrpgdumplayer.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_PGDump)
