#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoJSON driver testing.
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

###############################################################################
# Test utilities
def validate_layer(lyr, name, features, type, fields, box):

    if name != lyr.GetName():
        gdaltest.post_reason('Wrong layer name')
        return 'fail'

    if features != lyr.GetFeatureCount():
        gdaltest.post_reason('Wrong number of features')
        return fail

    lyrDefn = lyr.GetLayerDefn()
    if lyrDefn is None:
        gdaltest.post_reason('Layer definition is none')
        return 'fail'

    if type != lyrDefn.GetGeomType():
        gdaltest.post_reason('Wrong geometry type')
        return 'fail'

    if fields != lyrDefn.GetFieldCount():
        gdaltest.post_reason('Wrong number of fields')
        return 'fail'

    extent = lyr.GetExtent()

    minx = abs(extent[0] - box[0])
    maxx = abs(extent[1] - box[1])
    miny = abs(extent[2] - box[2])
    maxy = abs(extent[3] - box[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        gdaltest.post_reason( 'Wrong spatial extent of layer' )
        return 'fail'

###############################################################################
# Find GeoJSON driver

def ogr_geojson_1():

    gdaltest.geojson_drv = ogr.GetDriverByName('GeoJSON')
    
    if gdaltest.geojson_drv is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test file-based DS with standalone "Point" feature object.

def ogr_geojson_2():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/point.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 100.0, 0.0, 0.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPoint, 0, extent)

    return 'success'

###############################################################################
# Test file-based DS with standalone "LineString" feature object.

def ogr_geojson_3():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/linestring.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbLineString, 0, extent)

    return 'success'

##############################################################################
# Test file-based DS with standalone "Polygon" feature object.

def ogr_geojson_4():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/polygon.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPolygon, 0, extent)

    return 'success'

##############################################################################
# Test file-based DS with standalone "GeometryCollection" feature object.

def ogr_geojson_5():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/geometrycollection.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 102.0, 0.0, 1.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbGeometryCollection, 0, extent)

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiPoint" feature object.

def ogr_geojson_6():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multipoint.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 0, extent)

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiLineString" feature object.

def ogr_geojson_7():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multilinestring.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiLineString, 0, extent)

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiPolygon" feature object.

def ogr_geojson_8():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multipolygon.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPolygon, 0, extent)

    return 'success'

###############################################################################

def ogr_geojson_cleanup():

    return 'success'

gdaltest_list = [ 
    ogr_geojson_1,
    ogr_geojson_2,
    ogr_geojson_3,
    ogr_geojson_4,
    ogr_geojson_5,
    ogr_geojson_6,
    ogr_geojson_7,
    ogr_geojson_8,
    ogr_geojson_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geojson' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

