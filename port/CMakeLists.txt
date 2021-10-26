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
    cpl_userfaultfd.cpp
    cpl_vax.cpp
    cpl_compressor.cpp)
add_library(cpl OBJECT ${CPL_SOURCES})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:cpl>)
target_compile_options(cpl PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
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

if (NOT WIN32)
  gdal_add_private_link_libraries(${CMAKE_DL_LIBS})
endif ()

# Internal libraries first
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
endif ()

target_compile_definitions(cpl PRIVATE -DHAVE_LIBZ -DZIP_SUPPORT)
target_public_header(TARGET gdal HEADERS cpl_minizip_ioapi.h cpl_minizip_unzip.h cpl_minizip_zip.h)
target_sources(cpl PRIVATE cpl_vsil_gzip.cpp cpl_minizip_ioapi.cpp cpl_minizip_unzip.cpp cpl_minizip_zip.cpp)

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()

# Now external libraries
if (NOT GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:JSONC::JSONC,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(JSONC::JSONC)
endif ()

if (NOT GDAL_USE_LIBZ_INTERNAL)
  target_compile_definitions(cpl PRIVATE -DHAVE_ZLIB_H -DHAVE_ZLIB)
  if (MSVC)
    target_compile_definitions(cpl PRIVATE -DZLIB_DLL)
  endif (MSVC)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:ZLIB::ZLIB,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(ZLIB::ZLIB)
endif ()

if (GDAL_USE_CURL)
  target_compile_definitions(cpl PRIVATE -DHAVE_CURL)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:CURL::libcurl,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(CURL::libcurl)
endif ()

if (GDAL_USE_ICONV)
  target_compile_definitions(cpl PRIVATE -DHAVE_ICONV)
  if (Iconv_IS_BUILT_IN)
    target_sources(cpl PRIVATE cpl_recode_iconv.cpp)
  else ()
    if (APPLE)
      target_compile_definitions(cpl PRIVATE -DLIBICONV_PLUG)
    endif ()
    target_sources(cpl PRIVATE cpl_recode_iconv.cpp)
    target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:Iconv::Iconv,INTERFACE_INCLUDE_DIRECTORIES>)
    gdal_add_private_link_libraries(Iconv::Iconv)
  endif ()
endif ()

if (ODBC_FOUND)
  target_sources(cpl PRIVATE cpl_odbc.cpp)
  target_public_header(TARGET gdal HEADERS cpl_odbc.h)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:ODBC::ODBC,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:ODBC::ODBCINST,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(ODBC::ODBC ODBC::ODBCINST)
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
    gdal_add_private_link_libraries(Threads::Threads)
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

if (GDAL_USE_LIBXML2)
  target_compile_definitions(cpl PRIVATE -DHAVE_LIBXML2)
  target_include_directories(cpl PRIVATE ${LIBXML2_INCLUDE_DIRS})
  gdal_add_private_link_libraries(LibXml2::LibXml2)
endif ()

if (HAVE_CRYPTPP)
  target_compile_definitions(cpl PRIVATE -DHAVE_CRYPTOPP)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:CRYPTPP::CRYPTPP,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(CRYPTPP::CRYPTPP)
endif ()

if (TARGET OpenSSL::Crypto)
  target_compile_definitions(cpl PRIVATE -DUSE_OPENSSL_CRYPTO)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:OpenSSL::Crypto,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(OpenSSL::Crypto)
endif ()

if (CRYPTOPP_USE_ONLY_CRYPTODLL_ALG AND NOT BUILD_SHARED_LIBS)
  # Define USE_ONLY_CRYPTODLL_ALG is running against cryptopp.dll, but not if statically linking
  # USE_ONLY_CRYPTODLL_ALG=YES
  #
  # When linking against the DLL: CRYPTOPP_LIB = c:/install-cryptopp-x64/lib/cryptopp.lib
  #
  # When linking against the static lib: CRYPTOPP_LIB = c:/install-cryptopp-x64/lib/cryptlib.lib
  target_compile_definitions(cpl PRIVATE -DUSE_ONLY_CRYPTODLL_ALG)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(cpl PRIVATE -DHAVE_ZSTD)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:ZSTD::zstd,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(ZSTD::zstd)
endif ()

if (GDAL_USE_LIBLZMA)
  target_compile_definitions(cpl PRIVATE -DHAVE_LZMA)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:LibLZMA::LibLZMA,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(LibLZMA::LibLZMA)
endif ()

if (GDAL_USE_DEFLATE)
  target_compile_definitions(cpl PRIVATE -DHAVE_LIBDEFLATE)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:Deflate::Deflate,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(Deflate::Deflate)
endif ()

if (GDAL_USE_LZ4)
  target_compile_definitions(cpl PRIVATE -DHAVE_LZ4)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:LZ4::LZ4,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(LZ4::LZ4)
endif ()

if (GDAL_USE_BLOSC)
  target_compile_definitions(cpl PRIVATE -DHAVE_BLOSC)
  target_include_directories(cpl PRIVATE $<TARGET_PROPERTY:Blosc::Blosc,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(Blosc::Blosc)
endif ()
