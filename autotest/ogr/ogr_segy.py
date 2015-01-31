#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SEG-Y driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

###############################################################################
# Read SEG-Y

def ogr_segy_1():

    ds = ogr.Open('data/testsegy.segy')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    if lyr.GetSpatialRef() != None:
        gdaltest.post_reason('bad spatial ref')
        return 'fail'

    if lyr.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldCount() != 71:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT (500000 4500000)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(1)
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    if lyr.GetSpatialRef() != None:
        gdaltest.post_reason('bad spatial ref')
        return 'fail'

    if lyr.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldCount() != 32:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_segy_1,
]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_segy' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

