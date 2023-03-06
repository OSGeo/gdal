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

if(NOT DEFINED GDAL_ENABLE_HDF5_GLOBAL_LOCK AND UNIX AND
   (HDF5_C_LIBRARIES MATCHES ".so" OR HDF5_C_LIBRARIES MATCHES ".dylib"))
  # We try to infer if the HDF5 library is built with thread-safe support
  # by checking if it links against pthread
  include(CheckCSourceRuns)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES "${HDF5_C_LIBRARIES}")
  check_c_source_runs(
      "
      #include <pthread.h>
      int main()
      {
          pthread_cancel(0);
          return 0;
      }
      "
      HDF5_LINKS_TO_PTHREAD)
  cmake_pop_check_state()
  if (NOT HDF5_LINKS_TO_PTHREAD)
    message(WARNING "HDF5 library does not seem to have thread-safety enabled. Adding a global lock on GDAL side")
    target_compile_definitions(gdal_HDF5 PRIVATE -DENABLE_HDF5_GLOBAL_LOCK)
  endif()
elseif(GDAL_ENABLE_HDF5_GLOBAL_LOCK)
  target_compile_definitions(gdal_HDF5 PRIVATE -DENABLE_HDF5_GLOBAL_LOCK)
endif()

# When build as plugin, initialize all drivers from Register_HDF5
if (GDAL_ENABLE_DRIVER_HDF5_PLUGIN)
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
gdal_target_link_libraries(gdal_HDF5 PRIVATE ${HDF5_C_LIBRARIES})

if (HDF5_BUILD_SHARED_LIBS)
  target_compile_definitions(gdal_HDF5 PRIVATE -DH5_BUILT_AS_DYNAMIC_LIB)
else ()
  target_compile_definitions(gdal_HDF5 PRIVATE -DH5_BUILT_AS_STATIC_LIB)
endif ()
