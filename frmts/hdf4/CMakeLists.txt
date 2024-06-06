# HDF4 and HDF4Image driver
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

set_property(SOURCE hdf-eos/EHapi.c hdf-eos/GDapi.c hdf-eos/SWapi.c PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

add_gdal_driver(TARGET gdal_HDF4
                SOURCES ${_SOURCES}
                CORE_SOURCES hdf4drivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)
unset(_SOURCES)

if(NOT TARGET gdal_HDF4)
    return()
endif()

if (GDAL_ENABLE_DRIVER_HDF4_PLUGIN)
  # When build as plugin, it initialize HDF4Image driver too.
  target_compile_definitions(gdal_HDF4 PRIVATE -DHDF4_PLUGIN)
endif ()

gdal_standard_includes(gdal_HDF4)
target_include_directories(gdal_HDF4 PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hdf-eos>)


# HDF4 library newer than 4.2.5 has a SDreset_maxopenfiles/SDget_maxopenfiles interface
# which allows opening many HDF files simultaneously (the max number of files was previously
# hardcoded and too low, smth. like 32). Search for it and use if available.
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES ${HDF4_LIBRARIES})
set(CMAKE_REQUIRED_INCLUDES ${HDF4_INCLUDE_DIRS})
# We need to use symbols of both libdf (Hclose()) and libmfhdf (SDget_maxopenfiles())
# otherwise the linking fails due to libmfhdf not explicitly linking to
# libdf, but still requiring it...
check_c_source_compiles(
"
    #include <hdf.h>
    #include <mfhdf.h>
    intn nCurrMax = 0;
    intn nSysLimit = 0;
    int main() { Hclose(0); return SDget_maxopenfiles(&nCurrMax, &nSysLimit); }
    "
HDF4_HAS_MAXOPENFILES)
cmake_pop_check_state()

if (HDF4_HAS_MAXOPENFILES)
  target_compile_definitions(gdal_HDF4 PRIVATE -DHDF4_HAS_MAXOPENFILES)
else()
  message(WARNING "SDget_maxopenfiles() not found in HDF library. Strange...")
endif ()

gdal_target_link_libraries(gdal_HDF4 PRIVATE HDF4::HDF4)
