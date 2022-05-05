add_library(gnm_frmts_file OBJECT gnmfile.h gnmfiledriver.cpp gnmfilenetwork.cpp)
gdal_standard_includes(gnm_frmts_file)
add_dependencies(gnm_frmts_file generate_gdal_version_h)
target_compile_options(gnm_frmts PRIVATE ${GDAL_CXX_WARNING_FLAGS})
target_include_directories(gnm_frmts_file PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>
                                                  $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR>)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gnm_frmts_file>)
set_property(TARGET gnm_frmts_file PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
