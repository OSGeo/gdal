set(SOURCE ecwdataset.cpp ecwcreatecopy.cpp ecwasyncreader.cpp jp2userbox.cpp)
add_gdal_driver(
  TARGET gdal_ECW_JP2ECW
  SOURCES ${SOURCE} DRIVER_NAME_OPTION ECW
  DEF FRMT_ecw PLUGIN_CAPABLE)
gdal_standard_includes(gdal_ECW_JP2ECW)
target_compile_definitions(gdal_ECW_JP2ECW PRIVATE -DFRMT_ecw)
gdal_target_link_libraries(gdal_ECW_JP2ECW PRIVATE ECW::ECW_ALL)
foreach (d IN LISTS ECW_INCLUDE_DIRS)
  if (EXISTS "${d}/ECWJP2BuildNumber.h")
    target_compile_definitions(gdal_ECW_JP2ECW PRIVATE -DHAVE_ECW_BUILDNUMBER_H)
    break()
  endif ()
endforeach ()
