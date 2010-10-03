#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR HTF driver.
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
import ogr

###############################################################################
# Basic test

def ogr_htf_1():

    ds = ogr.Open('data/test.htf')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'polygon':
        gdaltest.post_reason('layer 0 is not polygon')
        return 'fail'

    lyr = ds.GetLayerByName('polygon')
    if lyr is None:
        gdaltest.post_reason('cannot find layer polygon')
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POLYGON ((320830 7678810,350840 7658030,308130 7595560,278310 7616820,320830 7678810))',
                                      max_error = 0.0000001 ) != 0:
        gdaltest.post_reason('did not get expected first geom')
        print(geom.ExportToWkt())
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POLYGON ((320830 7678810,350840 7658030,308130 7595560,278310 7616820,320830 7678810),(0 0,0 1,1 1,0 0))',
                                      max_error = 0.0000001 ) != 0:
        gdaltest.post_reason('did not get expected first geom')
        print(geom.ExportToWkt())
        return 'fail'

    if feat.GetField('IDENTIFIER') != 2:
        gdaltest.post_reason('did not get expected identifier')
        print(feat.GetField('IDENTIFIER'))
        return 'fail'


    lyr = ds.GetLayerByName('sounding')
    if lyr is None:
        gdaltest.post_reason('cannot find layer sounding')
        return 'fail'

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('did not get expected feature count')
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat,'POINT (278670 7616330)',
                                      max_error = 0.0000001 ) != 0:
        gdaltest.post_reason('did not get expected first geom')
        print(geom.ExportToWkt())
        return 'fail'

    if feat.GetField('other3') != 'other3':
        gdaltest.post_reason('did not get expected other3 val')
        print(feat.GetField('other3'))
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_htf_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.htf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'


    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.htf metadata')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_htf_1,
    ogr_htf_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_htf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

