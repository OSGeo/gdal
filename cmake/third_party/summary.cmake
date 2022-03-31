################################################################################
# Project:  CMake4GDAL
# Purpose:  CMake build scripts
# Author:   Dmitry Baryshnikov, polimax@mail.ru
################################################################################
# Copyright (C) 2015-2016, NextGIS <info@nextgis.com>
# Copyright (C) 2012,2013,2014 Dmitry Baryshnikov
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
################################################################################

macro(summary_message text value)
    string(ASCII 27 Esc)
    set(BoldYellow  "${Esc}[1;33m")
    set(Magenta     "${Esc}[35m")
    set(Cyan        "${Esc}[36m")
    set(BoldCyan    "${Esc}[1;36m")
    set(White       "${Esc}[37m")
    set(ColourReset "${Esc}[m")

    if("${${value}}")
        message(STATUS "${BoldCyan}  ${text} yes${ColourReset}")
    else()
        message(STATUS "${Cyan}  ${text} no${ColourReset}")
    endif()
endmacro()

message(STATUS "GDAL is now configured for ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Installation directory:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  C compiler:                ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS}")
message(STATUS "  C++ compiler:              ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS}")
# TODO: do we need it?
#message(STATUS "")
#summary_message("  LIBTOOL support:         "  WITH_LIBTOOL)
message(STATUS "")
summary_message("LIBZ support:              " ZLIB_FOUND)
summary_message("LIBLZMA support:           " LIBLZMA_FOUND)
summary_message("cryptopp support:          " WITH_CRYPTOPP)
summary_message("GRASS support:             " WITH_GRASS)
summary_message("CFITSIO support:           " WITH_CFITSIO)
summary_message("PCRaster support:          " WITH_PCRASTER)
summary_message("LIBPNG support:            " PNG_FOUND)
summary_message("DDS support:               " WITH_DDS)
summary_message("GTA support:               " WITH_GTA)
if(HAVE_BIGTIFF OR WITH_TIFF_EXTERNAL)
    summary_message("LIBTIFF support (BigTIFF=yes)" TIFF_FOUND)
else()
    summary_message("LIBTIFF support            " TIFF_FOUND)
endif()
summary_message("LIBGEOTIFF support:        " GEOTIFF_FOUND)
summary_message("LIBJPEG support:           " JPEG_FOUND)
summary_message("12 bit JPEG:               " JPEG12_FOUND)
summary_message("12 bit JPEG-in-TIFF:       " JPEG12_FOUND AND WITH_TIFF_EXTERAL)
summary_message("LIBGIF support:            " GIF_FOUND)
summary_message("OGDI support:              " WITH_OGDI)
summary_message("HDF4 support:              " HDF4_FOUND)
summary_message("HDF5 support:              " HDF5_FOUND)
summary_message("OpenJPEG support:          " OPENJPEG_FOUND)
summary_message("Kea support:               " WITH_KEA)
summary_message("NetCDF support:            " WITH_NETCDF)
summary_message("Kakadu support:            " WITH_KAKADU)

if(HAVE_JASPER_UUID)
summary_message("JasPer support (GeoJP2=${HAVE_JASPER_UUID}): " WITH_JASPER)
else()
summary_message("JasPer support:            " WITH_JASPER)
endif()

summary_message("ECW support:               " WITH_ECW)
summary_message("MrSID support:             " WITH_MRSID)
summary_message("MrSID/MG4 Lidar support:   " WITH_MRSID_LIDAR)
summary_message("MSG support:               " WITH_MSG)
summary_message("GRIB support:              " WITH_GRIB)
summary_message("EPSILON support:           " WITH_EPSILON)
summary_message("WebP support:              " WITH_WEBP)
summary_message("cURL support (wms/wcs/...):" CURL_FOUND)
summary_message("PostgreSQL support:        " POSTGRESQL_FOUND)
summary_message("MRF support:               " ENABLE_MRF)
summary_message("MySQL support:             " WITH_MYSQL)
summary_message("Ingres support:            " WITH_INGRES)
summary_message("Xerces-C support:          " WITH_XERCES)
summary_message("NAS support:               " WITH_NAS)
summary_message("Expat support:             " EXPAT_FOUND)
summary_message("libxml2 support:           " LIBXML2_FOUND)
summary_message("Google libkml support:     " ENABLE_LIBKML)
summary_message("ODBC support:              " WITH_ODBC)
summary_message("PGeo support:              " WITH_PGEO)
summary_message("FGDB support:              " WITH_FGDB)
summary_message("MDB support:               " WITH_MDB)
summary_message("PCIDSK support:            " ENABLE_PCIDSK AND JPEG_FOUND)
summary_message("OCI support:               " WITH_OCI)
summary_message("GEORASTER support:         " WITH_GEORASTER)
summary_message("SDE support:               " WITH_SDE)
summary_message("Rasdaman support:          " WITH_RASDAMAN)
summary_message("DODS support:              " WITH_DODS)
summary_message("SQLite support:            " SQLITE3_FOUND)
summary_message("PCRE support:              " WITH_PCRE)
summary_message("SpatiaLite support:        " SPATIALITE_FOUND)
summary_message("DWGdirect support          " WITH_DWGDIRECT)
summary_message("INFORMIX DataBlade support:" WITH_IDB)
summary_message("GEOS support:              " GEOS_FOUND)
summary_message("QHull support:             " QHULL_FOUND)
summary_message("Poppler support:           " WITH_POPPLER)
summary_message("Podofo support:            " WITH_PODOFO)
summary_message("PDFium support:            " WITH_PDFIUM)
summary_message("OpenCL support:            " OPENCL_FOUND)
summary_message("Armadillo support:         " WITH_Armadillo)
summary_message("FreeXL support:            " WITH_FREEXL)
summary_message("SOSI support:              " WITH_SOSI)
summary_message("MongoDB support:           " WITH_MONGODB)
message(STATUS "")
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
summary_message("Mac OS X Framework :       " MACOSX_FRAMEWORK)
message(STATUS "")
endif()
if(GDAL_BINDINGS)
    message(STATUS "  SWIG Bindings:              ${GDAL_BINDINGS}")
else()
    message(STATUS "  SWIG Bindings:              no")
endif()
message(STATUS "")
summary_message("PROJ support:              " PROJ_FOUND)
summary_message("Json-c support:            " JSONC_FOUND)
summary_message("enable OGR building:       " GDAL_ENABLE_OGR)
summary_message("enable GNM building:       " GDAL_ENABLE_GNM)
message(STATUS "")
summary_message("enable pthread support:    " GDAL_USE_CPL_MULTIPROC_PTHREAD)
summary_message("enable POSIX iconv support:" ICONV_FOUND)
summary_message("hide internal symbols:     " GDAL_HIDE_INTERNAL_SYMBOLS)

if(WITH_PODOFO AND WITH_POPPLER AND WITH_PDFIUM)
    message(WARNING "--with-podofo, --with-poppler and --with-pdfium available.
                     This is unusual setup, but will work. Pdfium will be used
                     by default...")
elseif(WITH_PODOFO AND WITH_POPPLER)
    message(WARNING "--with-podofo and --with-poppler are both available.
                     This is unusual setup, but will work. Poppler will be used
                     by default...")
elseif(WITH_POPPLER AND WITH_PDFIUM)
    message(WARNING "--with-poppler and --with-pdfium are both available. This
                     is unusual setup, but will work. Pdfium will be used by
                     default...")
elseif(WITH_PODOFO AND WITH_PDFIUM)
    message(WARNING "--with-podofo and --with-pdfium are both available. This is
                     unusual setup, but will work. Pdfium will be used by
                     default...")
endif()

if(WITH_LIBXML2 AND WITH_FGDB)
    message(WARNING "-DWITH_LIBXML2 and -DWITH_FGDB are both available.
                     There might be some incompatibility between system libxml2
                     and the embedded copy within libFileGDBAPI")
endif()
