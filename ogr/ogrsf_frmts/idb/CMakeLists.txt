add_gdal_driver(TARGET ogr_IDB SOURCES ogr_idb.h ogridbdatasource.cpp ogridbdriver.cpp ogridblayer.cpp
                                       ogridbselectlayer.cpp ogridbtablelayer.cpp)
gdal_standard_includes(ogr_IDB)
target_compile_definitions(ogr_IDB PRIVATE -DIT_DLLIB -DIT_DO_NOT_SIMULATE_BOOL)
target_include_directories(ogr_IDB PRIVATE ${IDB_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_IDB LIBRARIES ${IDB_LIBRARIES})
