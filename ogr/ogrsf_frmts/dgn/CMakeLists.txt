add_gdal_driver(
  TARGET ogr_DGN
  SOURCES dgnhelp.cpp
          dgnopen.cpp
          dgnread.cpp
          dgnwrite.cpp
          dgnstroke.cpp
          ogrdgndriver.cpp
          ogrdgnlayer.cpp
          ogrdgndatasource.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_DGN)
add_executable(dgnwritetest EXCLUDE_FROM_ALL dgnwritetest.cpp)
add_executable(dgndump EXCLUDE_FROM_ALL dgndump.cpp)
