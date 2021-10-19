add_gdal_driver(
  TARGET gdal_MSGN
  SOURCES msg_basic_types.cpp
          msg_basic_types.h
          msg_reader_core.cpp
          msg_reader_core.h
          msgndataset.cpp
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_MSGN)
target_compile_definitions(gdal_MSGN PRIVATE -DGDAL_SUPPORT)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE msg_basic_types.cpp msgndataset.cpp msg_reader_core.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
