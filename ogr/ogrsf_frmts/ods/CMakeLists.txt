add_gdal_driver(
  TARGET ogr_ODS
  SOURCES ods_formula_node.cpp
          ods_formula.cpp
          ogrodsdatasource.cpp
          ogrodsdriver.cpp
          ods_formula_parser.hpp
          ods_formula.h
          ogr_ods.h
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_ODS)
target_include_directories(ogr_ODS PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                           $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
add_executable(testparser EXCLUDE_FROM_ALL testparser.cpp)

if (GDAL_USE_EXPAT)
  gdal_target_link_libraries(ogr_ODS PRIVATE ${EXPAT_LIBRARIES})
  target_compile_definitions(ogr_ODS PRIVATE -DHAVE_EXPAT=1)
endif ()

find_package(BISON)
if (BISON_FOUND)
  add_custom_command(
    OUTPUT ods_formula_parser.cpp
    COMMAND ${BISON_EXECUTABLE} -p ods_formula -d -o${CMAKE_CURRENT_BINARY_DIR}/ods_formula_parser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ods_formula_parser.y
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ods_formula_parser.y)
endif ()
