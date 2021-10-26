add_gdal_driver(
  TARGET ogr_NTF
  SOURCES ntf.h
          ntffilereader.cpp
          ntfrecord.cpp
          ogrntfdatasource.cpp
          ogrntfdriver.cpp
          ogrntflayer.cpp
          ntf_estlayers.cpp
          ogrntffeatureclasslayer.cpp
          ntf_generic.cpp
          ntf_raster.cpp
          ntf_codelist.cpp
          ntfstroke.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_NTF)
add_executable(ntfdump EXCLUDE_FROM_ALL ntfdump.cpp)
