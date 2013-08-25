#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDS driver.
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

def ogr_pds_1():

    ds = ogr.Open('data/ap01578l.lbl')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    lyr = ds.GetLayerByName('RAMAPPING')
    if lyr is None:
        gdaltest.post_reason('cannot find layer')
        return 'fail'

    if lyr.GetFeatureCount() != 74786:
        gdaltest.post_reason('did not get expected feature count')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('NOISE_COUNTS_1') != 96:
        feat.DumpReadable()
        return 'fail'
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POINT (146.1325 -55.648)',
                                      max_error = 0.000000001 ) != 0:
        print('did not get expected geom')
        print(geom.ExportToWkt())
        return 'fail'

    feat = lyr.GetFeature(1)
    if feat.GetField('MARS_RADIUS') != 3385310.2:
        feat.DumpReadable()
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_pds_1 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pds' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

