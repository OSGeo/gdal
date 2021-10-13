# depend from pg
add_gdal_driver(
  TARGET ogr_PGDump
  SOURCES ogr_pgdump.h ogrpgdumpdriver.cpp ogrpgdumpdatasource.cpp ogrpgdumplayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_PGDump)
