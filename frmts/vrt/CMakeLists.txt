
add_gdal_driver(
  TARGET gdal_vrt
  BUILTIN
  SOURCES gdal_vrt.h
          vrtdataset.h
          vrtderivedrasterband.cpp
          vrtdriver.cpp
          vrtexpression.h
          vrtfilters.cpp
          vrtrasterband.cpp
          vrtrawrasterband.cpp
          vrtsourcedrasterband.cpp
          vrtsources.cpp
          vrtwarped.cpp
          vrtdataset.cpp
          pixelfunctions.cpp
          vrtpansharpened.cpp
          vrtprocesseddataset.cpp
          vrtprocesseddatasetfunctions.cpp
          vrtmultidim.cpp
          STRONG_CXX_WFLAGS)
gdal_standard_includes(gdal_vrt)
target_include_directories(gdal_vrt PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/raw
                                            $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)

if (GDAL_USE_EXPRTK)
    target_sources(gdal_vrt PRIVATE vrtexpression_exprtk.cpp)
        if (MSVC)
            set_source_files_properties(vrtexpression_exprtk.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
        elseif(MINGW)
            set_source_files_properties(vrtexpression_exprtk.cpp PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj")
        endif()
    target_compile_definitions(gdal_vrt PRIVATE GDAL_VRT_ENABLE_EXPRTK)
    target_link_libraries(gdal_vrt PRIVATE ExprTk::ExprTk)
endif()

if (GDAL_USE_MUPARSER)
    target_sources(gdal_vrt PRIVATE vrtexpression_muparser.cpp)
    gdal_target_link_libraries(gdal_vrt PRIVATE muparser::muparser)
    target_compile_definitions(gdal_vrt PRIVATE GDAL_VRT_ENABLE_MUPARSER)
endif()

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdalvrt.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if (GDAL_USE_GEOS)
  gdal_target_link_libraries(gdal_vrt PRIVATE ${GEOS_TARGET})
  target_compile_definitions(gdal_vrt PRIVATE -DHAVE_GEOS=1)
endif ()

include(TargetPublicHeader)
target_public_header(TARGET gdal HEADERS vrtdataset.h gdal_vrt.h)

# because of use of GDALOpenVerticalShiftGrid
set_property(SOURCE vrtwarped.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

if (NOT GDAL_ENABLE_DRIVER_VRT)
    target_compile_definitions(gdal_vrt PRIVATE -DNO_OPEN)
endif()

