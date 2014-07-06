#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR Idrisi driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import ogr
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Test point layer

def ogr_idrisi_1():

    ds = ogr.Open('data/points.vct')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(1).GetName() != 'IntegerField':
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'

    sr = lyr.GetSpatialRef()
    if sr.ExportToWkt().find('PROJCS["UTM Zone 31, Northern Hemisphere"') != 0:
        gdaltest.post_reason('fail')
        print(sr.ExportToWkt())
        return 'fail'

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetExtent() != (400000.0, 600000.0, 4000000.0, 5000000.0):
        gdaltest.post_reason('fail')
        print(lyr.GetExtent())
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 1.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsInteger(1) != 2:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsDouble(2) != 3.45:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsString(3) != 'foo':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(400000 5000000)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (600000 4000000)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(600000-1,4000000-1,600000+1,4000000+1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(0,0,1,1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test linestring layer

def ogr_idrisi_2():

    ds = ogr.Open('data/lines.vct')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetExtent() != (400000.0, 600000.0, 4000000.0, 5000000.0):
        gdaltest.post_reason('fail')
        print(lyr.GetExtent())
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 10.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (400000 5000000,600000 4500000)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 20.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (450000 4000000,550000 4500000)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(0,0,1,1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test polygon layer

def ogr_idrisi_3():

    ds = ogr.Open('data/polygons.vct')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPolygon:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetExtent() != (400000.0, 600000.0, 4000000.0, 5000000.0):
        gdaltest.post_reason('fail')
        print(lyr.GetExtent())
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 1.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((400000 4000000,400000 5000000,600000 5000000,600000 4000000,400000 4000000),(450000 4250000,450000 4750000,550000 4750000,550000 4250000,450000 4250000))')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((400000 4000000,400000 5000000,600000 5000000,600000 4000000,400000 4000000))')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(0,0,1,1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

gdaltest_list = [
    ogr_idrisi_1,
    ogr_idrisi_2,
    ogr_idrisi_3,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_idrisi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

