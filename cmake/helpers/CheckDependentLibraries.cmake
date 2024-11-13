# Distributed under the GDAL/OGR MIT style License.  See accompanying file LICENSE.TXT.

#[=======================================================================[.rst:
CheckDependentLibraries.cmake
-----------------------------

Detect GDAL dependencies and set variable HAVE_*

#]=======================================================================]

include(CheckDependentLibrariesCommon)

# Custom find_package definitions

define_find_package2(Crnlib crunch/crnlib.h crunch)

if (WIN32)
  gdal_check_package(ODBC "Enable DB support through ODBC" CAN_DISABLE)
else ()
  gdal_check_package(ODBC "Enable DB support through ODBC" COMPONENTS ODBCINST CAN_DISABLE)
endif ()
gdal_check_package(ODBCCPP "odbc-cpp library (external)" CAN_DISABLE)
gdal_check_package(MSSQL_NCLI "MSSQL Native Client to enable bulk copy" CAN_DISABLE)
gdal_check_package(MSSQL_ODBC "MSSQL ODBC driver to enable bulk copy" CAN_DISABLE)

gdal_check_package(MySQL "MySQL" CAN_DISABLE)

# basic libraries
gdal_check_package(CURL "Enable drivers to use web API" CAN_DISABLE RECOMMENDED VERSION 7.68)

gdal_check_package(Iconv "Character set recoding (used in GDAL portability library)" CAN_DISABLE)
if (Iconv_FOUND)
  set(CMAKE_REQUIRED_INCLUDES ${Iconv_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${Iconv_LIBRARY})

  set(ICONV_CONST_TEST_CODE
      "#include <stdlib.h>
    #include <iconv.h>
    int main(){
      iconv_t conv = 0;
      char* in = 0;
      size_t ilen = 0;
      char* out = 0;
      size_t olen = 0;
      size_t ret = iconv(conv, &in, &ilen, &out, &olen);
      return (size_t)ret;
    }")
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("${ICONV_CONST_TEST_CODE}" _ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  if (_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
    set(ICONV_CPP_CONST "")
  else ()
    set(ICONV_CPP_CONST "const")
  endif ()

  if (NOT CMAKE_CROSSCOMPILING)
      include(CheckCXXSourceRuns)
      set(ICONV_HAS_EXTRA_CHARSETS_CODE
"#include <stdlib.h>
#include <iconv.h>
int main(){
    iconv_t conv = iconv_open(\"UTF-8\", \"CP1251\");
    if( conv != (iconv_t)-1 )
    {
        iconv_close(conv);
        return 0;
    }
    return 1;
}")
      check_cxx_source_runs("${ICONV_HAS_EXTRA_CHARSETS_CODE}" ICONV_HAS_EXTRA_CHARSETS)
      if (NOT ICONV_HAS_EXTRA_CHARSETS)
          message(WARNING "ICONV is available but some character sets used by "
                          "some drivers are not available. "
                          "You may need to install an extra package "
                          "(e.g. 'glibc-gconv-extra' on Fedora)")
      endif()
  endif()

  unset(ICONV_CONST_TEST_CODE)
  unset(_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  unset(CMAKE_REQUIRED_FLAGS)
endif ()

if (HAVE_ICONV AND DEFINED GDAL_USE_ICONV AND NOT GDAL_USE_ICONV)
  set(HAVE_ICONV 0)
endif()

gdal_check_package(LibXml2 "Read and write XML formats" CAN_DISABLE)

gdal_check_package(EXPAT "Read and write XML formats" RECOMMENDED CAN_DISABLE
  NAMES expat
  TARGETS expat::expat EXPAT::EXPAT
)
if(EXPAT_FOUND AND NOT DEFINED EXPAT_TARGET)
    set(EXPAT_TARGET EXPAT::EXPAT)
endif()

gdal_check_package(XercesC "Read and write XML formats (needed for GMLAS and ILI drivers)" CAN_DISABLE)

include(CheckDependentLibrariesZLIB)

gdal_check_package(Deflate "Enable libdeflate compression library (complement to ZLib)" CAN_DISABLE)

gdal_check_package(OpenSSL "Use OpenSSL library" COMPONENTS SSL Crypto CAN_DISABLE)

gdal_check_package(CryptoPP "Use crypto++ library for CPL." CAN_DISABLE)
if (GDAL_USE_CRYPTOPP)
  option(CRYPTOPP_USE_ONLY_CRYPTODLL_ALG "Use Only cryptoDLL alg. only work on dynamic DLL" OFF)
endif ()

set(GDAL_FIND_PACKAGE_PROJ_MODE "CUSTOM" CACHE STRING "Mode to use for find_package(PROJ): CUSTOM, CONFIG, MODULE or empty string")
set_property(CACHE GDAL_FIND_PACKAGE_PROJ_MODE PROPERTY STRINGS "CUSTOM" "CONFIG" "MODULE" "")
if(NOT GDAL_FIND_PACKAGE_PROJ_MODE STREQUAL "CUSTOM")
    find_package(PROJ ${GDAL_FIND_PACKAGE_PROJ_MODE} REQUIRED)
    if (NOT BUILD_SHARED_LIBS)
        string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(PROJ ${GDAL_FIND_PACKAGE_PROJ_MODE})\n")
    endif()
else()
    # First check with CMake config files, and then fallback to the FindPROJ module.
    find_package(PROJ CONFIG)
    if (PROJ_FOUND AND PROJ_VERSION VERSION_LESS "8")
        message(WARNING "PROJ ${PROJ_VERSION} < 8 found with Config file. As it is not trusted, retrying with module mode")
    endif()
    if (PROJ_FOUND)
      if (NOT BUILD_SHARED_LIBS)
        string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(PROJ CONFIG)\n")
      endif()
    else()
      find_package(PROJ REQUIRED)
      if (NOT BUILD_SHARED_LIBS)
        string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(PROJ)\n")
      endif()
    endif ()
endif()
if (DEFINED PROJ_VERSION_STRING AND NOT DEFINED PROJ_VERSION)
    set(PROJ_VERSION ${PROJ_VERSION_STRING})
endif()
if ("${PROJ_VERSION}" VERSION_LESS "6.3")
    message(FATAL_ERROR "PROJ >= 6.3 required. Version ${PROJ_VERSION} found")
endif()

gdal_check_package(TIFF "Support for the Tag Image File Format (TIFF)." VERSION 4.1 CAN_DISABLE)
set_package_properties(
  TIFF PROPERTIES
  URL "https://libtiff.gitlab.io/libtiff/"
  DESCRIPTION "Support for the Tag Image File Format (TIFF)."
  TYPE RECOMMENDED)
gdal_internal_library(TIFF)

if (DEFINED ENV{CONDA_PREFIX} AND UNIX)
    # Currently on Unix, the Zstd cmake config file is buggy. It declares a
    # libzstd_static target but the corresponding libzstd.a file is missing,
    # which cause CMake to error out.
    set(ZSTD_NAMES_AND_TARGETS)
else()
    set(ZSTD_NAMES_AND_TARGETS NAMES zstd TARGETS zstd::libzstd_shared zstd::libzstd_static ZSTD::zstd)
endif()
gdal_check_package(ZSTD "ZSTD compression library" CAN_DISABLE ${ZSTD_NAMES_AND_TARGETS})

gdal_check_package(SFCGAL "gdal core supports ISO 19107:2013 and OGC Simple Features Access 1.2 for 3D operations"
                   CAN_DISABLE)

include(CheckDependentLibrariesGeoTIFF)

gdal_check_package(PNG "PNG compression library (external)" CAN_DISABLE RECOMMENDED VERSION "1.6")
gdal_internal_library(PNG)

include(CheckDependentLibrariesJpeg)

gdal_check_package(GIF "GIF compression library (external)" CAN_DISABLE)
gdal_internal_library(GIF)

gdal_check_package(JSONC "json-c library (external)" CAN_DISABLE
  NAMES json-c
  TARGETS json-c::json-c JSONC::JSONC
)
gdal_internal_library(JSONC REQUIRED)
if(TARGET json-c::json-c)
  get_target_property(include_dirs json-c::json-c INTERFACE_INCLUDE_DIRECTORIES)
  find_path(GDAL_JSON_INCLUDE_DIR NAMES json.h PATHS ${include_dirs} PATH_SUFFIXES json-c NO_DEFAULT_PATH)
  list(APPEND include_dirs "${GDAL_JSON_INCLUDE_DIR}")
  list(REMOVE_DUPLICATES include_dirs)
  set_target_properties(json-c::json-c PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GDAL_JSON_INCLUDE_DIR}"
  )
endif()

gdal_check_package(OpenCAD "libopencad (external, used by OpenCAD driver)" CAN_DISABLE)
gdal_internal_library(OPENCAD)

gdal_check_package(QHULL "Enable QHULL (external)" CAN_DISABLE RECOMMENDED)
gdal_internal_library(QHULL)

# libcsf upstream is now at https://github.com/pcraster/rasterformat, but the library name has been changed to
# pcraster_raster_format and it is forced as a static library (at least as of commit
# https://github.com/pcraster/rasterformat/commit/88fae8652fd36d878648f3fe303306c3dc68b7e6) So do not allow external
# libcsf for now define_find_package2(LIBCSF csf.h csf)

# gdal_check_package(LIBCSF "libcsf (external, used by PCRaster driver)" CAN_DISABLE) if (NOT GDAL_USE_LIBCSF)
# set(GDAL_USE_LIBCSF_INTERNAL ON CACHE BOOL "Set ON to build pcraster driver with internal libcsf (if set to ON, has
# precedence over GDAL_USE_LIBCSF)") else () set(GDAL_USE_LIBCSF_INTERNAL OFF CACHE BOOL "Set ON to build pcraster
# driver with internal libcsf (if set to ON, has precedence over GDAL_USE_LIBCSF)") endif ()
set(GDAL_USE_LIBCSF_INTERNAL ON)

# Compression used by GTiff and MRF
if( NOT WORDS_BIGENDIAN )
  gdal_check_package(LERC "Enable LERC (external)" CAN_DISABLE RECOMMENDED)
  gdal_internal_library(LERC)
endif()

gdal_check_package(BRUNSLI "Enable BRUNSLI for JPEG packing in MRF" CAN_DISABLE)

gdal_check_package(libQB3 "Enable QB3 compression in MRF" CONFIG CAN_DISABLE)

# Disable by default the use of external shapelib, as currently the SAOffset member that holds file offsets in it is a
# 'unsigned long', hence 32 bit on 32 bit platforms, whereas we can handle DBFs file > 4 GB. Internal shapelib has not
# this issue
gdal_check_package(Shapelib "Enable Shapelib support (not recommended, internal Shapelib is preferred)."
                   DISABLED_BY_DEFAULT)
if (NOT GDAL_USE_SHAPELIB)
  set(GDAL_USE_SHAPELIB_INTERNAL
      ON
      CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
else ()
  if (Shapelib_VERSION VERSION_LESS 1.4.0)
    message(STATUS "Detected Shapelib version ${Shapelib_VERSION} is too lower to support. Enables internal shapelib.")
    set(GDAL_USE_SHAPELIB_INTERNAL
        ON
        CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
  else ()
    set(GDAL_USE_SHAPELIB_INTERNAL
        OFF
        CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
  endif ()
endif ()

# 3rd party libraries

gdal_check_package(PCRE2 "Enable PCRE2 support for sqlite3" CAN_DISABLE)
if (NOT GDAL_USE_PCRE2)
  gdal_check_package(PCRE "Enable PCRE support for sqlite3" CAN_DISABLE)
endif ()

gdal_check_package(SQLite3 "Enable SQLite3 support (used by SQLite/Spatialite, GPKG, Rasterlite, MBTiles, etc.)"
                   CAN_DISABLE RECOMMENDED
                   VERSION 3.31)
if (SQLite3_FOUND)
  if (NOT DEFINED SQLite3_HAS_COLUMN_METADATA)
    message(FATAL_ERROR "missing SQLite3_HAS_COLUMN_METADATA")
  endif ()
  if (NOT DEFINED SQLite3_HAS_MUTEX_ALLOC)
    message(FATAL_ERROR "missing SQLite3_HAS_MUTEX_ALLOC")
  endif ()
  if (GDAL_USE_SQLITE3 AND NOT SQLite3_HAS_MUTEX_ALLOC)
    if (NOT ACCEPT_MISSING_SQLITE3_MUTEX_ALLOC)
      message(
        FATAL_ERROR
          "${SQLite3_LIBRARIES} lacks mutex support! Access to SQLite3 databases from multiple threads will be unsafe. Define the ACCEPT_MISSING_SQLITE3_MUTEX_ALLOC:BOOL=ON CMake variable if you want to build despite this limitation."
        )
    else ()
      message(WARNING "${SQLite3_LIBRARIES} lacks the mutex extension! Access to SQLite3 databases from multiple threads will be unsafe")
    endif ()
  endif ()
endif ()

# Checks that SQLite3 has RTree support
# Called by ogr/ogrsf_frmts/sqlite/CMakeLists.txt and ogr/ogrsf_frmts/gpkg/CMakeLists.txt
function (check_sqlite3_rtree driver_name)
  if (NOT DEFINED SQLite3_HAS_RTREE)
    message(FATAL_ERROR "missing SQLite3_HAS_RTREE")
  endif ()
  if (GDAL_USE_SQLITE3 AND NOT SQLite3_HAS_RTREE)
    if (NOT ACCEPT_MISSING_SQLITE3_RTREE)
      message(
        FATAL_ERROR
          "${SQLite3_LIBRARIES} lacks the RTree extension! ${driver_name} will not behave properly. Define the ACCEPT_MISSING_SQLITE3_RTREE:BOOL=ON CMake variable if you want to build despite this limitation."
        )
    else ()
      message(WARNING "${SQLite3_LIBRARIES} lacks the RTree extension! ${driver_name} will not behave properly.")
    endif ()
  endif ()
endfunction()

gdal_check_package(SPATIALITE "Enable spatialite support for sqlite3" VERSION 4.1.2 CAN_DISABLE)
gdal_check_package(RASTERLITE2 "Enable RasterLite2 support for sqlite3" VERSION 1.1.0 CAN_DISABLE)

gdal_check_package(LibKML "Use LIBKML library" COMPONENTS DOM ENGINE CAN_DISABLE)

define_find_package2(KEA libkea/KEACommon.h kea;libkea)
gdal_check_package(KEA "Enable KEA driver" CAN_DISABLE)

if(HAVE_KEA)
    # CXX is only needed for KEA driver
    gdal_check_package(HDF5 "Enable HDF5" COMPONENTS "C" "CXX" CAN_DISABLE VERSION 1.10)
else()
    gdal_check_package(HDF5 "Enable HDF5" COMPONENTS "C" CAN_DISABLE VERSION 1.10)
endif()

gdal_check_package(WebP "WebP compression" CAN_DISABLE)
gdal_check_package(FreeXL "Enable XLS driver" CAN_DISABLE)

define_find_package2(GTA gta/gta.h gta PKGCONFIG_NAME gta)
gdal_check_package(GTA "Enable GTA driver" CAN_DISABLE)

include(CheckDependentLibrariesMrSID)

set(GDAL_USE_ARMADILLO_OLD ${GDAL_USE_ARMADILLO})
gdal_check_package(Armadillo "C++ library for linear algebra (used for TPS transformation)" CAN_DISABLE)
if (ARMADILLO_FOUND)
  # On Conda, the armadillo package has no dependency on lapack, but the later is required for successful linking. So
  # try to build & link a test program using Armadillo.
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES "${ARMADILLO_INCLUDE_DIRS}")
  set(CMAKE_REQUIRED_LIBRARIES "${ARMADILLO_LIBRARIES}")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  set(CMAKE_TRY_COMPILE_CONFIGURATION "Release")
  check_cxx_source_compiles(
    "
        #include <armadillo>
        int main(int argc, char** argv) {
            arma::mat matInput(2,2);
            const arma::mat& matInv = arma::inv(matInput);
            return 0;
        }
    "
    ARMADILLO_TEST_PROGRAM_WITHOUT_LAPACK_COMPILES)
  unset(CMAKE_MSVC_RUNTIME_LIBRARY)
  unset(CMAKE_TRY_COMPILE_CONFIGURATION)
  cmake_pop_check_state()
  if (NOT ARMADILLO_TEST_PROGRAM_WITHOUT_LAPACK_COMPILES)
    find_package(LAPACK)
    if (LAPACK_FOUND)
      list(APPEND ARMADILLO_LIBRARIES ${LAPACK_LIBRARIES})
      cmake_push_check_state(RESET)
      set(CMAKE_REQUIRED_INCLUDES "${ARMADILLO_INCLUDE_DIRS}")
      set(CMAKE_REQUIRED_LIBRARIES "${ARMADILLO_LIBRARIES}")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
      set(CMAKE_TRY_COMPILE_CONFIGURATION "Release")
      check_cxx_source_compiles(
        "
        #include <armadillo>
        int main(int argc, char** argv) {
            arma::mat matInput(2,2);
            const arma::mat& matInv = arma::inv(matInput);
            return 0;
        }
        "
        ARMADILLO_TEST_PROGRAM_WITH_LAPACK_COMPILES)
      unset(CMAKE_MSVC_RUNTIME_LIBRARY)
      unset(CMAKE_TRY_COMPILE_CONFIGURATION)
      cmake_pop_check_state()
    endif ()
  endif ()

  if (GDAL_USE_ARMADILLO AND
      NOT ARMADILLO_TEST_PROGRAM_WITHOUT_LAPACK_COMPILES AND
      NOT ARMADILLO_TEST_PROGRAM_WITH_LAPACK_COMPILES)
    if (DEFINED ENV{CONDA_PREFIX})
        if (GDAL_USE_ARMADILLO_OLD)
          message(FATAL_ERROR
              "Armadillo found, but test program does not build. To enable Armadillo, you may need to install the following Conda-Forge packages: blas blas-devel libblas libcblas liblapack liblapacke")
        else()
          message(WARNING
              "Armadillo found, but test program does not build. Disabling it. To enable Armadillo, you may need to install the following Conda-Forge packages: blas blas-devel libblas libcblas liblapack liblapacke")
        endif()
    else ()
        if (GDAL_USE_ARMADILLO_OLD)
          message(FATAL_ERROR "Armadillo found, but test program does not build.")
        else()
          message(WARNING
              "Armadillo found, but test program does not build. Disabling it.")
        endif()
    endif ()
    unset(GDAL_USE_ARMADILLO CACHE)
    unset(GDAL_USE_ARMADILLO)
  endif ()

  # LAPACK support required for arma::solve()
  if (GDAL_USE_ARMADILLO AND EXISTS "${ARMADILLO_INCLUDE_DIRS}/armadillo_bits/config.hpp")
      file(READ "${ARMADILLO_INCLUDE_DIRS}/armadillo_bits/config.hpp" armadillo_config)
      if ("${armadillo_config}" MATCHES "/\\* #undef ARMA_USE_LAPACK")
          if (GDAL_USE_ARMADILLO_OLD)
              message(FATAL_ERROR "Armadillo build lacks LAPACK support")
          else()
              message(WARNING "Armadillo build lacks LAPACK support. Disabling it as it cannot be used by GDAL")
          endif()
          unset(GDAL_USE_ARMADILLO CACHE)
          unset(GDAL_USE_ARMADILLO)
      endif()
  endif()

endif ()

define_find_package2(CFITSIO fitsio.h cfitsio PKGCONFIG_NAME cfitsio)
gdal_check_package(CFITSIO "C FITS I/O library" CAN_DISABLE)

gdal_check_package(GEOS "Geometry Engine - Open Source (GDAL core dependency)" RECOMMENDED CAN_DISABLE
  VERSION 3.8
  NAMES GEOS
  TARGETS GEOS::geos_c GEOS::GEOS
)
gdal_check_package(HDF4 "Enable HDF4 driver" CAN_DISABLE)

include(CheckDependentLibrariesECW)

gdal_check_package(NetCDF "Enable netCDF driver" CAN_DISABLE
  NAMES netCDF
  TARGETS netCDF::netcdf NETCDF::netCDF
  VERSION "4.7")
gdal_check_package(OGDI "Enable ogr_OGDI driver" CAN_DISABLE)
gdal_check_package(OpenCL "Enable OpenCL (may be used for warping)" CAN_DISABLE)

set(PostgreSQL_ADDITIONAL_VERSIONS "14" CACHE STRING "Additional PostgreSQL versions to check")
gdal_check_package(PostgreSQL "" CAN_DISABLE)

gdal_check_package(FYBA "enable ogr_SOSI driver" CAN_DISABLE)
# Assume liblzma from xzutils, skip expensive checks.
set(LIBLZMA_HAS_AUTO_DECODER 1)
set(LIBLZMA_HAS_EASY_ENCODER 1)
set(LIBLZMA_HAS_LZMA_PRESET 1)
gdal_check_package(LibLZMA "LZMA compression" CAN_DISABLE)
gdal_check_package(LZ4 "LZ4 compression" CAN_DISABLE)
gdal_check_package(Blosc "Blosc compression" CAN_DISABLE)

define_find_package2(ARCHIVE archive.h archive)
gdal_check_package(ARCHIVE "Multi-format archive and compression library library (used for /vsi7z/" CAN_DISABLE)

define_find_package2(LIBAEC libaec.h aec)
gdal_check_package(LIBAEC "Adaptive Entropy Coding implementing Golomb-Rice algorithm (used by GRIB)" CAN_DISABLE)

define_find_package2(JXL jxl/decode.h jxl PKGCONFIG_NAME libjxl)
gdal_check_package(JXL "JPEG-XL compression" CAN_DISABLE)

define_find_package2(JXL_THREADS jxl/resizable_parallel_runner.h jxl_threads PKGCONFIG_NAME libjxl_threads)
gdal_check_package(JXL_THREADS "JPEG-XL threading" CAN_DISABLE)

# unused for now gdal_check_package(OpenMP "")
gdal_check_package(Crnlib "enable gdal_DDS driver" CAN_DISABLE)
gdal_check_package(basisu "Enable BASISU driver" CONFIG CAN_DISABLE)
gdal_check_package(IDB "enable ogr_IDB driver" CAN_DISABLE)
gdal_check_package(rdb "enable RIEGL RDB library" CONFIG CAN_DISABLE)
include(CheckDependentLibrariesTileDB)

gdal_check_package(OpenEXR "OpenEXR >=2.2" CAN_DISABLE)
gdal_check_package(MONGOCXX "Enable MongoDBV3 driver" CAN_DISABLE)

define_find_package2(HEIF libheif/heif.h heif PKGCONFIG_NAME libheif)
gdal_check_package(HEIF "HEIF >= 1.1" CAN_DISABLE)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "
    #include <libheif/heif.h>
    int main()
    {
        struct heif_image_tiling tiling;
        return 0;
    }
    "
    LIBHEIF_SUPPORTS_TILES
)
if (LIBHEIF_SUPPORTS_TILES)
  set_property(TARGET HEIF::HEIF APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "LIBHEIF_SUPPORTS_TILES")
endif ()

include(CheckDependentLibrariesAVIF)

include(CheckDependentLibrariesOpenJPEG)

gdal_check_package(HDFS "Enable Hadoop File System through native library" CAN_DISABLE)

# PDF library: one of them enables the read side of the PDF driver
gdal_check_package(Poppler "Enable PDF driver with Poppler (read side)" CAN_DISABLE VERSION 0.86)

define_find_package2(PDFIUM public/fpdfview.h pdfium FIND_PATH_SUFFIX pdfium)
gdal_check_package(PDFIUM "Enable PDF driver with Pdfium (read side)" CAN_DISABLE)

gdal_check_package(Podofo "Enable PDF driver with Podofo (read side)" CAN_DISABLE)


include(CheckDependentLibrariesOracle)
gdal_check_package(TEIGHA "Enable DWG and DGNv8 drivers" CAN_DISABLE)
gdal_check_package(FileGDB "Enable FileGDB (based on closed-source SDK) driver" CAN_DISABLE)

option(GDAL_USE_PUBLICDECOMPWT
       "Set ON to build MSG driver and download external https://gitlab.eumetsat.int/open-source/PublicDecompWT" OFF)

# proprietary libraries KAKADU
include(CheckDependentLibrariesKakadu)

include(CheckDependentLibrariesArrowParquet)

gdal_check_package(OpenDrive "Enable libOpenDRIVE" CONFIG CAN_DISABLE)

gdal_check_package(AdbcDriverManager "Enable ADBC" CONFIG CAN_DISABLE)

# bindings

# finding python in top of project because of common for autotest and bindings

set(JAVA_AWT_LIBRARY NotNeeded)
set(JAVA_AWT_INCLUDE_PATH NotNeeded)
find_package(JNI)
find_package(Java COMPONENTS Runtime Development)
find_program(
  ANT
  NAMES ant
  DOC "ant executable for Java binding")
set_package_properties(JNI PROPERTIES PURPOSE "SWIG_JAVA: Java binding")

find_package(CSharp)
set_package_properties(CSharp PROPERTIES PURPOSE "SWIG_CSharp: CSharp binding")

# vim: ts=4 sw=4 sts=4 et
