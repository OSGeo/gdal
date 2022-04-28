# Distributed under the GDAL/OGR MIT style License.  See accompanying file LICENSE.TXT.

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

option(
  GDAL_USE_EXTERNAL_LIBS
  "Whether detected external libraries should be used by default. This should be set before CMakeCache.txt is created."
  ON)

set(GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES ON OFF WHEN_NO_EXTERNAL)
set(
  GDAL_USE_INTERNAL_LIBS WHEN_NO_EXTERNAL
  CACHE STRING "Control how internal libraries should be used by default. This should be set before CMakeCache.txt is created.")
set_property(CACHE GDAL_USE_INTERNAL_LIBS PROPERTY STRINGS ${GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES})
if(NOT GDAL_USE_INTERNAL_LIBS IN_LIST GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES)
    message(FATAL_ERROR "GDAL_USE_INTERNAL_LIBS must be one of ${GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES}")
endif()

set(GDAL_IMPORT_DEPENDENCIES [[
include(CMakeFindDependencyMacro)
include("${CMAKE_CURRENT_LIST_DIR}/DefineFindPackage2.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GdalFindModulePath.cmake")
]])
if(TARGET Threads::Threads)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(Threads)\n")
endif()

# Check that the configuration has a valid value for INTERFACE_INCLUDE_DIRECTORIES. This aimed at avoiding issues like
# https://github.com/OSGeo/gdal/issues/5324
function (gdal_check_target_is_valid target res_var)
  get_target_property(_interface_include_directories ${target} "INTERFACE_INCLUDE_DIRECTORIES")
  if(_interface_include_directories)
    foreach(_dir IN LISTS _interface_include_directories)
      if(NOT EXISTS "${_dir}")
        message(WARNING "Target ${target} references ${_dir} as a INTERFACE_INCLUDE_DIRECTORIES, but it does not exist. Ignoring that target.")
        set(${res_var} FALSE PARENT_SCOPE)
        return()
      endif()
    endforeach()
  elseif("${target}" STREQUAL "geotiff_library" AND DEFINED GeoTIFF_INCLUDE_DIRS)
    # geotiff-config.cmake of GeoTIFF 1.7.0 doesn't define a INTERFACE_INCLUDE_DIRECTORIES
    # property, but a GeoTIFF_INCLUDE_DIRS variable.
    set_target_properties(${target} PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${GeoTIFF_INCLUDE_DIRS})
  else()
     message(WARNING "Target ${target} has no INTERFACE_INCLUDE_DIRECTORIES property. Ignoring that target.")
     set(${res_var} FALSE PARENT_SCOPE)
     return()
  endif()
  set(${res_var} TRUE PARENT_SCOPE)
endfunction()

# Package acceptance based on a candidate target list.
# If a matching target is found, sets ${name}_FOUND to TRUE,
# ${name}_INCLUDE_DIRS to "" and ${name}_LIBRARIES to the target name.
# If `REQUIRED` is used, ${name}_FOUND is set to FALSE if no target matches.
function(gdal_check_package_target name)
  if("REQUIRED" IN_LIST ARGN)
    list(REMOVE_ITEM ARGN "REQUIRED")
    set(${name}_FOUND FALSE PARENT_SCOPE)
  endif()
  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      gdal_check_target_is_valid(${target} _is_valid)
      if (_is_valid)
        set(${name}_TARGET "${target}" PARENT_SCOPE)
        set(${name}_FOUND TRUE PARENT_SCOPE)
        return()
      endif()
    endif()
  endforeach()
endfunction()

# Macro to declare a dependency on an external package.
# If not marked with the ALWAYS_ON_WHEN_FOUND option, dependencies can be
# marked for user control with either the CAN_DISABLE or DISABLED_BY_DEFAULT
# option. User control is done via a cache variable GDAL_USE_{name in upper case}
# with the default value ON for CAN_DISABLE or OFF for DISABLED_BY_DEFAULT.
# The RECOMMENDED option is used for the feature summary.
# The VERSION, CONFIG, COMPONENTS and NAMES parameters are passed to find_package().
# Using NAMES with find_package() implies config mode. However, gdal_check_package()
# attempts another find_package() without NAMES if the config mode attempt was not
# successful, allowing a fallback to Find modules.
# The TARGETS parameter can define a list of candidate targets. If given, a
# package will only be accepted if it defines one of the given targets. The matching
# target name will be saved in ${name}_TARGET.
# The NAMES and TARGETS map to GDAL_CHECK_PACKAGE_${name}_NAMES and
# GDAL_CHECK_PACKAGE_${name}_TARGETS cache variables which can be used to
# overwrite the default config and targets names.
# The required find_dependency() commands for exported config are appended to
# the GDAL_IMPORT_DEPENDENCIES string.
macro (gdal_check_package name purpose)
  set(_options CONFIG CAN_DISABLE RECOMMENDED DISABLED_BY_DEFAULT ALWAYS_ON_WHEN_FOUND)
  set(_oneValueArgs VERSION NAMES)
  set(_multiValueArgs COMPONENTS TARGETS PATHS)
  cmake_parse_arguments(_GCP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  string(TOUPPER ${name} key)
  set(_find_dependency "")
  set(_find_dependency_args "")
  find_package2(${name} QUIET OUT_DEPENDENCY _find_dependency)
  if (NOT DEFINED ${key}_FOUND)
    set(_find_package_args)
    if (_GCP_VERSION)
      list(APPEND _find_package_args ${_GCP_VERSION})
    endif ()
    if (_GCP_CONFIG)
      list(APPEND _find_package_args CONFIG)
    endif ()
    if (_GCP_COMPONENTS)
      list(APPEND _find_package_args COMPONENTS ${_GCP_COMPONENTS})
    endif ()
    if (_GCP_PATHS)
      list(APPEND _find_package_args PATHS ${_GCP_PATHS})
    endif ()
    if (_GCP_NAMES)
      set(GDAL_CHECK_PACKAGE_${name}_NAMES "${_GCP_NAMES}" CACHE STRING "Config file name for ${name}")
      mark_as_advanced(GDAL_CHECK_PACKAGE_${name}_NAMES)
    endif ()
    if (_GCP_TARGETS)
      set(GDAL_CHECK_PACKAGE_${name}_TARGETS "${_GCP_TARGETS}" CACHE STRING "Target name candidates for ${name}")
      mark_as_advanced(GDAL_CHECK_PACKAGE_${name}_TARGETS)
    endif ()
    if (GDAL_CHECK_PACKAGE_${name}_NAMES)
      find_package(${name} NAMES ${GDAL_CHECK_PACKAGE_${name}_NAMES} ${_find_package_args})
      gdal_check_package_target(${name} ${GDAL_CHECK_PACKAGE_${name}_TARGETS} REQUIRED)
      if (${name}_FOUND)
        get_filename_component(_find_dependency_args "${${name}_CONFIG}" NAME)
        string(REPLACE ";" " " _find_dependency_args "${name} NAMES ${GDAL_CHECK_PACKAGE_${name}_NAMES} CONFIGS ${_find_dependency_args} ${_find_package_args}")
      endif ()
    endif ()
    if (NOT ${name}_FOUND)
      find_package(${name} ${_find_package_args})
      if (${name}_FOUND)
        gdal_check_package_target(${name} ${GDAL_CHECK_PACKAGE_${name}_TARGETS})
      elseif (${key}_FOUND) # Some find modules do not set <Pkg>_FOUND
        gdal_check_package_target(${key} ${GDAL_CHECK_PACKAGE_${name}_TARGETS})
        set(${name}_FOUND "${key}_FOUND")
      endif ()
      if (${name}_FOUND)
        string(REPLACE ";" " " _find_dependency_args "${name} ${_find_package_args}")
      endif()
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
    set(_gcpp_status ON)
    if (GDAL_USE_${key})
      if (NOT HAVE_${key})
        message(FATAL_ERROR "Configured to use ${key}, but not found")
      endif ()
    elseif (NOT GDAL_USE_EXTERNAL_LIBS)
      set(_gcpp_status OFF)
      if (HAVE_${key} AND NOT GDAL_USE_${key})
        message(STATUS
          "${key} has been found, but is disabled due to GDAL_USE_EXTERNAL_LIBS=OFF. Enable it by setting GDAL_USE_${key}=ON"
          )
        set(_find_dependency_args "")
      endif ()
    endif ()
    if (_gcpp_status AND _GCP_DISABLED_BY_DEFAULT)
      set(_gcpp_status OFF)
      if (HAVE_${key} AND NOT GDAL_USE_${key})
        message(STATUS "${key} has been found, but is disabled by default. Enable it by setting GDAL_USE_${key}=ON")
        set(_find_dependency_args "")
      endif ()
    endif ()
    cmake_dependent_option(GDAL_USE_${key} "Set ON to use ${key}" ${_gcpp_status} "HAVE_${key}" OFF)
  elseif (NOT _GCP_ALWAYS_ON_WHEN_FOUND)
    message(FATAL_ERROR "Programming error: missing CAN_DISABLE or DISABLED_BY_DEFAULT option for component ${name}")
  endif ()

  if(_find_dependency_args)
    string(REPLACE "\"" "\\\"" _find_dependency_args "${_find_dependency_args}")
    set(_find_dependency "find_dependency(${_find_dependency_args})\n")
  endif()
  if(NOT BUILD_SHARED_LIBS AND GDAL_USE_${key} AND _find_dependency)
    string(APPEND GDAL_IMPORT_DEPENDENCIES "${_find_dependency}")
  endif()
  unset(_find_dependency_args)
  unset(_find_dependency)
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

function (gdal_internal_library libname)
  set(_options REQUIRED)
  set(_oneValueArgs)
  set(_multiValueArgs)
  cmake_parse_arguments(_GIL "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if ("${GDAL_USE_INTERNAL_LIBS}" STREQUAL "ON")
      set(_default_value ON)
  elseif ("${GDAL_USE_INTERNAL_LIBS}" STREQUAL "OFF")
      set(_default_value OFF)
  elseif( GDAL_USE_${libname} )
      set(_default_value OFF)
  else()
      set(_default_value ON)
  endif()
  set(GDAL_USE_${libname}_INTERNAL
      ${_default_value}
      CACHE BOOL "Use internal ${libname} copy (if set to ON, has precedence over GDAL_USE_${libname})")
  if (_GIL_REQUIRED
      AND (NOT GDAL_USE_${libname})
      AND (NOT GDAL_USE_${libname}_INTERNAL))
    message(FATAL_ERROR "GDAL_USE_${libname} or GDAL_USE_${libname}_INTERNAL must be set to ON")
  endif ()
endfunction ()

# Custom find_package definitions

define_find_package2(Crnlib crunch/crnlib.h crunch)
define_find_package2(RASDAMAN rasdaman.hh raslib)

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

gdal_check_package(ZLIB "zlib (external)" CAN_DISABLE)
gdal_internal_library(ZLIB REQUIRED)

gdal_check_package(Deflate "Enable libdeflate compression library (complement to ZLib)" CAN_DISABLE)

gdal_check_package(OpenSSL "Use OpenSSL library" COMPONENTS SSL Crypto CAN_DISABLE)

gdal_check_package(CryptoPP "Use crypto++ library for CPL." CAN_DISABLE)
if (GDAL_USE_CRYPTOPP)
  option(CRYPTOPP_USE_ONLY_CRYPTODLL_ALG "Use Only cryptoDLL alg. only work on dynamic DLL" OFF)
endif ()

# First check with CMake config files (starting at version 8, due to issues with earlier ones), and then fallback to the FindPROJ module.
find_package(PROJ 9 CONFIG QUIET)
if (NOT PROJ_FOUND)
  find_package(PROJ 8 CONFIG QUIET)
endif()
if (PROJ_FOUND)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(PROJ ${PROJ_VERSION_MAJOR} CONFIG)\n")
else()
  find_package(PROJ 6.0 REQUIRED)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(PROJ 6.0)\n")
endif ()

gdal_check_package(TIFF "Support for the Tag Image File Format (TIFF)." VERSION 4.0 CAN_DISABLE)
set_package_properties(
  TIFF PROPERTIES
  URL "https://libtiff.gitlab.io/libtiff/"
  DESCRIPTION "Support for the Tag Image File Format (TIFF)."
  TYPE RECOMMENDED)
gdal_internal_library(TIFF REQUIRED)

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

gdal_check_package(GeoTIFF "libgeotiff library (external)" CAN_DISABLE RECOMMENDED
  NAMES GeoTIFF
  TARGETS geotiff_library GEOTIFF::GEOTIFF
)
gdal_internal_library(GEOTIFF REQUIRED)

gdal_check_package(PNG "PNG compression library (external)" CAN_DISABLE RECOMMENDED)
gdal_internal_library(PNG)

gdal_check_package(JPEG "JPEG compression library (external)" CAN_DISABLE RECOMMENDED)
if (GDAL_USE_JPEG AND (JPEG_LIBRARY MATCHES ".*turbojpeg\.(so|lib)"))
  message(
    FATAL_ERROR
      "JPEG_LIBRARY should point to a library with libjpeg ABI, not TurboJPEG. See https://libjpeg-turbo.org/About/TurboJPEG for the difference"
    )
endif ()
gdal_internal_library(JPEG)

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
gdal_check_package(LERC "Enable LERC (external)" CAN_DISABLE RECOMMENDED)
gdal_internal_library(LERC)

gdal_check_package(BRUNSLI "Enable BRUNSLI for JPEG packing in MRF" CAN_DISABLE RECOMMENDED)

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
if (GDAL_USE_LIBKML)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(LibKML COMPONENTS DOM ENGINE)\n")
endif ()

# CXX is only needed for KEA driver
gdal_check_package(HDF5 "Enable HDF5" COMPONENTS "C" "CXX" CAN_DISABLE)

gdal_check_package(WebP "WebP compression" CAN_DISABLE)
gdal_check_package(FreeXL "Enable XLS driver" CAN_DISABLE)

define_find_package2(GTA gta/gta.h gta PKGCONFIG_NAME gta)
gdal_check_package(GTA "Enable GTA driver" CAN_DISABLE)

gdal_check_package(MRSID "MrSID raster SDK" CAN_DISABLE)
gdal_check_package(DAP "Data Access Protocol library for server and client." CAN_DISABLE)
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

gdal_check_package(GEOS "Geometry Engine - Open Source (GDAL core dependency)" RECOMMENDED CAN_DISABLE
  NAMES GEOS
  TARGETS GEOS::geos_c GEOS::GEOS
)
gdal_check_package(HDF4 "Enable HDF4 driver" CAN_DISABLE)

define_find_package2(KEA libkea/KEACommon.h kea)
gdal_check_package(KEA "Enable KEA driver" CAN_DISABLE)

gdal_check_package(ECW "Enable ECW driver" CAN_DISABLE)
gdal_check_package(NetCDF "Enable netCDF driver" CAN_DISABLE
  # NAMES netCDF # Cf. https://github.com/OSGeo/gdal/pull/5453
  TARGETS netCDF::netcdf NETCDF::netCDF)
gdal_check_package(OGDI "Enable ogr_OGDI driver" CAN_DISABLE)
# OpenCL warping gives different results than the ones expected by autotest, so disable it by default even if found.
gdal_check_package(OpenCL "Enable OpenCL (may be used for warping)" DISABLED_BY_DEFAULT)

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

define_find_package2(JXL jxl/decode.h jxl PKGCONFIG_NAME libjxl)
gdal_check_package(JXL "JPEG-XL compression (when used with internal libtiff)" CAN_DISABLE)

# unused for now gdal_check_package(OpenMP "")
gdal_check_package(Crnlib "enable gdal_DDS driver" CAN_DISABLE)
gdal_check_package(IDB "enable ogr_IDB driver" CAN_DISABLE)
# TODO: implement FindRASDAMAN libs: -lrasodmg -lclientcomm -lcompression -lnetwork -lraslib
gdal_check_package(RASDAMAN "enable rasdaman driver" CAN_DISABLE)
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
if (GDAL_USE_OPENJPEG)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(OpenJPEG MODULE)\n")
endif ()

# FIXME: we should probably ultimately move the GRASS driver to an
# external repository, due to GRASS depending on GDAL, hence the GRASS driver
# can only be built as a plugin. Or at the very least we should only allow building
# it as a plugin, and have a GDAL_USE_GRASS variable to control if libgrass should
# be used (and change frmts/CMakeLists.txt and ogr/ogrsf_frmts/CMakeLists.txt
# to use it instead of HAVE_GRASS)
if( ALLOW_GRASS_DRIVER )
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
endif ()

gdal_check_package(HDFS "Enable Hadoop File System through native library" CAN_DISABLE)

# PDF library: one of them enables PDF driver
gdal_check_package(Poppler "Enable PDF driver with Poppler (read side)" CAN_DISABLE)

define_find_package2(PDFIUM public/fpdfview.h pdfium FIND_PATH_SUFFIX pdfium)
gdal_check_package(PDFIUM "Enable PDF driver with Pdfium (read side)" CAN_DISABLE)

gdal_check_package(Podofo "Enable PDF driver with Podofo (read side)" CAN_DISABLE)
if (GDAL_USE_POPPLER
    OR GDAL_USE_PDFIUM
    OR GDAL_USE_PODOFO)
  set(HAVE_PDFLIB ON)
else ()
  set(HAVE_PDFLIB OFF)
endif ()

set(Oracle_CAN_USE_CLNTSH_AS_MAIN_LIBRARY ON)
gdal_check_package(Oracle "Enable Oracle OCI driver" CAN_DISABLE)
gdal_check_package(TEIGHA "Enable DWG and DGNv8 drivers" CAN_DISABLE)
gdal_check_package(FileGDB "Enable FileGDB (based on closed-source SDK) driver" CAN_DISABLE)

option(GDAL_USE_PUBLICDECOMPWT
       "Set ON to build MSG driver and download external https://gitlab.eumetsat.int/open-source/PublicDecompWT" OFF)

# proprietary libraries KAKADU
gdal_check_package(KDU "Enable KAKADU" CAN_DISABLE)
gdal_check_package(LURATECH "Enable JP2Lura driver" CAN_DISABLE)

gdal_check_package(Arrow "Apache Arrow C++ library" CONFIG CAN_DISABLE)
if (Arrow_FOUND)
    gdal_check_package(Parquet "Apache Parquet C++ library" CONFIG PATHS ${Arrow_DIR} CAN_DISABLE)
endif()

# bindings
gdal_check_package(SWIG "Enable language bindings" ALWAYS_ON_WHEN_FOUND)
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
