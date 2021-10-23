# HDF4 and HDF4Image driver
option(GDAL_ENABLE_FRMT_HDF4_PLUGIN "build HDF4 driver as plugin" ON)
set(_SOURCES
    hdf-eos/ease.h
    hdf-eos/HdfEosDef.h
    hdf-eos/HDFEOSVersion.h
    hdf-eos/EHapi.c
    hdf-eos/GDapi.c
    hdf-eos/gctp_wrap.c
    hdf-eos/SWapi.c
    hdf4compat.h
    hdf4dataset.h
    hdf4dataset.cpp
    hdf4imagedataset.cpp
    hdf4multidim.cpp)
if (GDAL_ENABLE_FRMT_HDF4_PLUGIN)
  add_gdal_driver(
    TARGET gdal_HDF4
    SOURCES ${_SOURCES}
    PLUGIN)
else ()
  add_gdal_driver(TARGET gdal_HDF4 SOURCES ${_SOURCES})
endif ()
unset(_SOURCES)
if (GDAL_ENABLE_PLUGIN OR GDAL_ENABLE_FRMT_HDF4_PLUGIN)
  # When build as plugin, it initialize HDF4Image driver too.
  target_compile_definitions(gdal_HDF4 PRIVATE -DHDF4_PLUGIN)
endif ()

gdal_standard_includes(gdal_HDF4)
target_include_directories(
  gdal_HDF4 PRIVATE $<TARGET_PROPERTY:gdal_PDS,SOURCE_DIR> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hdf-eos>)
if (HDF4_HAS_MAXOPENFILES)
  target_compile_definitions(gdal_HDF4 PRIVATE -DHDF4_HAS_MAXOPENFILES)
endif ()

gdal_target_link_libraries(TARGET gdal_HDF4 LIBRARIES HDF4::HDF4 HDF4::MFHDF)
