#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OpenAir driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
# Basic test

def ogr_openair_1():

    ds = ogr.Open('data/openair_test.txt')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    lyr = ds.GetLayerByName('airspaces')
    if lyr is None:
        gdaltest.post_reason('cannot find layer airspaces')
        return 'fail'

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POLYGON ((49.75 2.75,49.75 3.0,49.5 3.0,49.5 2.75,49.75 2.75))',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected first geom')
        print(geom.ExportToWkt())
        return 'fail'
    style = feat.GetStyleString()
    if style != 'PEN(c:#0000FF,w:2pt,p:"5px 5px");BRUSH(fc:#00FF00)':
        print('did not get expected style')
        print(style)
        return 'fail'

    lyr = ds.GetLayerByName('labels')
    if lyr is None:
        gdaltest.post_reason('cannot find layer labels')
        return 'fail'
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POINT (49.2625 2.504166666666667)',
                                      max_error = 0.0000001 ) != 0:
        print('did not get expected geom on labels layer')
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_openair_1 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_openair' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

