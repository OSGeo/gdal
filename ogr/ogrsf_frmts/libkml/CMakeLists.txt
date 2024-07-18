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
  CORE_SOURCES
          ogrlibkmldrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_LIBKML)
    return()
endif()

gdal_standard_includes(ogr_LIBKML)
gdal_target_link_libraries(ogr_LIBKML PRIVATE LIBKML::LibKML LIBKML::DOM LIBKML::ENGINE)
if (TARGET LIBKML::MINIZIP AND TARGET LIBKML::URIPARSER AND TARGET ZLIB::ZLIB AND GDAL_USE_EXPAT)
  # Fedora Rawhide doesn't have LIBKML::MINIZIP
  gdal_target_link_libraries(ogr_LIBKML PRIVATE LIBKML::MINIZIP LIBKML::URIPARSER ZLIB::ZLIB ${EXPAT_TARGET})
endif()
