add_gdal_driver(
  TARGET gdal_KEA
  SOURCES keaband.cpp
          keaband.h
          keacopy.cpp
          keacopy.h
          keadataset.cpp
          keadataset.h
          keadriver.cpp
          keamaskband.cpp
          keamaskband.h
          keaoverview.cpp
          keaoverview.h
          kearat.cpp
          kearat.h
          libkea_headers.h
  CORE_SOURCES
          keadrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_KEA_core)
    target_include_directories(gdal_KEA_core PRIVATE ${HDF5_INCLUDE_DIRS})
    target_include_directories(gdal_KEA_core PRIVATE $<TARGET_PROPERTY:KEA::KEA,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_KEA_core PRIVATE $<TARGET_PROPERTY:KEA::KEA,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_KEA)
    return()
endif()

gdal_standard_includes(gdal_KEA)
target_include_directories(gdal_KEA PRIVATE ${HDF5_INCLUDE_DIRS})
gdal_target_link_libraries(gdal_KEA PRIVATE KEA::KEA ${HDF5_CXX_LIBRARIES})
if (HDF5_BUILD_SHARED_LIBS)
  target_compile_definitions(gdal_KEA PRIVATE -DH5_BUILT_AS_DYNAMIC_LIB)
else ()
  target_compile_definitions(gdal_KEA PRIVATE -DH5_BUILT_AS_STATIC_LIB)
endif ()
