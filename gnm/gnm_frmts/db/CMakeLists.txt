# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

add_library(gnm_frmts_db OBJECT gnmdb.h gnmdbdriver.cpp gnmdbnetwork.cpp)
gdal_standard_includes(gnm_frmts_db)
target_include_directories(gnm_frmts_db PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>
                                                $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR>)
target_sources(gdal PRIVATE $<TARGET_OBJECTS:gnm_frmts_db>)
set_property(TARGET gnm_frmts_db PROPERTY POSITION_INDEPENDENT_CODE ON)
