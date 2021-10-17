add_library(
  libjpeg OBJECT
  jcapimin.c
  jchuff.h
  jconfig.h
  jdapimin.c
  jdct.h
  jdmarker.c
  jdtrans.c
  jidctflt.c
  jmemmgr.c
  jquant2.c
  jcapistd.c
  jcinit.c
  jcparam.c
  jdapistd.c
  jddctmgr.c
  jdmaster.c
  jerror.c
  jidctfst.c
  jmemsys.h
  jutils.c
  jccoefct.c
  jcmainct.c
  jcphuff.c
  jdatadst.c
  jdhuff.c
  jdmerge.c
  jerror.h
  jidctint.c
  jmorecfg.h
  jversion.h
  jccolor.c
  jcmarker.c
  jcprepct.c
  jdatasrc.c
  jdhuff.h
  jdphuff.c
  jfdctflt.c
  jidctred.c
  jpegint.h
  jcdctmgr.c
  jcmaster.c
  jcsample.c
  jdcoefct.c
  jdinput.c
  jdpostct.c
  jfdctfst.c
  jinclude.h
  jpeglib.h
  jchuff.c
  jcomapi.c
  jctrans.c
  jdcolor.c
  jdmainct.c
  jdsample.c
  jfdctint.c
  jmemnobs.c
  jquant1.c)
target_include_directories(libjpeg PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                           $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)

option(GDAL_JPEG12_SUPPORTED "Set ON to use libjpeg12 support" OFF)
if (GDAL_JPEG12_SUPPORTED)
  target_compile_definitions(libjpeg PRIVATE -DJPEG_DUAL_MODE_8_12)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../libjpeg12/jmorecfg.h.12 DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
  file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/jmorecfg.h.12 ${CMAKE_CURRENT_SOURCE_DIR}/jmorecfg.h)
endif ()

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(libjpeg PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  gdal_add_private_link_libraries(ZLIB::ZLIB)
endif ()

target_compile_definitions(libjpeg PRIVATE -DDEFAULT_MAX_MEM=500000000L)
set_property(TARGET libjpeg PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(gdal PRIVATE $<TARGET_OBJECTS:libjpeg>)
