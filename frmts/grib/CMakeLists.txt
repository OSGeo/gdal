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
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_PNG_INTERNAL;NOT GDAL_USE_ZLIB_INTERNAL"
          NO_DEPS)
gdal_standard_includes(gdal_GRIB)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(
    SOURCE degrib/degrib/clock.c
           degrib/degrib/degrib1.cpp
           degrib/degrib/degrib2.cpp
           degrib/degrib/inventory.cpp
           degrib/degrib/metaname.cpp
           degrib/degrib/myerror.c
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
    PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
target_include_directories(
  gdal_GRIB PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/degrib/degrib>
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/degrib/g2clib> $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
target_compile_options(gdal_GRIB PRIVATE ${GDAL_SOFTWARNFLAGS}) # FIXME: only needed for files within degrib/ actually
if (GDAL_USE_PNG_INTERNAL)
  gdal_add_vendored_lib(gdal_GRIB libpng)
  target_compile_definitions(gdal_GRIB PRIVATE -DUSE_PNG)
  target_sources(gdal_GRIB PRIVATE degrib/g2clib/pngunpack.c degrib/g2clib/dec_png.c)
else ()
  target_compile_definitions(gdal_GRIB PRIVATE -DUSE_PNG)
  gdal_target_link_libraries(gdal_GRIB PRIVATE PNG::PNG)
  target_sources(gdal_GRIB PRIVATE degrib/g2clib/pngunpack.c degrib/g2clib/dec_png.c)
endif ()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_GRIB libz)
else ()
  gdal_target_link_libraries(gdal_GRIB PRIVATE ZLIB::ZLIB)
endif ()
