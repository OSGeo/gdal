set(SOURCE ecwdataset.cpp ecwcreatecopy.cpp ecwasyncreader.cpp jp2userbox.cpp)
add_gdal_driver(
  TARGET gdal_ECW_JP2ECW
  SOURCES ${SOURCE}
  CORE_SOURCES ecwdrivercore.cpp
  DRIVER_NAME_OPTION ECW
  DEF FRMT_ecw PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_ECW_JP2ECW_core)
    target_include_directories(gdal_ECW_JP2ECW_core PRIVATE $<TARGET_PROPERTY:ECW::ECW_ALL,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_ECW_JP2ECW_core PRIVATE $<TARGET_PROPERTY:ECW::ECW_ALL,INTERFACE_COMPILE_DEFINITIONS>)
    foreach (d IN LISTS ECW_INCLUDE_DIRS)
      if (EXISTS "${d}/ECWJP2BuildNumber.h")
        target_compile_definitions(gdal_ECW_JP2ECW_core PRIVATE -DHAVE_ECW_BUILDNUMBER_H)
        break()
      endif ()
    endforeach ()
endif()

if(NOT TARGET gdal_ECW_JP2ECW)
    return()
endif()

gdal_standard_includes(gdal_ECW_JP2ECW)
gdal_target_link_libraries(gdal_ECW_JP2ECW PRIVATE ECW::ECW_ALL)
foreach (d IN LISTS ECW_INCLUDE_DIRS)
  if (EXISTS "${d}/ECWJP2BuildNumber.h")
    target_compile_definitions(gdal_ECW_JP2ECW PRIVATE -DHAVE_ECW_BUILDNUMBER_H)
    break()
  endif ()
endforeach ()
