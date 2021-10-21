# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

if (NOT TARGET ogr_SQLite)
  return()
endif ()
if (CMAKE_CROSSCOMPILONG)
  return()
endif ()

include(GdalStandardIncludes)
add_library(fuzzingengine EXCLUDE_FROM_ALL fuzzingengine.cpp)
set(FUZZ_TARGETS)
set(FUZZ_CORPUS)

function (build_fuzzer)
  set(_options)
  set(_oneValueArgs NAME)
  set(_multiValueArgs SOURCES DEFINITIONS)
  cmake_parse_arguments(_FUZZ "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  add_executable(${_FUZZ_NAME} EXCLUDE_FROM_ALL ${_FUZZ_SOURCES})
  set(FUZZ_TARGETS
      ${FUZZ_TARGETS} ${_FUZZ_NAME}
      PARENT_SCOPE)
  set(FUZZ_CORPUS
      ${FUZZ_CORPUS} ${_FUZZ_NAME}_seed_corpus.zip
      PARENT_SCOPE)
  if (_FUZZ_DEFINITIONS)
    target_compile_definitions(${_FUZZ_NAME} PRIVATE ${_FUZZ_DEFINITIONS})
  endif ()
  target_link_libraries(${_FUZZ_NAME} PRIVATE $<TARGET_NAME:fuzzingengine> $<TARGET_NAME:gdal>)
  gdal_standard_includes(${_FUZZ_NAME})
  set_target_properties(${_FUZZ_NAME} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
  target_include_directories(${_FUZZ_NAME} PRIVATE $<TARGET_PROPERTY:ogr_SQLite,SOURCE_DIR>)
  # Windows(Mingw/MSVC) link libraries
  if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${_FUZZ_NAME} PRIVATE wsock32 ws2_32 psapi)
  endif ()
endfunction ()

build_fuzzer(NAME gdal_fuzzer SOURCES gdal_fuzzer.cpp)
build_fuzzer(NAME gdal_translate_fuzzer SOURCES gdal_translate_fuzzer.cpp)
build_fuzzer(NAME gdal_vector_translate_fuzzer SOURCES gdal_vector_translate_fuzzer.cpp)
build_fuzzer(NAME get_jpeg2000_structure_fuzzer SOURCES get_jpeg2000_structure_fuzzer.cpp)
build_fuzzer(NAME gml_geom_import_fuzzer SOURCES gml_geom_import_fuzzer.cpp)
build_fuzzer(NAME ogr_fuzzer SOURCES ogr_fuzzer.cpp)
build_fuzzer(NAME osr_set_from_user_input_fuzzer SOURCES osr_set_from_user_input_fuzzer.cpp)
build_fuzzer(NAME spatialite_geom_import_fuzzer SOURCES spatialite_geom_import_fuzzer.cpp)
build_fuzzer(NAME wkb_import_fuzzer SOURCES wkb_import_fuzzer.cpp)
build_fuzzer(NAME wkt_import_fuzzer SOURCES wkt_import_fuzzer.cpp)
build_fuzzer(
  NAME
  alg_fuzzer
  SOURCES
  gdal_fuzzer.cpp
  DEFINITIONS
  -DREGISTER_FUNC=GDALRegister_AIGrid
  -DMEM_FILENAME="/vsimem/test.tar"
  -DGDAL_FILENAME="/vsitar//vsimem/test.tar/hdr.adf")
build_fuzzer(
  NAME
  gtiff_mmap
  SOURCES
  gdal_fuzzer.cpp
  DEFINITIONS
  -DREGISTER_FUNC=GDALRegister_GTiff
  -DGTIFF_USE_MMAP)

function (build_ogr_specialized_fuzzer _format _registerFunc _memfile _gdalfile)
  build_fuzzer(
    NAME
    ${_format}_fuzzer
    SOURCES
    ogr_fuzzer.cpp
    DEFINITIONS
    -DREGISTER_FUNC=${_registerFunc}
    -DMEM_FILENAME="${_memfile}"
    -DGDAL_FILENAME="${_gdalfile}")
endfunction ()

build_ogr_specialized_fuzzer(dxf RegisterOGRDXF "/vsimem/test" "/vsimem/test")
build_ogr_specialized_fuzzer(ogr_sdts RegisterOGRSDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/TR01CATD.DDF")
build_ogr_specialized_fuzzer(openfilegdb RegisterOGROpenFileGDB "/vsimem/test.gdb.tar" "/vsimem/test.gdb.tar")
build_ogr_specialized_fuzzer(shape OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.shp")
build_ogr_specialized_fuzzer(mitab_mif OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.mif")
build_ogr_specialized_fuzzer(mitab_tab OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.tab")
build_ogr_specialized_fuzzer(csv RegisterOGRCSV "/vsimem/test.csv" "/vsimem/test.csv")
build_ogr_specialized_fuzzer(bna RegisterOGRBNA "/vsimem/test.bna" "/vsimem/test.bna")
build_ogr_specialized_fuzzer(wasp RegisterOGRWAsP "/vsimem/test.map" "/vsimem/test.map")
build_ogr_specialized_fuzzer(xlsx RegisterOGRXLSX "/vsimem/test.xlsx" "/vsitar/{/vsimem/test.xlsx}")
build_ogr_specialized_fuzzer(ods RegisterOGRODS "/vsimem/test.ods" "/vsitar/{/vsimem/test.ods}")
build_ogr_specialized_fuzzer(avce00 RegisterOGRAVCE00 "/vsimem/test.e00" "/vsimem/test.e00")
build_ogr_specialized_fuzzer(avcbin RegisterOGRAVCBin "/vsimem/test.tar" "/vsitar/{/vsimem/test.tar}/testavc")
build_ogr_specialized_fuzzer(gml RegisterOGRGML "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.gml")

build_fuzzer(NAME cad_fuzzer SOURCES ogr_fuzzer.cpp DEFINITIONS -DREGISTER_FUNC=RegisterOGRCAD)
build_fuzzer(
  NAME
  rec_fuzzer
  SOURCES
  ogr_fuzzer.cpp
  DEFINITIONS
  -DREGISTER_FUNC=RegisterOGRREC
  -DUSE_FILESYSTEM
  -DEXTENSION="rec")

function (build_gdal_specialized_fuzzer _format _registerFunc _memfile _gdalfile)
  build_fuzzer(
    NAME
    ${_format}_fuzzer
    SOURCES
    gdal_fuzzer.cpp
    DEFINITIONS
    -DREGISTER_FUNC=${_registerFunc}
    -DMEM_FILENAME="${_memfile}"
    -DGDAL_FILENAME="${_gdalfile}")
endfunction ()

foreach (format IN ITEMS GTiff HFA)
  string(TOLOWER ${format} fuzzerName)
  build_gdal_specialized_fuzzer(${fuzzerName} "GDALRegister_${format}" "/vsimem/test" "/vsimem/test")
endforeach ()
build_gdal_specialized_fuzzer(adrg GDALRegister_ADRG "/vsimem/test.tar" "/vsitar//vsimem/test.tar/ABCDEF01.GEN")
build_gdal_specialized_fuzzer(srp GDALRegister_SRP "/vsimem/test.tar" "/vsitar//vsimem/test.tar/FKUSRP01.IMG")
build_gdal_specialized_fuzzer(envi GDALRegister_ENVI "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.dat")
build_gdal_specialized_fuzzer(ehdr GDALRegister_EHdr "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.dat")
build_gdal_specialized_fuzzer(genbin GDALRegister_GenBin "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.bil")
build_gdal_specialized_fuzzer(isce GDALRegister_ISCE "/vsimem/test.tar" "/vsitar//vsimem/test.tar/isce.slc")
build_gdal_specialized_fuzzer(roipac GDALRegister_ROIPAC "/vsimem/test.tar" "/vsitar//vsimem/test.tar/srtm.dem")
build_gdal_specialized_fuzzer(rraster GDALRegister_RRASTER "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.grd")
build_gdal_specialized_fuzzer(aig GDALRegister_AIGrid "/vsimem/test.tar" "/vsitar//vsimem/test.tar/hdr.adf")
# mrf can use indirectly the GTiff driver
build_gdal_specialized_fuzzer(mrf "GDALRegister_mrf();GDALRegister_GTiff" "/vsimem/test.tar"
                              "/vsitar//vsimem/test.tar/byte.mrf")
build_gdal_specialized_fuzzer(gdal_sdts GDALRegister_SDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/1107CATD.DDF")
build_gdal_specialized_fuzzer(gdal_vrt GDALAllRegister "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.vrt")
build_gdal_specialized_fuzzer(ers GDALRegister_ERS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.ers")

build_fuzzer(
  NAME
  gdal_filesystem_fuzzer
  SOURCES
  gdal_fuzzer.cpp
  fuzzingengine.cpp
  DEFINITIONS
  -DUSE_FILESYSTEM)
build_fuzzer(
  NAME
  ogr_filesystem_fuzzer
  SOURCES
  ogr_fuzzer.cpp
  fuzzingengine.cpp
  DEFINITIONS
  -DUSE_FILESYSTEM)

add_custom_command(
  OUTPUT ${FUZZ_CORPUS}
  COMMAND env OUT=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/build_seed_corpus.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL)
add_custom_target(fuzzers DEPENDS ${FUZZ_TARGETS} ${FUZZ_CORPUS})

unset(FUZZ_TARGETS)
unset(FUZZ_CORPUS)
