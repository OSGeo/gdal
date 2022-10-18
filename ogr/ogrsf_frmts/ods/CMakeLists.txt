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
  gdal_target_link_libraries(ogr_ODS PRIVATE ${EXPAT_TARGET})
  target_compile_definitions(ogr_ODS PRIVATE -DHAVE_EXPAT=1)
endif ()

###########################################################
# Targets to refresh ods_formula_parser.cpp
###########################################################

# Those targets need to be run manually each time ods_formula_parser.y
# is modified.
# We could of course run them automatically, but that would make building
# GDAL harder.

# This target checks that ods_formula_parser.y md5sum has not changed
# If it has, then it should be updated and the generate_ods_formula_parser target
# should be manually run
add_custom_target(check_ods_formula_parser_md5 ALL
                  COMMAND ${CMAKE_COMMAND}
                      "-DIN_FILE=ods_formula_parser.y"
                      "-DTARGET=generate_ods_formula_parser"
                      "-DEXPECTED_MD5SUM=acf825ac2d574098a64c74c864aee15e"
                      "-DFILENAME_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
                      -P "${PROJECT_SOURCE_DIR}/cmake/helpers/check_md5sum.cmake"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ods_formula_parser.y"
                  VERBATIM)

find_package(BISON)
add_custom_target(generate_ods_formula_parser
                  COMMAND ${CMAKE_COMMAND}
                      "-DBISON_FOUND=${BISON_FOUND}"
                      "-DBISON_EXECUTABLE=${BISON_EXECUTABLE}"
                      -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_ods_formula_parser.cmake"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  VERBATIM)
