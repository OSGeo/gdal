set(SOURCES
    jcapimin.c
    jdapimin.c
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
    jutils.c
    jccoefct.c
    jcmainct.c
    jcphuff.c
    jdatadst.c
    jdhuff.c
    jdmerge.c
    jidctint.c
    jccolor.c
    jcmarker.c
    jcprepct.c
    jdatasrc.c
    jdphuff.c
    jfdctflt.c
    jidctred.c
    jcdctmgr.c
    jcmaster.c
    jcsample.c
    jdcoefct.c
    jdinput.c
    jdpostct.c
    jfdctfst.c
    jchuff.c
    jcomapi.c
    jctrans.c
    jdcolor.c
    jdmainct.c
    jdsample.c
    jfdctint.c
    jmemnobs.c
    jquant1.c)

set(HEADERS
    jchuff.h
    jconfig.h
    jdct.h
    jmemsys.h
    jerror.h
    # jmorecfg.h
    jversion.h
    jdhuff.h
    jpegint.h
    jinclude.h
    jpeglib.h)

set(SOURCES_12)
foreach (_src IN LISTS SOURCES)
  string(REPLACE ".c" "12.c" _dst ${_src})
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../libjpeg/${_src}" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/${_src}" "${CMAKE_CURRENT_BINARY_DIR}/${_dst}")
  set(SOURCES_12 ${SOURCES_12} "${CMAKE_CURRENT_BINARY_DIR}/${_dst}")
endforeach ()

foreach (_src IN LISTS HEADERS)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../libjpeg/${_src}" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endforeach ()

configure_file(jmorecfg.h.12 ${CMAKE_CURRENT_BINARY_DIR}/jmorecfg.h COPYONLY)

add_library(libjpeg12 OBJECT ${SOURCES_12})
target_include_directories(libjpeg12 PRIVATE ${CMAKE_CURRENT_BINARY_DIR} $<TARGET_PROPERTY:cpl,SOURCE_DIR>
                                             $<TARGET_PROPERTY:cpl,BINARY_DIR>)
target_compile_options(libjpeg12 PRIVATE ${GDAL_C_WARNING_FLAGS} ${GDAL_SOFTWARNFLAGS})
target_compile_definitions(libjpeg12 PRIVATE -DJPEG_DUAL_MODE_8_12)

target_compile_definitions(libjpeg12 PRIVATE -DDEFAULT_MAX_MEM=500000000L)
set_property(TARGET libjpeg12 PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libjpeg12>)
