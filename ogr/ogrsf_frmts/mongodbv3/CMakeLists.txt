add_gdal_driver(TARGET ogr_MongoDBv3 SOURCES ogrmongodbv3driver.cpp NO_CXX_WFLAGS PLUGIN_CAPABLE)
gdal_standard_includes(ogr_MongoDBv3)
set(GDAL_CXX_WARNING_FLAGS_WITHOUT_NO_OPERATOR_NAMES "${GDAL_CXX_WARNING_FLAGS}")
list(REMOVE_ITEM GDAL_CXX_WARNING_FLAGS_WITHOUT_NO_OPERATOR_NAMES "-fno-operator-names")
target_compile_options(ogr_MongoDBv3
                       PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS_WITHOUT_NO_OPERATOR_NAMES}>)
target_include_directories(ogr_MongoDBv3 PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_target_link_libraries(ogr_MongoDBv3 PRIVATE MONGOCXX::MONGOCXX MONGOCXX::BSONCXX)
