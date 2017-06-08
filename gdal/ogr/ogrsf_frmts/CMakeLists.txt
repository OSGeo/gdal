# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012,2013 Bishop
# * Copyright (C) 2017 Hiroshi Miura
# * 
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

cmake_minimum_required(VERSION 2.8.12)

include(MacroOptionalAddDriver)

set(LIB_NAME "ogrsf_frmts")
project("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)

set(LIB_HHEADERS
        ogr_attrind.h
        ogrsf_frmts.h
    )

add_subdirectory(generic)
add_subdirectory(geojson)
add_subdirectory(mitab)
add_subdirectory(mem)
add_definitions(-DTAB_ENABLED)
MACRO_ADD_OGR_DEFAULT_DRIVER(GPX gpx)
add_subdirectory(gpx)
MACRO_ADD_OGR_DEFAULT_DRIVER(GML gml)
add_subdirectory(gml)

find_package(CURL)
if(CURL_FOUND)
    set(GDAL_HAVE_CURL ON CACHE INTERNAL "GDAL_HAVE_CURL")
    MACRO_OPTIONAL_ADD_DRIVER(AMIGOCLOUD amigocloud ON)
    add_subdirectory(amigocloud)
    MACRO_OPTIONAL_ADD_DRIVER(CLOUDANT cloudant ON)
    add_subdirectory(cloudant)
    add_subdirectory(couchdb)
    add_subdirectory(csw)
    MACRO_OPTIONAL_ADD_DRIVER(WFS wfs ON)
    add_subdirectory(wfs)
endif()

MACRO_OPTIONAL_ADD_DRIVER(AERONAVFAA aeronavfaa ON)
add_subdirectory(aeronavfaa)
MACRO_OPTIONAL_ADD_DRIVER(ARCGEN arcgen ON)
add_subdirectory(arcgen)
MACRO_OPTIONAL_ADD_DRIVER(ARCOBJECTS arcobjects ON)
add_subdirectory(arcobjects)
MACRO_OPTIONAL_ADD_DRIVER(AVC avc ON)
add_subdirectory(avc)
MACRO_OPTIONAL_ADD_DRIVER(BNA bna ON)
add_subdirectory(bna)
MACRO_OPTIONAL_ADD_DRIVER(CAD cad ON)
add_subdirectory(cad)
MACRO_OPTIONAL_ADD_DRIVER(CARTO carto ON)
add_subdirectory(carto)
MACRO_OPTIONAL_ADD_DRIVER(CSV csv ON)
add_subdirectory(csv)
MACRO_OPTIONAL_ADD_DRIVER(DB2 db2 ON)
add_subdirectory(db2)
MACRO_OPTIONAL_ADD_DRIVER(DXF dxf ON)
add_subdirectory(dxf)
MACRO_OPTIONAL_ADD_DRIVER(GMLAS gmlas ON)
add_subdirectory(gmlas)
MACRO_OPTIONAL_ADD_DRIVER(KML kml ON)
add_subdirectory(kml)
MACRO_OPTIONAL_ADD_DRIVER(LIBKML LibKML OFF)
add_subdirectory(libkml)
MACRO_OPTIONAL_ADD_DRIVER(MONGODB mongodb OFF)
add_subdirectory(mongodb)
MACRO_OPTIONAL_ADD_DRIVER(MYSQL MySQL ON)
add_subdirectory(mysql)
MACRO_OPTIONAL_ADD_DRIVER(NAS nas ON)
add_subdirectory(nas)
MACRO_OPTIONAL_ADD_DRIVER(PG PostGIS ON)
add_subdirectory(pg)
MACRO_OPTIONAL_ADD_DRIVER(S57 s57 ON)
add_subdirectory(s57)
MACRO_OPTIONAL_ADD_DRIVER(SHP "ESRI shape-file" ON)
add_subdirectory(shape)
MACRO_OPTIONAL_ADD_DRIVER(SQLITE SpatialLite ON)
add_subdirectory(sqlite)
MACRO_OPTIONAL_ADD_DRIVER(SXF sxf ON)
add_subdirectory(sxf)
MACRO_OPTIONAL_ADD_DRIVER(VRT vrt ON)
add_subdirectory(vrt)
MACRO_OPTIONAL_ADD_DRIVER(XLSX "Microsoft Office Excel(xlsx)"  ON)
add_subdirectory(xlsx)
MACRO_OPTIONAL_ADD_DRIVER(XPLANE xplane ON)
add_subdirectory(xplane)
