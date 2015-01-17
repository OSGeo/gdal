#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Cloudant driver testing.
# Author:   Norman Barker, norman at cloudant com
#          Based on the CouchDB driver
# 
###############################################################################
#  Copyright (c) 2014, Norman Barker <norman at cloudant com>

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
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Test if driver is available

def ogr_cloudant_init():

    ogrtest.cloudant_drv = None

    try:
        ogrtest.cloudant_drv = ogr.GetDriverByName('Cloudant')
    except:
        pass

    if ogrtest.cloudant_drv is None:
        return 'skip'

    if 'CLOUDANT_TEST_SERVER' in os.environ:
        ogrtest.cloudant_test_server = os.environ['CLOUDANT_TEST_SERVER']
        ogrtest.cloudant_test_url = ogrtest.cloudant_test_server
    else:
        ogrtest.cloudant_test_server = 'https://yescandrephereddescamill:I1rhuWEIVDRvbpoQNOBW3pGV@normanb.cloudant.com'
        ogrtest.cloudant_test_url = 'https://normanb.cloudant.com'
    
    ogrtest.cloudant_test_layer = 'gdaltest'

    if gdaltest.gdalurlopen(ogrtest.cloudant_test_url) is None:
        print('cannot open %s' % ogrtest.cloudant_test_url)
        ogrtest.cloudant_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test GetFeatureCount()

def ogr_cloudant_GetFeatureCount():
    if ogrtest.cloudant_drv is None:
        return 'skip'

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    count = lyr.GetFeatureCount()
    if count != 52:
        gdaltest.post_reason('did not get expected feature count')
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# Test GetNextFeature()

def ogr_cloudant_GetNextFeature():
    if ogrtest.cloudant_drv is None:
        return 'skip'

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('_id') != '0400000US01':
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test GetSpatialRef()

def ogr_cloudant_GetSpatialRef():
    if ogrtest.cloudant_drv is None:
        return 'skip'

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    sr = lyr.GetSpatialRef()

    if sr is None:
        return 'success'

    return 'success'

###############################################################################
# Test GetExtent()

def ogr_cloudant_GetExtent():
    if ogrtest.cloudant_drv is None:
        return 'skip'

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    extent = lyr.GetExtent()
    if extent is None:
        gdaltest.post_reason('did not get expected extent')
        return 'fail'

    if extent != (-179.14734, 179.77847, 17.884813, 71.352561):
        gdaltest.post_reason('did not get expected extent')
        print(extent)
        return 'fail'

    return 'success'

###############################################################################
# Test SetSpatialFilter()

def ogr_cloudant_SetSpatialFilter():
    if ogrtest.cloudant_drv is None:
        return 'skip'

    if not ogrtest.have_geos():
        return 'skip'

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    lyr.SetSpatialFilterRect( -104.9847,39.7392,-104.9847,39.7392 )

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('NAME') != 'Colorado':
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

if gdaltest.skip_on_travis():
    gdaltest_list = []
else:
    gdaltest_list = [ 
    ogr_cloudant_init,
    ogr_cloudant_GetFeatureCount,
    ogr_cloudant_GetNextFeature,
    ogr_cloudant_GetSpatialRef,
    ogr_cloudant_GetExtent,
    ogr_cloudant_SetSpatialFilter
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cloudant' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
