# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

add_library(gnm_frmts OBJECT gnm_frmts.h gnmregisterall.cpp)
gdal_standard_includes(gnm_frmts)
add_dependencies(gnm_frmts generate_gdal_version_h)
target_include_directories(gnm_frmts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
target_compile_definitions(gnm_frmts PRIVATE -DGNMFILE_ENABLED -DGNMDB_ENABLED)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gnm_frmts>)
set_property(TARGET gnm_frmts PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
add_subdirectory(db)
add_subdirectory(file)
