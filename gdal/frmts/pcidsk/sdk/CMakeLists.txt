add_library(
  pcidsk_sdk STATIC
  blockdir/asciitiledir.cpp
  blockdir/asciitilelayer.cpp
  blockdir/binarytiledir.cpp
  blockdir/binarytilelayer.cpp
  blockdir/blockdir.cpp
  blockdir/blockfile.cpp
  blockdir/blocklayer.cpp
  blockdir/blocktiledir.cpp
  blockdir/blocktilelayer.cpp
  channel/cbandinterleavedchannel.cpp
  channel/cexternalchannel.cpp
  channel/cpcidskchannel.cpp
  channel/cpixelinterleavedchannel.cpp
  channel/ctiledchannel.cpp
  core/clinksegment.cpp
  core/cpcidskblockfile.cpp
  core/cpcidskfile.cpp
  core/edb_pcidsk.cpp
  core/libjpeg_io.cpp
  core/metadataset_p.cpp
  core/pcidsk_pubutils.cpp
  core/pcidsk_raster.cpp
  core/pcidsk_scanint.cpp
  core/pcidsk_utils.cpp
  core/pcidskbuffer.cpp
  core/pcidskcreate.cpp
  core/pcidskexception.cpp
  core/pcidskinterfaces.cpp
  core/pcidskopen.cpp
  segment/cpcidsk_array.cpp
  segment/cpcidsk_tex.cpp
  segment/cpcidskbinarysegment.cpp
  segment/cpcidskbitmap.cpp
  segment/cpcidskblut.cpp
  segment/cpcidskpct.cpp
  segment/cpcidskephemerissegment.cpp
  segment/cpcidskgcp2segment.cpp
  segment/cpcidskgeoref.cpp
  segment/cpcidsklut.cpp
  segment/cpcidskbpct.cpp
  segment/cpcidskpolymodel.cpp
  segment/cpcidskrpcmodel.cpp
  segment/cpcidsksegment.cpp
  segment/cpcidsktoutinmodel.cpp
  segment/cpcidskvectorsegment.cpp
  segment/cpcidskvectorsegment_consistencycheck.cpp
  segment/metadatasegment_p.cpp
  segment/systiledir.cpp
  segment/vecsegdataindex.cpp
  segment/vecsegheader.cpp)
if (WIN32)
  target_sources(pcidsk_sdk PRIVATE port/io_win32.cpp port/win32_mutex.cpp)
else ()
  target_sources(pcidsk_sdk PRIVATE port/io_stdio.cpp port/pthread_mutex.cpp)
endif ()
target_include_directories(pcidsk_sdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} $<TARGET_PROPERTY:cpl,SOURCE_DIR>
                                              $<TARGET_PROPERTY:cpl,BINARY_DIR>)
set_property(TARGET pcidsk_sdk PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET pcidsk_sdk PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(TARGET pcidsk_sdk PROPERTY UNITY_BUILD OFF)
endif ()

if (NOT TARGET PCIDSK::PCIDSK)
  add_library(PCIDSK::PCIDSK UNKNOWN IMPORTED)
  set_target_properties(PCIDSK::PCIDSK PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

target_link_libraries(GDAL_LINK_LIBRARY INTERFACE pcidsk_sdk)
