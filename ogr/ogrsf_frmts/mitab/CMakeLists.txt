# referenced from frmts/northwood driver
add_gdal_driver(
  TARGET ogr_TAB
  DEF MITAB_ENABLED
  PLUGIN_CAPABLE
  NO_DEPS
  SOURCES mitab_rawbinblock.cpp
          mitab_mapheaderblock.cpp
          mitab_mapindexblock.cpp
          mitab_mapobjectblock.cpp
          mitab_mapcoordblock.cpp
          mitab_feature.cpp
          mitab_feature_mif.cpp
          mitab_mapfile.cpp
          mitab_idfile.cpp
          mitab_datfile.cpp
          mitab_tabfile.cpp
          mitab_miffile.cpp
          mitab_utils.cpp
          mitab_imapinfofile.cpp
          mitab_middatafile.cpp
          mitab_maptoolblock.cpp
          mitab_tooldef.cpp
          mitab_ogr_driver.cpp
          mitab_indfile.cpp
          mitab_tabview.cpp
          mitab_ogr_datasource.cpp
          mitab_geometry.cpp
          mitab_tabseamless.cpp)
gdal_standard_includes(ogr_TAB)
target_compile_definitions(ogr_TAB PRIVATE -DMITAB_USE_OFTDATETIME)
