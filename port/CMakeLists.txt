# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  cpl_atomic_ops.h
  cpl_auto_close.h
  cpl_compressor.h
  cpl_config_extras.h
  cpl_conv.h
  cpl_csv.h
  cpl_error.h
  cpl_hash_set.h
  cpl_http.h
  cpl_json.h
  cplkeywordparser.h
  cpl_list.h
  cpl_minixml.h
  cpl_multiproc.h
  cpl_port.h
  cpl_progress.h
  cpl_quad_tree.h
  cpl_spawn.h
  cpl_string.h
  cpl_time.h
  cpl_vsi.h
  cpl_vsi_error.h
  cpl_vsi_virtual.h
  cpl_virtualmem.h
  gdal_csv.h)

set(CPL_SOURCES
    cpl_alibaba_oss.cpp
    cpl_azure.cpp
    cpl_conv.cpp
    cpl_error.cpp
    cpl_string.cpp
    cplgetsymbol.cpp
    cplstringlist.cpp
    cpl_strtod.cpp
    cpl_path.cpp
    cpl_csv.cpp
    cpl_findfile.cpp
    cpl_minixml.cpp
    cpl_multiproc.cpp
    cpl_list.cpp
    cpl_getexecpath.cpp
    cplstring.cpp
    cpl_vsisimple.cpp
    cpl_vsil.cpp
    cpl_vsi_mem.cpp
    cpl_http.cpp
    cpl_hash_set.cpp
    cplkeywordparser.cpp
    cpl_recode.cpp
    cpl_recode_stub.cpp
    cpl_quad_tree.cpp
    cpl_atomic_ops.cpp
    cpl_vsil_subfile.cpp
    cpl_time.cpp
    cpl_vsil_stdout.cpp
    cpl_vsil_sparsefile.cpp
    cpl_vsil_abstract_archive.cpp
    cpl_vsil_tar.cpp
    cpl_vsil_stdin.cpp
    cpl_vsil_buffered_reader.cpp
    cpl_base64.cpp
    cpl_vsil_curl.cpp
    cpl_vsil_curl_streaming.cpp
    cpl_vsil_cache.cpp
    cpl_xml_validate.cpp
    cpl_spawn.cpp
    cpl_google_oauth2.cpp
    cpl_progress.cpp
    cpl_virtualmem.cpp
    cpl_worker_thread_pool.cpp
    cpl_vsil_crypt.cpp
    cpl_sha1.cpp
    cpl_sha256.cpp
    cpl_aws.cpp
    cpl_aws_win32.cpp
    cpl_vsi_error.cpp
    cpl_cpu_features.cpp
    cpl_google_cloud.cpp
    cpl_json.cpp
    cpl_json_streaming_parser.cpp
    cpl_md5.cpp
    cpl_vsil_hdfs.cpp
    cpl_swift.cpp
    cpl_vsil_adls.cpp
    cpl_vsil_az.cpp
    cpl_vsil_uploadonclose.cpp
    cpl_vsil_gs.cpp
    cpl_vsil_webhdfs.cpp
    cpl_vsil_s3.cpp
    cpl_vsil_oss.cpp
    cpl_vsil_swift.cpp
    cpl_json_streaming_writer.cpp
    cpl_vax.cpp
    cpl_compressor.cpp)
add_library(cpl OBJECT ${CPL_SOURCES})
target_sources(gdal PRIVATE $<TARGET_OBJECTS:cpl>)
set_property(TARGET cpl PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # Part of those files have CPL_CVSID() which break UNITY_BUILD
  set_property(TARGET cpl PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
# include path to generated cpl_config.h
target_include_directories(cpl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if ((EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cpl_config.h") AND NOT ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL
                                                                 "${CMAKE_CURRENT_BINARY_DIR}"))
  # Try to detect issues when building with cmake out of source tree, but against a previous build done in source tree
  message(
    FATAL_ERROR
      "${CMAKE_CURRENT_SOURCE_DIR}/cpl_config.h was found, and likely conflicts with ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h"
    )
endif ()

if (WIN32)
  target_sources(cpl PRIVATE cpl_vsil_win32.cpp)
else ()
  target_sources(cpl PRIVATE cpl_vsil_unix_stdio_64.cpp)
endif ()

if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_definitions(cpl PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
endif ()
if (HAVE_AVX_AT_COMPILE_TIME)
  target_compile_definitions(cpl PRIVATE -DHAVE_AVX_AT_COMPILE_TIME)
endif ()

function (cpl_add_target_link_library)
  set(_multiValueArgs INTERFACE)
  cmake_parse_arguments(_ATLL "" "" "${_multiValueArgs}" ${ARGN})
  if (NOT _ATLL_INTERFACE)
    message(FATAL_ERROR "cpl_add_target_link_library: INTERFACE is mandatory argument")
  endif ()
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
    target_link_libraries(cpl INTERFACE "${_ATLL_INTERFACE}")
  else ()
    # set_target_properties(cpl PROPERTIES INTERFACE_LINK_LIBRARIES "${_ATLL_INTERFACE}")
    target_link_libraries(GDAL_LINK_LIBRARY INTERFACE ${_ATLL_INTERFACE})
  endif ()
endfunction ()

if (NOT WIN32)
  cpl_add_target_link_library(INTERFACE ${CMAKE_DL_LIBS})
endif ()

if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  target_include_directories(cpl PRIVATE ${JSONC_INCLUDE_DIR})
  cpl_add_target_link_library(INTERFACE JSONC::JSONC)
endif ()

target_compile_definitions(cpl PRIVATE -DHAVE_LIBZ -DZIP_SUPPORT)
target_public_header(TARGET gdal HEADERS cpl_minizip_ioapi.h cpl_minizip_unzip.h cpl_minizip_zip.h)
target_sources(cpl PRIVATE cpl_vsil_gzip.cpp cpl_minizip_ioapi.cpp cpl_minizip_unzip.cpp cpl_minizip_zip.cpp)
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  target_compile_definitions(cpl PRIVATE -DHAVE_ZLIB_H -DHAVE_ZLIB)
  if (MSVC)
    target_compile_definitions(cpl PRIVATE -DZLIB_DLL)
  endif (MSVC)
  cpl_add_target_link_library(INTERFACE ZLIB::ZLIB)
endif ()

if (HAVE_CURL)
  target_include_directories(cpl PRIVATE ${CURL_INCLUDE_DIRS})
  target_compile_definitions(cpl PRIVATE -DHAVE_CURL)
  cpl_add_target_link_library(INTERFACE CURL::libcurl)
endif ()

if (HAVE_ICONV)
  target_compile_definitions(cpl PRIVATE -DHAVE_ICONV)
  if (Iconv_IS_BUILT_IN)
    target_sources(cpl PRIVATE cpl_recode_iconv.cpp)
  else ()
    if (APPLE)
      target_compile_definitions(cpl PRIVATE -DLIBICONV_PLUG)
    endif ()
    target_sources(cpl PRIVATE cpl_recode_iconv.cpp)
    cpl_add_target_link_library(INTERFACE Iconv::Iconv)
  endif ()
endif ()

if (ODBC_FOUND)
  target_sources(cpl PRIVATE cpl_odbc.cpp)
  target_public_header(TARGET gdal HEADERS cpl_odbc.h)
  target_include_directories(cpl PRIVATE ${ODBC_INCLUDE_DIRS})
  cpl_add_target_link_library(INTERFACE ODBC::ODBC ODBC::ODBCINST)
endif ()

if (GDAL_USE_XMLREFORMAT)
  target_sources(cpl PRIVATE xmlreformat.cpp)
endif ()

if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(cpl PRIVATE -D_WIN64)
  endif ()
else ()
  if (Threads_FOUND)
    cpl_add_target_link_library(INTERFACE Threads::Threads)
  endif ()
endif ()

if (HAVE_USERFAULTFD_H)
  target_compile_definitions(cpl PRIVATE -DENABLE_UFFD)
endif ()

option(HDFS_ENABLED "Include HDFS support" OFF)
if (HDFS_ENABLED)
  target_compile_definitions(cpl PRIVATE -DHDFS_ENABLED)
endif ()

# for plugin DLFCN: for win32 https://github.com/dlfcn-win32/dlfcn-win32/archive/v1.1.1.tar.gz if(WIN32)
# find_package(dlfcn- win32 REQUIRED) set(CMAKE_DL_LIBS dlfcn-win32::dl) endif()

add_executable(xmlreformat EXCLUDE_FROM_ALL xmlreformat.cpp)

if (HAVE_CRYPTPP)
  target_compile_definitions(cpl PRIVATE -DHAVE_CRYPTOPP)
  cpl_add_target_link_library(INTERFACE CRYPTPP::CRYPTPP)
endif ()

if (TARGET OpenSSL::Crypto)
  target_compile_definitions(cpl PRIVATE -DUSE_OPENSSL_CRYPTO)
  cpl_add_target_link_library(INTERFACE OpenSSL::Crypto)
endif ()

if (CRYPTOPP_USE_ONLY_CRYPTODLL_ALG AND NOT BULID_SHARED_LIBS)
  # Define USE_ONLY_CRYPTODLL_ALG is running against cryptopp.dll, but not if statically linking
  # USE_ONLY_CRYPTODLL_ALG=YES
  #
  # When linking against the DLL: CRYPTOPP_LIB = c:/install-cryptopp-x64/lib/cryptopp.lib
  #
  # When linking against the static lib: CRYPTOPP_LIB = c:/install-cryptopp-x64/lib/cryptlib.lib
  target_compile_definitions(cpl PRIVATE -DUSE_ONLY_CRYPTODLL_ALG)
endif ()

if (HAVE_ZSTD)
  target_compile_definitions(cpl PRIVATE -DHAVE_ZSTD)
  target_include_directories(cpl PRIVATE ${ZSTD_INCLUDE_DIRS})
  cpl_add_target_link_library(INTERFACE ZSTD::zstd)
endif ()
