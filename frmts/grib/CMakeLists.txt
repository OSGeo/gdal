add_gdal_driver(
  TARGET gdal_GRIB
  SOURCES gribdataset.cpp
          gribcreatecopy.cpp
          degrib/degrib/clock.c
          degrib/degrib/degrib1.cpp
          degrib/degrib/degrib2.cpp
          degrib/degrib/inventory.cpp
          degrib/degrib/metaname.cpp
          degrib/degrib/myerror.cpp
          degrib/degrib/grib1tab.cpp
          degrib/degrib/myutil.c
          degrib/degrib/metaparse.cpp
          degrib/degrib/weather.c
          degrib/degrib/metaprint.cpp
          degrib/degrib/grib2api.c
          degrib/degrib/myassert.c
          degrib/degrib/scan.c
          degrib/degrib/tendian.cpp
          degrib/g2clib/gridtemplates.c
          degrib/g2clib/drstemplates.c
          degrib/g2clib/pdstemplates.c
          degrib/g2clib/gbits.c
          degrib/g2clib/g2_free.c
          degrib/g2clib/g2_unpack1.c
          degrib/g2clib/g2_unpack2.c
          degrib/g2clib/g2_unpack3.c
          degrib/g2clib/g2_unpack4.c
          degrib/g2clib/g2_unpack5.c
          degrib/g2clib/g2_unpack6.c
          degrib/g2clib/g2_unpack7.c
          degrib/g2clib/g2_info.c
          degrib/g2clib/g2_getfld.c
          degrib/g2clib/simunpack.c
          degrib/g2clib/comunpack.c
          degrib/g2clib/pack_gp.c
          degrib/g2clib/reduce.c
          degrib/g2clib/specunpack.c
          degrib/g2clib/rdieee.c
          degrib/g2clib/mkieee.c
          degrib/g2clib/int_power.c
          degrib/g2clib/simpack.c
          degrib/g2clib/compack.c
          degrib/g2clib/cmplxpack.c
          degrib/g2clib/misspack.c
          degrib/g2clib/dec_jpeg2000.cpp
          degrib/g2clib/jpcunpack.c
          degrib/degrib/hazard.c
      CORE_SOURCES
          gribdrivercore.cpp
      PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_PNG_INTERNAL\\\;NOT GDAL_USE_ZLIB_INTERNAL"
      NO_DEPS
      NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_GRIB)
    return()
endif()

gdal_standard_includes(gdal_GRIB)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_versions.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_center.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_process.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_subcenter.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_13.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_14.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_15.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_16.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_17.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_18.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_190.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_191.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_19.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_20.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_21.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_3.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_4.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_5.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_6.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_0_7.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_191.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_3.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_10_4.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_1_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_1_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_1_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_20_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_20_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_20_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_2_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_2_3.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_2_4.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_2_5.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_2_6.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_3.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_4.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_5.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_3_6.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_0.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_10.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_1.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_2.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_3.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_4.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_5.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_6.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_7.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_8.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_4_9.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_Canada.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_HPC.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_index.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_MRMS.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_NCEP.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_2_local_NDFD.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/grib2_table_4_5.csv
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

target_include_directories(
  gdal_GRIB PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/degrib/degrib>
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/degrib/g2clib> $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
target_compile_options(gdal_GRIB PRIVATE ${GDAL_SOFTWARNFLAGS}) # FIXME: only needed for files within degrib/ actually
if (GDAL_USE_PNG_INTERNAL)
  gdal_add_vendored_lib(gdal_GRIB libpng)
  target_compile_definitions(gdal_GRIB PRIVATE -DUSE_PNG)
  target_sources(gdal_GRIB PRIVATE degrib/g2clib/pngunpack.c degrib/g2clib/dec_png.c)
endif()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_GRIB libz)
endif ()

if (GDAL_USE_PNG AND NOT GDAL_USE_PNG_INTERNAL)
  target_compile_definitions(gdal_GRIB PRIVATE -DUSE_PNG)
  gdal_target_link_libraries(gdal_GRIB PRIVATE PNG::PNG)
  target_sources(gdal_GRIB PRIVATE degrib/g2clib/pngunpack.c degrib/g2clib/dec_png.c)
endif ()

if (GDAL_USE_ZLIB AND NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(gdal_GRIB PRIVATE ZLIB::ZLIB)
endif ()

if (GDAL_USE_LIBAEC)
  target_compile_definitions(gdal_GRIB PRIVATE -DUSE_AEC)
  gdal_target_link_libraries(gdal_GRIB PRIVATE LIBAEC::LIBAEC)
  target_sources(gdal_GRIB PRIVATE degrib/g2clib/aecunpack.c)
endif ()
