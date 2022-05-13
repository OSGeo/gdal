add_gdal_driver(
  TARGET ogr_LIBKML
  SOURCES libkml_headers.h
          ogrlibkmlfield.h
          ogr_libkml.h
          ogrlibkmlfeaturestyle.h
          ogrlibkmlfeature.h
          ogrlibkmlgeometry.h
          ogrlibkmlstyle.h
          ogrlibkmldriver.cpp
          ogrlibkmlfeaturestyle.cpp
          ogrlibkmllayer.cpp
          ogrlibkmlfeature.cpp
          ogrlibkmlgeometry.cpp
          ogrlibkmlstyle.cpp
          ogrlibkmldatasource.cpp
          ogrlibkmlfield.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_LIBKML)
gdal_target_link_libraries(ogr_LIBKML PRIVATE LIBKML::LibKML LIBKML::DOM LIBKML::ENGINE)
