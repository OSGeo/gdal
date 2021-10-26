add_gdal_driver(
  TARGET gdal_iso8211
  SOURCES iso8211.h
          ddfmodule.cpp
          ddfutils.cpp
          ddffielddefn.cpp
          ddfrecord.cpp
          ddffield.cpp
          ddfsubfielddefn.cpp
  BUILTIN)
gdal_standard_includes(gdal_iso8211)
