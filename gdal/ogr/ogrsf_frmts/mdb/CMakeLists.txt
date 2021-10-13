add_gdal_driver(TARGET ogr_MDB SOURCES ogr_mdb.h ogrmdbdatasource.cpp ogrmdbdriver.cpp ogrmdbjackcess.cpp
                                       ogrmdblayer.cpp)
gdal_standard_includes(ogr_MDB)
target_include_directories(ogr_MDB PRIVATE ${JNI_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_MDB LIBRARIES ${JNI_LIBRARIES})
