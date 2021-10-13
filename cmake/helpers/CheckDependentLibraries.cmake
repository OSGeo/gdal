# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file gdal/LICENSE.TXT.

#[=======================================================================[.rst:
CheckDependentLibraries.cmake
-----------------------------

Detect GDAL depenencies and set variable HAVE_*

#]=======================================================================]

include(CheckFunctionExists)
include(CMakeDependentOption)
include(FeatureSummary)
include(DefineFindPackage2)

# macro
macro(gdal_check_package name purpose)
    string(TOUPPER ${name} key)
    find_package2(${name} QUIET)
    if(NOT DEFINED ${key}_FOUND)
        find_package(${name})
    endif()
    if(${key}_FOUND OR ${name}_FOUND)
        set(HAVE_${key} ON)
    else()
        set(HAVE_${key} OFF)
    endif()
    if(purpose STREQUAL "")
    else()
        set_package_properties(${name} PROPERTIES PURPOSE ${purpose})
    endif()
endmacro()

function(split_libpath _lib)
if(_lib)
   # split lib_line into -L and -l linker options
   get_filename_component(_path ${${_lib}} PATH)
   get_filename_component(_name ${${_lib}} NAME_WE)
   string(REGEX REPLACE "^lib" "" _name ${_name})
   set(${_lib} -L${_path} -l${_name})
endif()
endfunction()

# Custom find_package definitions
define_find_package2(LIBCSF csf.h csf)
define_find_package2(BPG libbpg.h bpg)
define_find_package2(Crnlib crnlib.h crunch)
define_find_package2(IDB it.h idb)
define_find_package2(RASDAMAN rasdaman.hh raslib)
define_find_package2(Epsilon epsilon.h epsilon)
define_find_package2(FME fmeobjects/cpp/issesion.h fme)

find_package(ODBC COMPONENTS ODBCINST)
set_package_properties(ODBC PROPERTIES PURPOSE "Enable DB support thru ODBC")
option(GDAL_USE_XMLREFORMAT "Set ON to use xmlreformat" OFF)

gdal_check_package(MySQL "MySQL")

# basic libaries
find_package(Boost)
gdal_check_package(CURL "Enable drivers to use web API")
cmake_dependent_option(GDAL_USE_CURL "Set ON to use libcurl" ON "CURL_FOUND" OFF)
if(GDAL_USE_CURL)
    if(NOT CURL_FOUND)
        message(FATAL_ERROR "Configured to use libcurl, but not found")
    endif()
endif()

gdal_check_package(Iconv "Used in GDAL portability library")
gdal_check_package(LibXml2 "Read and write XML formats")

gdal_check_package(EXPAT "Read and write XML formats")
gdal_check_package(XercesC "Read and write XML formats")
if(HAVE_EXPAT OR HAVE_XERCESC)
    set(HAVE_XMLPARSER ON)
else()
    set(HAVE_XMLPARSER OFF)
endif()

option(GDAL_USE_LIBZ "Set ON to use libz" ON)
gdal_check_package(ZLIB "GDAL core")
if(GDAL_USE_LIBZ)
    if(NOT HAVE_ZLIB)
        set(GDAL_USE_LIBZ_INTERNAL ON CACHE BOOL "")
    endif()
endif()

find_package(OpenSSL COMPONENTS Crypto SSL)
if(OPENSSL_FOUND)
    set(HAVE_OPENSSL ON CACHE INTERNAL "")
endif()
gdal_check_package(CryptoPP "Use crypto++ library for CPL.")
option(CRYPTOPPL_USE_ONLY_CRYPTODLL_ALG "Use Only cryptoDLL alg. only work on dynamic DLL" OFF)

find_package(PROJ 6.0 REQUIRED)

find_package(TIFF 4.0)
if(TIFF_FOUND)
    set(HAVE_TIFF ON)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${TIFF_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${TIFF_LIBRARIES})
    check_function_exists(TIFFScanlineSize64 HAVE_BIGTIFF)
    set(GDAL_USE_LIBTIFF_INTERNAL OFF CACHE BOOL "")
endif()
if(NOT HAVE_TIFF)
    set(GDAL_USE_LIBTIFF_INTERNAL ON CACHE BOOL "")
    set(HAVE_BIGTIFF ON)
endif()
set_package_properties(TIFF PROPERTIES
                       URL "http://libtiff.org/"
                       DESCRIPTION "support for the Tag Image File Format (TIFF)."
                       TYPE RECOMMENDED
                      )
if(HAVE_BIGTIFF)
    add_definitions(-DBIGTIFF_SUPPORT)
endif()
gdal_check_package(ZSTD "ZSTD compression library")
gdal_check_package(SFCGAL "gdal core supports ISO 19107:2013 and OGC Simple Features Access 1.2 for 3D operations")
cmake_dependent_option(GDAL_USE_SFCGAL "Set ON to use SFCGAL" ON "SFCGAL_FOUND" OFF)

gdal_check_package(GeoTIFF "")
if(NOT HAVE_GEOTIFF)
    set(GDAL_USE_LIBGEOTIFF_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_LIBGEOTIFF_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(PNG "")
if(NOT HAVE_PNG)
    set(GDAL_USE_LIBPNG_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_LIBPNG_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(JPEG "")
if(NOT HAVE_JPEG)
    set(GDAL_USE_LIBJPEG_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_LIBJPEG_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(GIF "")
if(NOT HAVE_GIF)
    set(GDAL_USE_GIFLIB_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_GIFLIB_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(JSONC "")
if(NOT HAVE_JSONC)
    set(GDAL_USE_LIBJSONC_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_LIBJSONC_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(OpenCAD "Enable OpenCAD driver")
if(NOT HAVE_OPENCAD)
    set(GDAL_USE_OPENCAD_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_OPENCAD_INTERNAL OFF CACHE BOOL "")
endif()
gdal_check_package(QHULL "Enable QHULL")
if(NOT HAVE_QHULL)
    set(GDAL_USE_QHULL_INTERNAL ON CACHE BOOL "")
else()
    set(GDAL_USE_QHULL_INTERNAL OFF CACHE BOOL "")
endif()

option(GDAL_USE_LIBPCIDSK_INTERNAL "Set ON to build PCIDSK sdk" ON)
gdal_check_package(LIBCSF "libcsf")
if(NOT HAVE_LIBCSF)
    set(GDAL_USE_LIBCSF_INTERNAL ON CACHE BOOL "Set ON to build pcraster driver with internal libcdf")
else()
    set(GDAL_USE_LIBCSF_INTERNAL OFF CACHE BOOL "Set ON to build pcraster driver with internal libcdf")
endif()

option(GDAL_USE_LIBLERC_INTERNAL "Set ON to build mrf driver with internal libLERC" ON)

gdal_check_package(Shapelib "Enable Shapelib support.")
if(NOT HAVE_SHAPELIB)
    set(GDAL_USE_SHAPELIB_INTERNAL ON CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
else()
    if(Shapelib_VERSION VERSION_LESS 1.4.0)
        message(STATUS "Detected Shapelib version ${Shapelib_VERSION} is too lower to support. Enables internal shapelib.")
        set(GDAL_USE_SHAPELIB_INTERNAL ON CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
    else()
        set(GDAL_USE_SHAPELIB_INTERNAL OFF CACHE BOOL "Set ON to build shape driver with internal shapelib" FORCE)
    endif()
endif()

# 3rd party libraries
gdal_check_package(PCRE "Enable PCRE support for sqlite3")
find_package(SQLite3)
if(SQLite3_FOUND)
    set(HAVE_SQLITE3 ON)
endif()
set_package_properties(SQLite3 PROPERTIES PURPOSE "Spatialite/rasterlite support")
gdal_check_package(SPATIALITE "Enalbe spatialite support for sqlite3")
find_package(Rasterlite2)
set_package_properties(Rasterlite2 PROPERTIES PURPOSE "Enable rasterlite2 support for sqlite3")
if(RASTERLITE2_FOUND AND NOT RASTERLITE2_VERSION_STRING VERSION_LESS 1.1.0)
    # GDAL requires rasterlite2 1.1.0 and later
    set(HAVE_RASTERLITE2 ON CACHE INTERNAL "HAVE_RASTERLITE2")
else()
    if(RASTERLITE2_FOUND AND RASTERLITE2_VERSION_STRING VERSION_LESS 1.1.0)
        message(STATUS "Rasterlite2 requires version 1.1.0 and later, detected: ${RASTERLITE2_VERSION_STRING}")
        message(STATUS "Turn off rasterlite2 support")
    endif()
    set(HAVE_RASTERLITE2 OFF CACHE INTERNAL "HAVE_RASTERLITE2")
endif()

find_package(LibKML COMPONENTS DOM ENGINE)
gdal_check_package(Jasper "Enable JPEG2000 support")

if(HAVE_JASPER)
    # Detect GeoJP2 UUID hack
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_QUIET "yes")
    set(CMAKE_REQUIRED_LIBRARIES jasper)
    check_c_source_compiles("#ifdef __cplusplus\nextern \"C\"\n#endif\n char jp2_encode_uuid ();int main () {return jp2_encode_uuid ();;return 0;}" HAVE_JASPER_UUID)
    if(HAVE_JASPER_UUID)
        message(STATUS "Jasper GeoJP2 UUID hack detected.")
        if(TARGET JASPER::Jasper)
            set_property(TARGET JASPER::Jasper APPEND PROPERTY
                         INTERFACE_COMPILE_DEFINITIONS "HAVE_JASPER_UUID")
        endif()
    endif()
endif()

find_package(HDF5 COMPONENTS CXX)
set_package_properties(HDF5 PROPERTIES PURPOSE "Enable HDF5 driver")

gdal_check_package(WebP "")
gdal_check_package(FreeXL "Enable XLS driver")
gdal_check_package(GTA "")
gdal_check_package(MRSID "")
gdal_check_package(DAP "Data Access Protocol library for server and client.")
gdal_check_package(Armadillo "")
gdal_check_package(CFITSIO "")
gdal_check_package(Epsilon "")
gdal_check_package(GEOS  "GDAL core dependency")
gdal_check_package(HDF4 "Enable HDF4 driver")
gdal_check_package(KEA "")
gdal_check_package(ECW "Enable ECW driver")
gdal_check_package(NetCDF "Enable netCDF driver")
gdal_check_package(OGDI "Enable ogr_OGDI driver")
gdal_check_package(OpenCL "")
gdal_check_package(PostgreSQL "")
gdal_check_package(SOSI  "enable ogr_SOSI driver")
gdal_check_package(LibLZMA "enable TIFF LZMA compression")
gdal_check_package(ZSTD "enable TIFF ZStandard compression")
gdal_check_package(DB2 "enable ogr_DB2 IBM DB2 driver")
gdal_check_package(CharLS "enable gdal_JPEGLS jpeg loss-less driver")
gdal_check_package(OpenMP "")
gdal_check_package(BPG  "enable gdal_BPG driver")
gdal_check_package(Crnlib "enable gdal_DDS driver")
gdal_check_package(IDB "enable ogr_IDB driver")
# TODO: implement FindRASDAMAN
# libs: -lrasodmg -lclientcomm -lcompression -lnetwork -lraslib
gdal_check_package(RASDAMAN "enable rasdaman driver")
#gdal_check_package(TileDB "enable TileDB driver")
gdal_check_package(OpenEXR "OpenEXR >=2.2")

# OpenJPEG's cmake-CONFIG is broken, so call module explicitly
find_package(OpenJPEG MODULE)


# Only GRASS 7 is currently supported but we keep dual version support in cmake for possible future switch to GRASS 8.
set(TMP_GRASS OFF)
foreach(GRASS_SEARCH_VERSION 7)
    # Cached variables: GRASS7_FOUND, GRASS_PREFIX7, GRASS_INCLUDE_DIR7
    # HAVE_GRASS: TRUE if at least one version of GRASS was found
    set(GRASS_CACHE_VERSION ${GRASS_SEARCH_VERSION})
    if(WITH_GRASS${GRASS_CACHE_VERSION})
        find_package(GRASS ${GRASS_SEARCH_VERSION} MODULE)
        if(${GRASS${GRASS_CACHE_VERSION}_FOUND})
            set(GRASS_PREFIX${GRASS_CACHE_VERSION} ${GRASS_PREFIX${GRASS_SEARCH_VERSION}} CACHE PATH "Path to GRASS ${GRASS_SEARCH_VERSION} base directory")
            set(TMP_GRASS ON)
        endif()
    endif()
endforeach()
if(TMP_GRASS)
    set(HAVE_GRASS ON CACHE INTERNAL "HAVE_GRASS")
else()
    set(HAVE_GRASS OFF CACHE INTERNAL "HAVE_GRASS")
endif()
unset(TMP_GRASS)

# PDF library: one of them enables PDF driver
gdal_check_package(Poppler "Enable PDF driver")
gdal_check_package(PDFium "Enable PDF driver")
gdal_check_package(Podofo "Enable PDF driver")
if(HAVE_POPPLER OR HAVE_PDFIUM OR HAVE_PODOFO)
    set(HAVE_PDFLIB ON)
else()
    set(HAVE_PDFLIB OFF)
endif()

gdal_check_package(Oracle "Enable Oracle OCI driver")
gdal_check_package(TEIGHA "")

# proprietary libraries
# Esri ArcSDE(Spatial Database Engine)
gdal_check_package(SDE "Enable ArcSDE(Spatial Database Engine)")
# KAKADU
gdal_check_package(KDU "Enable KAKADU")
# LURATECH JPEG2000 SDK
set(LURATECH_JP2SDK_DIRECTORY "" CACHE STRING "LURATECH JP2SDK library base directory")
gdal_check_package(FME "FME")

# bindings
gdal_check_package(SWIG "Enable language bindings")
set_package_properties(SWIG PROPERTIES
                       DESCRIPTION "software development tool that connects programs written in C and C++ with a variety of high-level programming languages."
                       URL "http://swig.org/"
                       TYPE RECOMMENDED)

# finding python in top of project because of common for autotest and bindings

find_package(Perl)
set_package_properties(Perl PROPERTIES PURPOSE "SWIG_PERL: Perl binding")
find_Package(JNI)
set_package_properties(JNI PROPERTIES PURPOSE "SWIG_JAVA: Java binding")
find_package(CSharp)
set_package_properties(CSharp PROPERTIES PURPOSE "SWIG_CSharp: CSharp binding")

# vim: ts=4 sw=4 sts=4 et
