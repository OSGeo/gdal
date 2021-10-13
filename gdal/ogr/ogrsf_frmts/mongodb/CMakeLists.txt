find_package(PkgConfig REQUIRED)
pkg_check_modules(MONGO libmongo-client)
add_gdal_driver(TARGET ogr_MongoDB SOURCES ogrmongodbdriver.cpp)
gdal_standard_includes(ogr_MongoDB)
target_include_directories(ogr_MongoDB PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR> ${MONGO_INCLUDE_DIRS}
                                               ${Boost_INCLUDE_DIRS})
gdal_target_link_libraries(ogr_MongoDB LIBRARIES ${MONGO_LIBRARIES} Boost::thread Boost::system Boost::regex)
