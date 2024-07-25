add_gdal_driver(
  TARGET ogr_MySQL
  SOURCES ogr_mysql.h
          ogrmysqldatasource.cpp
          ogrmysqldriver.cpp
          ogrmysqllayer.cpp
          ogrmysqlresultlayer.cpp
          ogrmysqltablelayer.cpp
  CORE_SOURCES
          ogrmysqldrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_MySQL)
    return()
endif()

gdal_standard_includes(ogr_MySQL)
target_include_directories(ogr_MySQL PRIVATE ${MYSQL_INCLUDE_DIRS})
target_compile_definitions(ogr_MySQL PRIVATE -DHAVE_MYSQL)
gdal_target_link_libraries(ogr_MySQL PRIVATE ${MYSQL_LIBRARIES})
