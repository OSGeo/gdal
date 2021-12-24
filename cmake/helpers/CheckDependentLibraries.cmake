# Distributed under the GDAL/OGR MIT/X style License.  See accompanying file LICENSE.TXT.

#[=======================================================================[.rst:
CheckDependentLibraries.cmake
-----------------------------

Detect GDAL dependencies and set variable HAVE_*

#]=======================================================================]

include(CheckFunctionExists)
include(CMakeDependentOption)
include(FeatureSummary)
include(DefineFindPackage2)
include(CheckSymbolExists)

# Macro to declare a package Accept a CAN_DISABLE option to specify that the package can be disabled if found, with the
# GDAL_USE_{name in upper case} option. Accept a DISABLED_BY_DEFAULT option to specify that the default value of
# GDAL_USE_ is OFF. Accept a RECOMMENDED option
macro (gdal_check_package name purpose)
  set(_options CONFIG CAN_DISABLE RECOMMENDED DISABLED_BY_DEFAULT)
  set(_oneValueArgs)
  set(_multiValueArgs COMPONENTS)
  cmake_parse_arguments(_GCP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  string(TOUPPER ${name} key)
  find_package2(${name} QUIET)
  if (NOT DEFINED ${key}_FOUND)
    if (_GCP_CONFIG)
      find_package(${name} CONFIG)
    elseif (_GCP_COMPONENTS)
      find_package(${name} COMPONENTS ${_GCP_COMPONENTS})
    else ()
      find_package(${name})
    endif ()
  endif ()
  if (${key}_FOUND OR ${name}_FOUND)
    set(HAVE_${key} ON)
  else ()
    set(HAVE_${key} OFF)
  endif ()
  if (purpose STREQUAL "")

  else ()
    if (_GCP_RECOMMENDED)
      set_package_properties(
        ${name} PROPERTIES
        PURPOSE ${purpose}
        TYPE RECOMMENDED)
    else ()
      set_package_properties(${name} PROPERTIES PURPOSE ${purpose})
    endif ()
  endif ()
  if (_GCP_CAN_DISABLE OR _GCP_DISABLED_BY_DEFAULT)
    if (GDAL_USE_${key})
      if (NOT HAVE_${key})
        message(FATAL_ERROR "Configured to use ${key}, but not found")
      endif ()
    endif ()
    set(_gcpp_status ON)
    if (_GCP_DISABLED_BY_DEFAULT)
      set(_gcpp_status OFF)
      if (HAVE_${key} AND NOT GDAL_USE_${key})
        message("${key} has been found, but is disabled by default. Enable it by setting GDAL_USE_${key}=ON")
      endif ()
    endif ()
    cmake_dependent_option(GDAL_USE_${key} "Set ON to use ${key}" ${_gcpp_status} "HAVE_${key}" OFF)
  endif ()
endmacro ()

function (split_libpath _lib)
  if (_lib)
    # split lib_line into -L and -l linker options
    get_filename_component(_path ${${_lib}} PATH)
    get_filename_component(_name ${${_lib}} NAME_WE)
    string(REGEX REPLACE "^lib" "" _name ${_name})
    set(${_lib} -L${_path} -l${_name})
  endif ()
endfunction ()

# Custom find_package definitions
define_find_package2(LIBCSF csf.h csf)
define_find_package2(Crnlib crunch/crnlib.h crunch)
define_find_package2(RASDAMAN rasdaman.hh raslib)
define_find_package2(FME fmeobjects/cpp/issesion.h fme)

if (WIN32)
  gdal_check_package(ODBC "Enable DB support through ODBC" CAN_DISABLE)
else ()
  gdal_check_package(ODBC "Enable DB support through ODBC" COMPONENTS ODBCINST CAN_DISABLE)
endif ()
gdal_check_package(MSSQL_NCLI "MSSQL Native Client to enable bulk copy" CAN_DISABLE)
gdal_check_package(MSSQL_ODBC "MSSQL ODBC driver to enable bulk copy" CAN_DISABLE)

gdal_check_package(MySQL "MySQL" CAN_DISABLE)

# basic libraries
gdal_check_package(CURL "Enable drivers to use web API" CAN_DISABLE)

gdal_check_package(Iconv "Character set recoding (used in GDAL portability library)" CAN_DISABLE)

if (Iconv_FOUND)
  set(CMAKE_REQUIRED_INCLUDES ${Iconv_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${Iconv_LIBRARY})
  if (MSVC)
    set(CMAKE_REQUIRED_FLAGS "/WX")
  else ()
    set(CMAKE_REQUIRED_FLAGS "-Werror")
  endif ()

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
  check_cxx_source_compiles("${ICONV_CONST_TEST_CODE}" _ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  if (_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
    set(ICONV_CPP_CONST "")
  else ()
    set(ICONV_CPP_CONST "const")
  endif ()
  unset(ICONV_CONST_TEST_CODE)
  unset(_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  unset(CMAKE_REQUIRED_FLAGS)
endif ()

gdal_check_package(LibXml2 "Read and write XML formats" CAN_DISABLE)

gdal_check_package(EXPAT "Read and write XML formats" RECOMMENDED)
gdal_check_package(XercesC "Read and write XML formats (needed for GMLAS and ILI drivers)" CAN_DISABLE)
if (HAVE_EXPAT OR GDAL_USE_XERCESC)
  set(HAVE_XMLPARSER ON)
else ()
  set(HAVE_XMLPARSER OFF)
endif ()

gdal_check_package(ZLIB "zlib (external)" CAN_DISABLE)
if (NOT GDAL_USE_ZLIB)
  set(GDAL_USE_LIBZ_INTERNAL
      ON
      CACHE BOOL "Use internal zlib copy (if set to ON, has precedence over GDAL_USE_ZLIB)")
  if (NOT GDAL_USE_LIBZ_INTERNAL)
    message(FATAL_ERROR "GDAL_USE_ZLIB or GDAL_USE_LIBZ_INTERNAL must be set to ON")
  endif ()
else ()
  set(GDAL_USE_LIBZ_INTERNAL
      OFF
      CACHE BOOL "Use internal zlib copy (if set to ON, has precedence over GDAL_USE_ZLIB)")
endif ()

gdal_check_package(Deflate "Enable libdeflate compression library (complement to ZLib)" CAN_DISABLE)

find_package(OpenSSL COMPONENTS Crypto SSL)
if (OPENSSL_FOUND)
  set(HAVE_OPENSSL
      ON
      CACHE INTERNAL "")
endif ()
gdal_check_package(CryptoPP "Use crypto++ library for CPL." CAN_DISABLE)
option(CRYPTOPPL_USE_ONLY_CRYPTODLL_ALG "Use Only cryptoDLL alg. only work on dynamic DLL" OFF)

find_package(PROJ 6.0 REQUIRED)

find_package(TIFF 4.0)
if (TIFF_FOUND)
  set(HAVE_TIFF ON)
  set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${TIFF_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${TIFF_LIBRARIES})
  set(GDAL_USE_LIBTIFF_INTERNAL
      OFF
      CACHE BOOL "")
endif ()
if (NOT HAVE_TIFF)
  set(GDAL_USE_LIBTIFF_INTERNAL
      ON
      CACHE BOOL "")
endif ()
set_package_properties(
  TIFF PROPERTIES
  URL "https://libtiff.gitlab.io/libtiff/"
  DESCRIPTION "support for the Tag Image File Format (TIFF)."
  TYPE RECOMMENDED)

gdal_check_package(ZSTD "ZSTD compression library" CAN_DISABLE)
gdal_check_package(SFCGAL "gdal core supports ISO 19107:2013 and OGC Simple Features Access 1.2 for 3D operations"
                   CAN_DISABLE)

gdal_check_package(GeoTIFF "libgeotiff library (external)" CAN_DISABLE)
if (NOT GDAL_USE_GEOTIFF)
  set(GDAL_USE_LIBGEOTIFF_INTERNAL
      ON
      CACHE BOOL "Use internal libgeotiff copy (if set to ON, has precedence over GDAL_USE_GEOTIFF)")
  if (NOT GDAL_USE_LIBGEOTIFF_INTERNAL)
    message(FATAL_ERROR "GDAL_USE_GEOTIFF or GDAL_USE_LIBGEOTIFF_INTERNAL must be set to ON")
  endif ()
else ()
  set(GDAL_USE_LIBGEOTIFF_INTERNAL
      OFF
      CACHE BOOL "Use internal libgeotiff copy (if set to ON, has precedence over GDAL_USE_GEOTIFF)")
endif ()

gdal_check_package(PNG "PNG compression library (external)" CAN_DISABLE)
if (NOT GDAL_USE_PNG)
  set(GDAL_USE_LIBPNG_INTERNAL
      ON
      CACHE BOOL "Use internal libpng copy (if set to ON, has precedence over GDAL_USE_PNG)")
else ()
  set(GDAL_USE_LIBPNG_INTERNAL
      OFF
      CACHE BOOL "Use internal libpng copy (if set to ON, has precedence over GDAL_USE_PNG)")
endif ()

gdal_check_package(JPEG "JPEG compression library (external)" CAN_DISABLE)
if (NOT GDAL_USE_JPEG)
  set(GDAL_USE_LIBJPEG_INTERNAL
      ON
      CACHE BOOL "Use internal libjpeg copy (if set to ON, has precedence over GDAL_USE_JPEG)")
else ()
  set(GDAL_USE_LIBJPEG_INTERNAL
      OFF
      CACHE BOOL "Use internal libjpeg copy (if set to ON, has precedence over GDAL_USE_JPEG)")
endif ()

gdal_check_package(GIF "GIF compression library (external)" CAN_DISABLE)
if (NOT GDAL_USE_GIF)
  set(GDAL_USE_GIFLIB_INTERNAL
      ON
      CACHE BOOL "Use internal giflib copy (if set to ON, has precedence over GDAL_USE_GIF)")
else ()
  set(GDAL_USE_GIFLIB_INTERNAL
      OFF
      CACHE BOOL "Use internal giflib copy (if set to ON, has precedence over GDAL_USE_GIF)")
endif ()

gdal_check_package(JSONC "json-c library (external)" CAN_DISABLE)
if (NOT GDAL_USE_JSONC)
  set(GDAL_USE_LIBJSONC_INTERNAL
      ON
      CACHE BOOL "Use internal libjson-c copy (if set to ON, has precedence over GDAL_USE_JSONC)")
  if (NOT GDAL_USE_LIBJSONC_INTERNAL)
    message(FATAL_ERROR "GDAL_USE_JSONC or GDAL_USE_LIBJSONC_INTERNAL must be set to ON")
  endif ()
else ()
  set(GDAL_USE_LIBJSONC_INTERNAL
      OFF
      CACHE BOOL "Use internal libjson-c copy (if set to ON, has precedence over GDAL_USE_JSONC)")
endif ()

gdal_check_package(OpenCAD "libopencad (external, used by OpenCAD driver)" CAN_DISABLE)
if (NOT GDAL_USE_OPENCAD)
  set(GDAL_USE_OPENCAD_INTERNAL
      ON
      CACHE BOOL "Use internal libopencad copy (if set to ON, has precedence over GDAL_USE_OPENCAD)")
else ()
  set(GDAL_USE_OPENCAD_INTERNAL
      OFF
      CACHE BOOL "Use internal libopencad copy (if set to ON, has precedence over GDAL_USE_OPENCAD)")
endif ()

gdal_check_package(QHULL "Enable QHULL (external)" CAN_DISABLE)
if (NOT GDAL_USE_QHULL)
  set(GDAL_USE_QHULL_INTERNAL
      ON
      CACHE BOOL "Use internal QHULL copy (if set to ON, has precedence over GDAL_USE_QHULL)")
  if (NOT GDAL_USE_QHULL_INTERNAL)
    message(FATAL_ERROR "GDAL_USE_QHULL or GDAL_USE_QHULL_INTERNAL must be set to ON")
  endif ()
else ()
  set(GDAL_USE_QHULL_INTERNAL
      OFF
      CACHE BOOL "Use internal QHULL copy (if set to ON, has precedence over GDAL_USE_QHULL)")
endif ()

gdal_check_package(LIBCSF "libcsf (external, used by PCRaster driver)" CAN_DISABLE)
if (NOT GDAL_USE_LIBCSF)
  set(GDAL_USE_LIBCSF_INTERNAL
      ON
      CACHE BOOL
            "Set ON to build pcraster driver with internal libcsf (if set to ON, has precedence over GDAL_USE_LIBCSF)")
else ()
  set(GDAL_USE_LIBCSF_INTERNAL
      OFF
      CACHE BOOL
            "Set ON to build pcraster driver with internal libcsf (if set to ON, has precedence over GDAL_USE_LIBCSF)")
endif ()

option(GDAL_USE_LIBLERC_INTERNAL "Set ON to build mrf driver with internal libLERC" ON)

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
                   CAN_DISABLE RECOMMENDED)
if (SQLite3_FOUND)
  if (NOT DEFINED SQLite3_HAS_COLUMN_METADATA)
    message(FATAL_ERROR "missing SQLite3_HAS_COLUMN_METADATA")
  endif ()
  if (NOT DEFINED SQLite3_HAS_RTREE)
    message(FATAL_ERROR "missing SQLite3_HAS_RTREE")
  endif ()
  if (GDAL_USE_SQLITE3 AND NOT SQLite3_HAS_RTREE)
    if (NOT ACCEPT_MISSING_SQLITE3_RTREE)
      message(
        FATAL_ERROR
          "${SQLite3_LIBRARIES} lacks the RTree extension! Spatialite and GPKG will not behave properly. Define ACCEPT_MISSING_SQLITE3_RTREE:BOOL=ON option if you want to build despite this limitation."
        )
    else ()
      message(WARNING "${SQLite3_LIBRARIES} lacks the RTree extension! Spatialite and GPKG will not behave properly.")
    endif ()
  endif ()
endif ()

gdal_check_package(SPATIALITE "Enable spatialite support for sqlite3" CAN_DISABLE)
gdal_check_package(RASTERLITE2 "Enable RasterLite2 support for sqlite3" CAN_DISABLE)

set(HAVE_RASTERLITE2 ${RASTERLITE2_FOUND})
if (RASTERLITE2_FOUND AND NOT RASTERLITE2_VERSION_STRING STREQUAL "unknown")
  if (NOT RASTERLITE2_VERSION_STRING VERSION_GREATER_EQUAL 1.1.0)
    message(STATUS "Rasterlite2 requires version 1.1.0 and later, detected: ${RASTERLITE2_VERSION_STRING}")
    message(STATUS "Turn off rasterlite2 support")
    set(HAVE_RASTERLITE2
        OFF
        CACHE INTERNAL "HAVE_RASTERLITE2")
  endif ()
endif ()
if (GDAL_USE_RASTERLITE2)
  if (NOT HAVE_RASTERLITE2)
    message(FATAL_ERROR "Configured to use GDAL_USE_RASTERLITE2, but not found")
  endif ()
endif ()
cmake_dependent_option(GDAL_USE_RASTERLITE2 "Set ON to use Rasterlite2" ON HAVE_RASTERLITE2 OFF)

find_package(LibKML COMPONENTS DOM ENGINE)
if (GDAL_USE_LIBKML)
  if (NOT LibKML_FOUND)
    message(FATAL_ERROR "Configured to use GDAL_USE_LIBKML, but not found")
  endif ()
endif ()
cmake_dependent_option(GDAL_USE_LIBKML "Set ON to use LibKML" ON LibKML_FOUND OFF)

gdal_check_package(Jasper "Enable JPEG2000 support" CAN_DISABLE)
if (HAVE_JASPER)
  # Detect GeoJP2 UUID hack
  include(CheckCSourceCompiles)
  set(CMAKE_REQUIRED_QUIET "yes")
  set(CMAKE_REQUIRED_LIBRARIES jasper)
  check_c_source_compiles(
    "#ifdef __cplusplus\nextern \"C\"\n#endif\n char jp2_encode_uuid ();int main () {return jp2_encode_uuid ();;return 0;}"
    HAVE_JASPER_UUID)
  unset(CMAKE_REQUIRED_QUIET)
  unset(CMAKE_REQUIRED_LIBRARIES)
  if (HAVE_JASPER_UUID)
    message(STATUS "Jasper GeoJP2 UUID hack detected.")
    if (TARGET JASPER::Jasper)
      set_property(
        TARGET JASPER::Jasper
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS "HAVE_JASPER_UUID")
    endif ()
  endif ()
endif ()

# CXX is only needed for KEA driver
gdal_check_package(HDF5 "Enable HDF5" COMPONENTS "C" "CXX" CAN_DISABLE)

gdal_check_package(WebP "WebP compression" CAN_DISABLE)
gdal_check_package(FreeXL "Enable XLS driver" CAN_DISABLE)

define_find_package2(GTA gta/gta.h gta PKGCONFIG_NAME gta)
gdal_check_package(GTA "Enable GTA driver" CAN_DISABLE)

gdal_check_package(MRSID "")
gdal_check_package(DAP "Data Access Protocol library for server and client." CAN_DISABLE)
gdal_check_package(Armadillo "C++ library for linear algebra (used for TPS transformation)" CAN_DISABLE)
if (ARMADILLO_FOUND)
  # On Conda, the armadillo package has no dependency on lapack, but the later is required for successful linking. So
  # try to build & link a test program using Armadillo.
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES "${ARMADILLO_INCLUDE_DIRS}")
  set(CMAKE_REQUIRED_LIBRARIES "${ARMADILLO_LIBRARIES}")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedReleaseDLL")
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
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedReleaseDLL")
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
  if (NOT ARMADILLO_TEST_PROGRAM_WITHOUT_LAPACK_COMPILES AND NOT ARMADILLO_TEST_PROGRAM_WITH_LAPACK_COMPILES)
    message(WARNING "Armadillo found, but test program does not build. Disabling it.")
    if (DEFINED ENV{CONDA_PREFIX})
      message(
        WARNING
          "To enable Armadillo, you may need to install the following Conda-Forge packages: blas blas-devel libblas libcblas liblapack liblapacke"
        )
    endif ()
    set(GDAL_USE_ARMADILLO CACHE BOOL OFF FORCE)
  endif ()

endif ()

define_find_package2(CFITSIO fitsio.h cfitsio PKGCONFIG_NAME cfitsio)
gdal_check_package(CFITSIO "C FITS I/O library" CAN_DISABLE)

gdal_check_package(GEOS "Geometry Engine - Open Source (GDAL core dependency)" RECOMMENDED CAN_DISABLE)
gdal_check_package(HDF4 "Enable HDF4 driver" CAN_DISABLE)

define_find_package2(KEA libkea/KEACommon.h kea)
gdal_check_package(KEA "Enable KEA driver" CAN_DISABLE)

gdal_check_package(ECW "Enable ECW driver")
gdal_check_package(NetCDF "Enable netCDF driver" CAN_DISABLE)
gdal_check_package(OGDI "Enable ogr_OGDI driver")
# OpenCL warping gives different results than the ones expected by autotest, so disable it by default even if found.
gdal_check_package(OpenCL "Enable OpenCL (may be used for warping)" DISABLED_BY_DEFAULT)
gdal_check_package(PostgreSQL "" CAN_DISABLE)
gdal_check_package(FYBA "enable ogr_SOSI driver" CAN_DISABLE)
gdal_check_package(LibLZMA "LZMA compression" CAN_DISABLE)
gdal_check_package(LZ4 "LZ4 compression" CAN_DISABLE)
gdal_check_package(Blosc "Blosc compression" CAN_DISABLE)

define_find_package2(JXL jxl/decode.h jxl PKGCONFIG_NAME libjxl)
gdal_check_package(JXL "JPEG-XL compression (when used with internal libtiff)" CAN_DISABLE)

gdal_check_package(CharLS "enable gdal_JPEGLS jpeg loss-less driver" CAN_DISABLE)
# unused for now gdal_check_package(OpenMP "")
gdal_check_package(Crnlib "enable gdal_DDS driver")
gdal_check_package(IDB "enable ogr_IDB driver")
# TODO: implement FindRASDAMAN libs: -lrasodmg -lclientcomm -lcompression -lnetwork -lraslib
gdal_check_package(RASDAMAN "enable rasdaman driver")
gdal_check_package(rdb "enable RIEGL RDB library" CONFIG CAN_DISABLE)
gdal_check_package(TileDB "enable TileDB driver" CONFIG CAN_DISABLE)
gdal_check_package(OpenEXR "OpenEXR >=2.2" CAN_DISABLE)
gdal_check_package(MONGOCXX "Enable MongoDBV3 driver" CAN_DISABLE)

define_find_package2(HEIF libheif/heif.h heif PKGCONFIG_NAME libheif)
gdal_check_package(HEIF "HEIF >= 1.1" CAN_DISABLE)

# OpenJPEG's cmake-CONFIG is broken, so call module explicitly
find_package(OpenJPEG MODULE)
if (GDAL_USE_OPENJPEG)
  if (NOT OPENJPEG_FOUND)
    message(FATAL_ERROR "Configured to use GDAL_USE_OPENJPEG, but not found")
  endif ()
endif ()
cmake_dependent_option(GDAL_USE_OPENJPEG "Set ON to use openjpeg" ON OPENJPEG_FOUND OFF)

# Only GRASS 7 is currently supported but we keep dual version support in cmake for possible future switch to GRASS 8.
set(TMP_GRASS OFF)
foreach (GRASS_SEARCH_VERSION 7)
  # Cached variables: GRASS7_FOUND, GRASS_PREFIX7, GRASS_INCLUDE_DIR7 HAVE_GRASS: TRUE if at least one version of GRASS
  # was found
  set(GRASS_CACHE_VERSION ${GRASS_SEARCH_VERSION})
  if (WITH_GRASS${GRASS_CACHE_VERSION})
    find_package(GRASS ${GRASS_SEARCH_VERSION} MODULE)
    if (${GRASS${GRASS_CACHE_VERSION}_FOUND})
      set(GRASS_PREFIX${GRASS_CACHE_VERSION}
          ${GRASS_PREFIX${GRASS_SEARCH_VERSION}}
          CACHE PATH "Path to GRASS ${GRASS_SEARCH_VERSION} base directory")
      set(TMP_GRASS ON)
    endif ()
  endif ()
endforeach ()
if (TMP_GRASS)
  set(HAVE_GRASS
      ON
      CACHE INTERNAL "HAVE_GRASS")
else ()
  set(HAVE_GRASS
      OFF
      CACHE INTERNAL "HAVE_GRASS")
endif ()
unset(TMP_GRASS)

# PDF library: one of them enables PDF driver
gdal_check_package(Poppler "Enable PDF driver (read side)" CAN_DISABLE)

define_find_package2(PDFium public/fpdfview.h pdfium FIND_PATH_SUFFIX pdfium)
gdal_check_package(PDFium "Enable PDF driver (read side)" CAN_DISABLE)

gdal_check_package(Podofo "Enable PDF driver (read side)" CAN_DISABLE)
if (GDAL_USE_POPPLER
    OR GDAL_USE_PDFIUM
    OR GDAL_USE_PODOFO)
  set(HAVE_PDFLIB ON)
else ()
  set(HAVE_PDFLIB OFF)
endif ()

set(Oracle_CAN_USE_CLNTSH_AS_MAIN_LIBRARY ON)
gdal_check_package(Oracle "Enable Oracle OCI driver")
gdal_check_package(TEIGHA "Enable DWG and DGNv8 drivers" CAN_DISABLE)
gdal_check_package(FileGDB "Enable FileGDB (based on closed-source SDK) driver" CAN_DISABLE)

option(GDAL_USE_MSG
       "Set ON to build MSG driver and download external https://gitlab.eumetsat.int/open-source/PublicDecompWT" OFF)

# proprietary libraries KAKADU
gdal_check_package(KDU "Enable KAKADU" CAN_DISABLE)
gdal_check_package(LURATECH "Enable JP2Lura driver" CAN_DISABLE)
gdal_check_package(FME "FME")
gdal_check_package(IDB "Informix DataBlade client")

# bindings
gdal_check_package(SWIG "Enable language bindings")
set_package_properties(
  SWIG PROPERTIES
  DESCRIPTION
    "software development tool that connects programs written in C and C++ with a variety of high-level programming languages."
  URL "http://swig.org/"
  TYPE RECOMMENDED)

# finding python in top of project because of common for autotest and bindings

find_package(Perl)
set_package_properties(Perl PROPERTIES PURPOSE "SWIG_PERL: Perl binding")

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
