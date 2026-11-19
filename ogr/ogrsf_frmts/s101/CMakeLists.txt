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

if (NOT USE_ONLY_EMBEDDED_RESOURCE_FILES)
    set(GDAL_DATA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/101_Feature_Catalogue_2.0.0.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/data/101_Feature_Catalogue_2.0.0.xml.LICENSE
    )
    set_property(
      TARGET ${GDAL_LIB_TARGET_NAME}
      APPEND
      PROPERTY RESOURCE "${GDAL_DATA_FILES}")
endif()
if (EMBED_RESOURCE_FILES)
    add_driver_embedded_resources(ogr_S101 OGR_ENABLE_DRIVER_S101_PLUGIN embedded_resources.c)
endif()
if (USE_ONLY_EMBEDDED_RESOURCE_FILES)
    target_compile_definitions(ogr_S101 PRIVATE USE_ONLY_EMBEDDED_RESOURCE_FILES)
endif()
