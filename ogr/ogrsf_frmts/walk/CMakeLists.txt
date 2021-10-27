add_gdal_driver(
  TARGET ogr_WALK
  SOURCES ogis_geometry_wkb_struct.h
          ogrwalk.h
          ogrwalkdatasource.cpp
          ogrwalkdriver.cpp
          ogrwalklayer.cpp
          ogrwalkselectlayer.cpp
          ogrwalktablelayer.cpp
          ogrwalktool.cpp
  BUILTIN) # because of PGEO dependency
gdal_standard_includes(ogr_WALK)
target_include_directories(ogr_WALK PRIVATE $<TARGET_PROPERTY:ogr_PGeo,SOURCE_DIR>
                                            $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
target_include_directories(ogr_WALK PRIVATE ${ODBC_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_WALK LIBRARIES ODBC::ODBC ODBC::ODBCINST)
