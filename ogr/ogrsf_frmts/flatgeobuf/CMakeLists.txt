add_gdal_driver(
  TARGET ogr_FlatGeobuf
  SOURCES ogrflatgeobufdataset.cpp
          ogrflatgeobuflayer.cpp
          packedrtree.cpp
          geometryreader.cpp
          geometrywriter.cpp
          ogrflatgeobufeditablelayer.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_FlatGeobuf)
target_include_directories(ogr_FlatGeobuf PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                                  $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
