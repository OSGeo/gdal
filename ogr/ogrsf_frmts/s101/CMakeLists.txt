add_gdal_driver(
  TARGET ogr_S101
  SOURCES ogrs101dataset.cpp
          ogrs101driver.cpp
          ogrs101featurecatalog.cpp
          ogrs101layer.cpp
          ogrs101layercompositecurve.cpp
          ogrs101layercurve.cpp
          ogrs101layerfeaturetype.cpp
          ogrs101layerinformationtype.cpp
          ogrs101layermultipoint.cpp
          ogrs101layerpoint.cpp
          ogrs101layersurface.cpp
          ogrs101reader.cpp
          ogrs101readerattributes.cpp
          ogrs101readercompositecurve.cpp
          ogrs101readercrs.cpp
          ogrs101readercurve.cpp
          ogrs101readerdatasetgeneralinformationrecord.cpp
          ogrs101readerfeaturetype.cpp
          ogrs101readerinformationtype.cpp
          ogrs101readermultipoint.cpp
          ogrs101readerpoint.cpp
          ogrs101readersurface.cpp
  CORE_SOURCES
          ogrs101drivercore.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_SHARED_SYMBOL_WITH_CORE
)
target_include_directories(ogr_S101 PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
gdal_standard_includes(ogr_S101)
