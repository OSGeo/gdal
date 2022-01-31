# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.
add_library(
  gcore_mdreader OBJECT
  reader_alos.h
  reader_digital_globe.h
  reader_eros.h
  reader_geo_eye.h
  reader_kompsat.h
  reader_landsat.h
  reader_orb_view.h
  reader_pleiades.h
  reader_rapid_eye.h
  reader_rdk1.h
  reader_spot.h
  reader_alos.cpp
  reader_digital_globe.cpp
  reader_eros.cpp
  reader_geo_eye.cpp
  reader_kompsat.cpp
  reader_landsat.cpp
  reader_orb_view.cpp
  reader_pleiades.cpp
  reader_rapid_eye.cpp
  reader_rdk1.cpp
  reader_spot.cpp)
gdal_standard_includes(gcore_mdreader)
add_dependencies(gcore_mdreader generate_gdal_version_h)
target_include_directories(gcore_mdreader PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gcore_mdreader>)
set_property(TARGET gcore_mdreader PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE reader_pleiades.cpp reader_digital_globe.cpp reader_alos.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION
                                                                                            ON)
endif ()
