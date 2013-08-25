#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SEG-P1 / UKOOA P1/90 driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
###############################################################################

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

###############################################################################
# Read SEGP1

def ogr_segp1_points():

    ds = ogr.Open('data/test.segp1')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    feat = lyr.GetNextFeature()

    expected_values = [
        ( 'LINENAME', 'firstline' ),
        ( 'POINTNUMBER', 10 ),
        ( 'RESHOOTCODE', ' ' ),
        ( 'LONGITUDE', 2 ),
        ( 'LATITUDE', 49 ),
        ( 'EASTING', 426857 ),
        ( 'NORTHING', 5427937 ),
        ( 'DEPTH', 1234 )
    ]

    for values in expected_values:
        if feat.GetField(values[0]) != values[1]:
            print('did not get expected value for %s' % values[0])
            feat.DumpReadable()
            return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POINT (2 49)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Read SEGP1 lines

def ogr_segp1_lines():

    ds = ogr.Open('data/test.segp1')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(1)
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'firstline':
        print('did not get expected value for LINENAME')
        feat.DumpReadable()
        return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'LINESTRING (2 49,2.0 49.5)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'secondline':
        print('did not get expected value for LINENAME')
        feat.DumpReadable()
        return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'LINESTRING (-2 -49,-2.5 -49.0)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Read UKOOA

def ogr_ukooa_points():

    ds = ogr.Open('data/test.ukooa')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    feat = lyr.GetNextFeature()

    expected_values = [
        ( 'LINENAME', 'firstline' ),
        ( 'POINTNUMBER', 10 ),
        ( 'LONGITUDE', 2 ),
        ( 'LATITUDE', 49 ),
        ( 'EASTING', 426857 ),
        ( 'NORTHING', 5427937 ),
        ( 'DEPTH', 1234 )
    ]

    for values in expected_values:
        if feat.GetField(values[0]) != values[1]:
            print('did not get expected value for %s' % values[0])
            feat.DumpReadable()
            return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POINT (2 49)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Read UKOOA lines

def ogr_ukooa_lines():

    ds = ogr.Open('data/test.ukooa')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(1)
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'firstline':
        print('did not get expected value for LINENAME')
        feat.DumpReadable()
        return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'LINESTRING (2 49,2.0 49.5)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'secondline':
        print('did not get expected value for LINENAME')
        feat.DumpReadable()
        return 'fail'

    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'LINESTRING (-2 -49,-2.5 -49.0)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_segp1_points,
    ogr_segp1_lines,
    ogr_ukooa_points,
    ogr_ukooa_lines,
]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_segukooa' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

