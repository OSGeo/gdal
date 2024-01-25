# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

include(TargetPublicHeader)
include(GdalDriverHelper)

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
    cpl_vsil_libarchive.cpp
    cpl_vsil_stdin.cpp
    cpl_vsil_buffered_reader.cpp
    cpl_vsil_plugin.cpp
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
    cpl_compressor.cpp
    cpl_float.cpp)
add_library(cpl OBJECT ${CPL_SOURCES})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:cpl>)
target_compile_options(cpl PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
set_property(TARGET cpl PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

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

file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}/${GDAL_RESOURCE_PATH}" INST_DATA_PATH)
target_compile_definitions(cpl PRIVATE INST_DATA="${INST_DATA_PATH}")

if(CMAKE_INSTALL_FULL_SYSCONFDIR)
    target_compile_definitions(cpl PRIVATE SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}")
endif()

# To enable conditional use of std::shared_mutex in cpl_vsi_mem.cpp
add_library(cpl_vsi_mem OBJECT cpl_vsi_mem.cpp)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:cpl_vsi_mem>)
target_compile_options(cpl_vsi_mem PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
set_property(TARGET cpl_vsi_mem PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_include_directories(cpl_vsi_mem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(COMPILER_SUPPORTS_CXX17)
    target_compile_features(cpl_vsi_mem PRIVATE cxx_std_17)
endif()

if (WIN32)
  target_sources(cpl PRIVATE cpl_vsil_win32.cpp)
else ()
  target_sources(cpl PRIVATE cpl_vsil_unix_stdio_64.cpp)
  if ("${CMAKE_SYSTEM}" MATCHES "Linux")
      check_include_file("linux/fs.h" HAVE_LINUX_FS_H)
      if (NOT HAVE_LINUX_FS_H)
          set(ACCEPT_MISSING_LINUX_FS_HEADER OFF CACHE BOOL "Build despite missing linux/fs.h header.")
          mark_as_advanced(ACCEPT_MISSING_LINUX_FS_HEADER)
          if (NOT ACCEPT_MISSING_LINUX_FS_HEADER)
              message(FATAL_ERROR "linux/fs.h header not found. Impact will be lack of sparse file detection. Define the ACCEPT_MISSING_LINUX_FS_HEADER:BOOL=ON CMake variable if you want to build despite this limitation.")
          endif()
          target_compile_definitions(cpl PRIVATE -DMISSING_LINUX_FS_H)
      endif()
  endif()
  if(HAVE_PREAD64)
      target_compile_definitions(cpl PRIVATE -DHAVE_PREAD64)
  elseif(HAVE_PREAD_BSD)
      target_compile_definitions(cpl PRIVATE -DHAVE_PREAD_BSD -DSIZEOF_OFF_T=${SIZEOF_OFF_T})
  endif()
  set(BUILD_WITHOUT_64BIT_OFFSET OFF CACHE BOOL "Build GDAL without > 4GB file support. If file API does not seem to support 64-bit offset.")
  mark_as_advanced(BUILD_WITHOUT_64BIT_OFFSET)
  if(BUILD_WITHOUT_64BIT_OFFSET)
      target_compile_definitions(cpl PRIVATE -DBUILD_WITHOUT_64BIT_OFFSET)
  endif()
endif ()

if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_definitions(cpl PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
endif ()
if (HAVE_AVX_AT_COMPILE_TIME)
  target_compile_definitions(cpl PRIVATE -DHAVE_AVX_AT_COMPILE_TIME)
endif ()

if (NOT WIN32 AND CMAKE_DL_LIBS)
  gdal_target_link_libraries(cpl PRIVATE ${CMAKE_DL_LIBS})
endif ()

# Internal libraries first
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(cpl libjson)
endif ()

target_compile_definitions(cpl PRIVATE -DHAVE_LIBZ -DZIP_SUPPORT)
target_sources(cpl PRIVATE cpl_vsil_gzip.cpp cpl_minizip_ioapi.cpp cpl_minizip_unzip.cpp cpl_minizip_zip.cpp)

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(cpl libz)
endif ()
if (ENABLE_DEFLATE64)
  gdal_add_vendored_lib(cpl infback9)
  target_compile_definitions(cpl PRIVATE -DENABLE_DEFLATE64)
endif ()

# Now external libraries
if (NOT GDAL_USE_JSONC_INTERNAL)
  gdal_target_link_libraries(cpl PRIVATE ${JSONC_TARGET})
endif ()

if (NOT GDAL_USE_ZLIB_INTERNAL)
  target_compile_definitions(cpl PRIVATE -DHAVE_ZLIB_H -DHAVE_ZLIB)
  if (MSVC AND NOT ZLIB_IS_STATIC)
    target_compile_definitions(cpl PRIVATE -DZLIB_DLL)
  endif ()
  gdal_target_link_libraries(cpl PRIVATE ZLIB::ZLIB)
endif ()

if (GDAL_USE_ARCHIVE)
  target_compile_definitions(cpl PRIVATE -DHAVE_LIBARCHIVE)
  gdal_target_link_libraries(cpl PRIVATE ARCHIVE::ARCHIVE)
endif ()

if (GDAL_USE_CURL)
  target_compile_definitions(cpl PRIVATE -DHAVE_CURL)
  gdal_target_link_libraries(cpl PRIVATE CURL::libcurl)
endif ()

if (GDAL_USE_ICONV)
  target_compile_definitions(cpl PRIVATE -DHAVE_ICONV)

  # Create a dedicated object library for Linux CondaForge builds, that could otherwise accidentally include libiconv
  # header file (that would have define iconv_open libiconv_open) whereas linking to system iconv.
  add_library(cpl_iconv OBJECT cpl_recode_iconv.cpp)
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:cpl_iconv>)
  target_compile_options(cpl_iconv PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
  set_property(TARGET cpl_iconv PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  target_compile_definitions(cpl_iconv PRIVATE -DHAVE_ICONV)
  # include path to generated cpl_config.h
  target_include_directories(cpl_iconv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

  if (NOT Iconv_IS_BUILT_IN)
    if (APPLE)
      # linkage against OSX's system iconv is going to require LIBICONV_PLUG to be defined, but other self-built
      # situations like CondaForge or Homebrew might not need this
      set(CMAKE_REQUIRED_LIBRARIES ${Iconv_LIBRARY})
      check_symbol_exists(_iconv_close "iconv.h" HAVE_ICONV_CLOSE)
      if (HAVE_ICONV_CLOSE)
        target_compile_definitions(cpl_iconv PRIVATE -DLIBICONV_PLUG)
      endif (HAVE_ICONV_CLOSE)
    endif (APPLE)
    gdal_target_link_libraries(cpl_iconv PRIVATE Iconv::Iconv)
  endif ()
endif ()

if (GDAL_USE_ODBC)
  target_sources(cpl PRIVATE cpl_odbc.cpp)
  target_public_header(TARGET gdal HEADERS cpl_odbc.h)
  gdal_target_link_libraries(cpl PRIVATE ODBC::ODBC)
  if (TARGET ODBC::ODBCINST)
    gdal_target_link_libraries(cpl PRIVATE ODBC::ODBCINST)
  endif ()
endif ()

if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(cpl PRIVATE -D_WIN64)
  endif ()
else ()
  if (Threads_FOUND)
    gdal_target_link_libraries(cpl PRIVATE Threads::Threads)
  endif ()
endif ()

if (HAVE_USERFAULTFD_H)
  target_compile_definitions(cpl PRIVATE -DENABLE_UFFD)
endif ()

# for plugin DLFCN: for win32 https://github.com/dlfcn-win32/dlfcn-win32/archive/v1.1.1.tar.gz if(WIN32)
# find_package(dlfcn- win32 REQUIRED) set(CMAKE_DL_LIBS dlfcn-win32::dl) endif()

if (GDAL_USE_LIBXML2)
  target_compile_definitions(cpl PRIVATE -DHAVE_LIBXML2)
  gdal_target_link_libraries(cpl PRIVATE LibXml2::LibXml2)
endif ()

if (GDAL_USE_CRYPTOPP)
  target_compile_definitions(cpl PRIVATE -DHAVE_CRYPTOPP)
  gdal_target_link_libraries(cpl PRIVATE CRYPTOPP::CRYPTOPP)
endif ()

if (GDAL_USE_OPENSSL
    AND TARGET OpenSSL::Crypto
    AND TARGET OpenSSL::SSL)
  target_compile_definitions(cpl PRIVATE -DHAVE_OPENSSL_CRYPTO)
  gdal_target_link_libraries(cpl PRIVATE OpenSSL::Crypto OpenSSL::SSL)
  if (WIN32)
    gdal_target_link_libraries(cpl PRIVATE crypt32)
  endif ()
endif ()

if (CRYPTOPP_USE_ONLY_CRYPTODLL_ALG)
  # Define USE_ONLY_CRYPTODLL_ALG is running against cryptopp.dll, but not if statically linking
  target_compile_definitions(cpl PRIVATE -DUSE_ONLY_CRYPTODLL_ALG)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(cpl PRIVATE -DHAVE_ZSTD)
  gdal_target_link_libraries(cpl PRIVATE ${ZSTD_TARGET})
endif ()

if (GDAL_USE_LIBLZMA)
  target_compile_definitions(cpl PRIVATE -DHAVE_LZMA)
  gdal_target_link_libraries(cpl PRIVATE LibLZMA::LibLZMA)
endif ()

if (GDAL_USE_DEFLATE)
  target_compile_definitions(cpl PRIVATE -DHAVE_LIBDEFLATE)
  gdal_target_link_libraries(cpl PRIVATE Deflate::Deflate)
endif ()

if (GDAL_USE_LZ4)
  target_compile_definitions(cpl PRIVATE -DHAVE_LZ4)
  gdal_target_link_libraries(cpl PRIVATE LZ4::LZ4)
endif ()

if (GDAL_USE_BLOSC)
  include(CheckSymbolExists)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${BLOSC_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${BLOSC_LIBRARIES})
  check_symbol_exists(blosc_cbuffer_validate blosc.h BLOSC_HAS_BLOSC_CBUFFER_VALIDATE)
  cmake_pop_check_state()
  if (BLOSC_HAS_BLOSC_CBUFFER_VALIDATE)
    target_compile_definitions(cpl PRIVATE -DHAVE_BLOSC)
    gdal_target_link_libraries(cpl PRIVATE Blosc::Blosc)
  else()
    message(STATUS "libblosc cannot be used as it lacks blosc_cbuffer_validate()")
  endif()
endif ()

if (GDAL_USE_HDFS)
  target_compile_definitions(cpl PRIVATE -DHDFS_ENABLED)
  gdal_target_link_libraries(cpl PRIVATE HDFS::HDFS)
  if (JNI_FOUND)
    gdal_target_link_libraries(cpl PRIVATE HDFS::JVM)
  endif ()
endif ()

if (MSVC)
  # needed for cpl_aws_win32
  gdal_target_link_libraries(cpl PRIVATE wbemuuid)
endif ()

if (NOT CMAKE_CROSSCOMPILING AND BUILD_VSIPRELOAD AND "${CMAKE_SYSTEM}" MATCHES "Linux" AND ${BUILD_SHARED_LIBS})
  option(BUILD_VSIPRELOAD "Build vsipreload.so" ON)
  mark_as_advanced(BUILD_VSIPRELOAD)
  if(BUILD_VSIPRELOAD)
    add_library(vsipreload MODULE vsipreload.cpp)
    set_target_properties(vsipreload PROPERTIES PREFIX "")
    target_link_libraries(vsipreload PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ${CMAKE_DL_LIBS})
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
      set_property(SOURCE vsipreload.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
    endif ()

    # For musl / Alpine
    foreach(_header IN ITEMS stdio.h fcntl.h unistd.h dirent.h sys/stat.h)
      if(EXISTS "/usr/include/${_header}")
        file(READ "/usr/include/${_header}" _content)
        if("${_content}" MATCHES "#define fopen64 fopen")
          target_compile_definitions(vsipreload PRIVATE FOPEN64_ALIAS_OF_FOPEN)
        endif()
        if("${_content}" MATCHES "#define fseeko64 fseeko")
          target_compile_definitions(vsipreload PRIVATE FSEEKO64_ALIAS_OF_FSEEKO)
        endif()
        if("${_content}" MATCHES "#define ftello64 ftello")
          target_compile_definitions(vsipreload PRIVATE FTELLO64_ALIAS_OF_FTELLO)
        endif()
        if("${_content}" MATCHES "#define open64 open")
          target_compile_definitions(vsipreload PRIVATE OPEN64_ALIAS_OF_OPEN)
        endif()
        if("${_content}" MATCHES "#define lseek64 lseek")
          target_compile_definitions(vsipreload PRIVATE LSEEK64_ALIAS_OF_LSEEK)
        endif()
        if("${_content}" MATCHES "#define dirent64 dirent")
          target_compile_definitions(vsipreload PRIVATE DIRENT64_ALIAS_OF_DIRENT)
        endif()
        if("${_content}" MATCHES "#define lstat64 lstat")
          target_compile_definitions(vsipreload PRIVATE LSTAT64_ALIAS_OF_LSTAT)
        endif()
      endif()
    endforeach()
  endif ()
endif ()
