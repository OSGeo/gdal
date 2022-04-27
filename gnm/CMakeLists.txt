# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

if (ENABLE_GNM)
  add_subdirectory(gnm_frmts)
  add_library(
    gnm OBJECT
    gnm_priv.h
    gnmlayer.cpp
    gnmgenericnetwork.cpp
    gnmgraph.cpp
    gnmnetwork.cpp
    gnmresultlayer.cpp
    gnmrule.cpp)
  include(TargetPublicHeader)
  target_public_header(TARGET gdal HEADERS gnm.h gnm_api.h gnmgraph.h)
  gdal_standard_includes(gnm)
  add_dependencies(gnm generate_gdal_version_h)
  target_compile_options(gnm PRIVATE ${GDAL_CXX_WARNING_FLAGS})
  target_include_directories(gnm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gnm_frmts)
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gnm>)
  set_property(TARGET gnm PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
endif ()
