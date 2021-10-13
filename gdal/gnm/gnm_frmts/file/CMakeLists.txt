add_library(gnm_frmts_file OBJECT gnmfile.h gnmfiledriver.cpp gnmfilenetwork.cpp)
gdal_standard_includes(gnm_frmts_file)
target_include_directories(gnm_frmts_file PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>
                                                  $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR>)
target_sources(gdal PRIVATE $<TARGET_OBJECTS:gnm_frmts_file>)
set_property(TARGET gnm_frmts_file PROPERTY POSITION_INDEPENDENT_CODE ON)
