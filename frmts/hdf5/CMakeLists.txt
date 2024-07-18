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
    hdf5multidim.cpp
    hdf5eosparser.cpp
    rat.cpp
    s100.cpp
    s100.h
    s102dataset.cpp
    s104dataset.cpp
    s111dataset.cpp
)

add_gdal_driver(TARGET gdal_HDF5
                SOURCES ${SOURCE}
                CORE_SOURCES hdf5drivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/bag_template.xml
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if(NOT TARGET gdal_HDF5)
    return()
endif()

# Try to detect if libhdf5 has thread-safety enabled
if(NOT DEFINED GDAL_ENABLE_HDF5_GLOBAL_LOCK AND UNIX AND
   (HDF5_C_LIBRARIES MATCHES ".so" OR HDF5_C_LIBRARIES MATCHES ".dylib"))

  set(HDF5_LINKS_TO_PTHREAD NO)
  if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    find_program(LDD_PROGRAM ldd)
    if(LDD_PROGRAM)
        foreach (lib IN LISTS HDF5_C_LIBRARIES)
            if(lib MATCHES "libhdf5*.so")
                execute_process(COMMAND "${LDD_PROGRAM}" "${lib}" OUTPUT_VARIABLE res)
                if("${res}" MATCHES "libpthread")
                    set(HDF5_LINKS_TO_PTHREAD YES)
                    message(STATUS "${lib} links against pthread.")
                    break()
                endif()
            endif()
        endforeach()
    endif()
  elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    find_program(OTOOL_PROGRAM otool)
    if(OTOOL_PROGRAM)
        foreach (lib IN LISTS HDF5_C_LIBRARIES)
            if(lib MATCHES "libhdf5*dylib")
                execute_process(COMMAND "${OTOOL_PROGRAM}" -L "${lib}" OUTPUT_VARIABLE res)
                if("${res}" MATCHES "libpthread")
                    set(HDF5_LINKS_TO_PTHREAD YES)
                    message(STATUS "${lib} links against pthread.")
                    break()
                endif()
            endif()
        endforeach()
    endif()
  endif()

  if (NOT HDF5_LINKS_TO_PTHREAD)
    message(WARNING "HDF5 library does not seem to have thread-safety enabled. Adding a global lock on GDAL side")
    target_compile_definitions(gdal_HDF5 PRIVATE -DENABLE_HDF5_GLOBAL_LOCK)
  endif()
elseif(GDAL_ENABLE_HDF5_GLOBAL_LOCK)
  target_compile_definitions(gdal_HDF5 PRIVATE -DENABLE_HDF5_GLOBAL_LOCK)
elseif(NOT DEFINED GDAL_ENABLE_HDF5_GLOBAL_LOCK)
  message(WARNING "HDF5 library status regarding thread-safety is unknown (set GDAL_ENABLE_HDF5_GLOBAL_LOCK=NO if it is known to be thread-safe). Adding a global lock on GDAL side")
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
