# This is common library for other driver. It is conditionally built.
# cf gdal.cmake for when it is built.
add_library(
  gdal_iso8211 OBJECT
  iso8211.h
  ddfmodule.cpp
  ddfutils.cpp
  ddffielddefn.cpp
  ddfrecord.cpp
  ddffield.cpp
  ddfsubfielddefn.cpp)
set_property(TARGET gdal_iso8211 PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gdal_iso8211>)
gdal_standard_includes(gdal_iso8211)

# Because linking an OBJECT library
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
    add_executable(8211dump EXCLUDE_FROM_ALL 8211dump.cpp)
    target_include_directories(8211dump PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
    target_link_libraries(8211dump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_iso8211)

    add_executable(8211view EXCLUDE_FROM_ALL 8211view.cpp)
    target_include_directories(8211view PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
    target_link_libraries(8211view PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_iso8211)

    add_executable(8211createfromxml EXCLUDE_FROM_ALL 8211createfromxml.cpp)
    target_include_directories(8211createfromxml PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
    target_link_libraries(8211createfromxml PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> gdal_iso8211)
endif ()
