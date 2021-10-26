add_gdal_driver(TARGET ogr_OGDI SOURCES ogrogdi.h ogrogdidatasource.cpp ogrogdidriver.cpp ogrogdilayer.cpp
                                        PLUGIN_CAPABLE)
gdal_standard_includes(ogr_OGDI)
gdal_target_link_libraries(TARGET ogr_OGDI LIBRARIES PROJ::PROJ OGDI::OGDI)
