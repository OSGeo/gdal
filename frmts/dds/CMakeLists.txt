add_gdal_driver(TARGET gdal_DDS
                SOURCES ddsdataset.cpp
                CORE_SOURCES ddsdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_DDS)
    return()
endif()

gdal_standard_includes(gdal_DDS)
gdal_target_link_libraries(gdal_DDS PRIVATE CRNLIB::Crnlib)
target_include_directories(gdal_DDS PRIVATE "${Crnlib_INCLUDE_DIR}/crunch")
if (TARGET Threads::Threads)
  gdal_target_link_libraries(gdal_DDS PRIVATE Threads::Threads)
endif ()
