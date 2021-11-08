# HDF5, BAG and HDF5Image
set(SOURCE
    hdf5dataset.h
    iso19115_srs.h
    gh5_convenience.h
    hdf5dataset.cpp
    hdf5imagedataset.cpp
    gh5_convenience.cpp
    iso19115_srs.cpp
    bagdataset.cpp
    hdf5multidim.cpp)

add_gdal_driver(TARGET gdal_HDF5 SOURCES ${SOURCE} PLUGIN_CAPABLE)

# When build as plugin, initialize all drivers from Register_HDF5
if (GDAL_ENABLE_FRMT_HDF5_PLUGIN)
  target_compile_definitions(gdal_HDF5 PRIVATE -DHDF5_PLUGIN)
endif ()
if (DEFINED HDF5_DEFINITIONS)
  target_compile_definitions(gdal_HDF5 PRIVATE ${HDF5_DEFINITIONS})
endif ()
gdal_standard_includes(gdal_HDF5)
if (WIN32)
  target_compile_definitions(gdal_HDF5 PRIVATE -DWIN32)
endif ()
target_include_directories(gdal_HDF5 SYSTEM PRIVATE ${HDF5_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET gdal_HDF5 LIBRARIES ${HDF5_C_LIBRARIES})

if (HDF5_BUILD_SHARED_LIBS)
  target_compile_definitions(gdal_HDF5 PRIVATE -DH5_BUILT_AS_DYNAMIC_LIB)
else ()
  target_compile_definitions(gdal_HDF5 PRIVATE -DH5_BUILT_AS_STATIC_LIB)
endif ()
