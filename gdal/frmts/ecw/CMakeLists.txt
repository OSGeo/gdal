option(GDAL_ENABLE_ECW_PLUGIN "On to build ECW driver as plugin" ON)
set(SOURCE ecwdataset.cpp ecwcreatecopy.cpp ecwasyncreader.cpp jp2userbox.cpp)
if (GDAL_ENABLE_ECW_PLUGIN)
  add_gdal_driver(
    TARGET gdal_ECW_JP2ECW
    SOURCES ${SOURCE}
    PLUGIN)
else ()
  add_gdal_driver(
    TARGET gdal_ECW_JP2ECW
    SOURCES ${SOURCE}
    DEF FRMT_ecw)
endif ()
gdal_standard_includes(gdal_ECW_JP2ECW)
target_compile_definitions(gdal_ECW_JP2ECW PRIVATE -DFRMT_ecw -DDO_NOT_USE_DEBUG_BOOL)
gdal_target_link_libraries(TARGET gdal_ECW_JP2ECW LIBRARIES ECW::ECW ECW::ECWC ECW::ECWnet ECW::NCSUtil)
foreach (d IN LISTS ECW_INCLUDE_DIRS)
  if (EXISTS "${d}/ECWJP2BuildNumber.h")
    target_compile_definitions(gdal_ECW_JP2ECW PRIVATE -DHAVE_ECW_BUILDNUMBER_H)
    break()
  endif ()
endforeach ()
