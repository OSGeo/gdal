#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CouchDB driver testing.
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
import gdal
import ogr

###############################################################################
# Test if driver is available

def ogr_couchdb_init():

    ogrtest.couchdb_drv = None

    try:
        ogrtest.couchdb_drv = ogr.GetDriverByName('CouchDB')
    except:
        pass

    if ogrtest.couchdb_drv is None:
        return 'skip'

    ogrtest.couchdb_test_db = 'http://gdalautotest.iriscouch.com'

    if gdaltest.gdalurlopen(ogrtest.couchdb_test_db) is None:
        print('cannot open %s' % ogrtest.couchdb_test_db)
        ogrtest.couchdb_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test GetFeatureCount()

def ogr_couchdb_GetFeatureCount():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    count = lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason('did not get expected feature count')
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# Test GetNextFeature()

def ogr_couchdb_GetNextFeature():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test GetFeature()

def ogr_couchdb_GetFeature():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    feat = lyr.GetFeature(0)
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test GetSpatialRef()

def ogr_couchdb_GetSpatialRef():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    sr = lyr.GetSpatialRef()
    if sr is None:
        gdaltest.post_reason('did not get expected srs')
        return 'fail'

    txt = sr.ExportToWkt()
    if txt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected srs')
        print(txt)
        return 'fail'

    return 'success'

###############################################################################
# Test GetExtent()

def ogr_couchdb_GetExtent():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    extent = lyr.GetExtent()
    if extent is None:
        gdaltest.post_reason('did not get expected extent')
        return 'fail'

    if extent != (478315.53125, 481645.3125, 4762880.5, 4765610.5):
        gdaltest.post_reason('did not get expected extent')
        print(extent)
        return 'fail'

    return 'success'

###############################################################################
# Test SetSpatialFilter()

def ogr_couchdb_SetSpatialFilter():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    lyr.SetSpatialFilterRect( 479647, 4764856.5, 480389.6875, 4765610.5 )

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    count = 0
    while feat is not None:
        count = count + 1
        feat = lyr.GetNextFeature()

    if count != 5:
        gdaltest.post_reason('did not get expected feature count (1)')
        print(count)
        return 'fail'

    count = lyr.GetFeatureCount()
    if count != 5:
        gdaltest.post_reason('did not get expected feature count (2)')
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# Test SetAttributeFilter()

def ogr_couchdb_SetAttributeFilter():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_db)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    lyr.SetAttributeFilter( 'EAS_ID = 170' )

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get expected feature')
        return 'fail'
    if feat.GetField('EAS_ID') != 170:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    count = 0
    while feat is not None:
        count = count + 1
        feat = lyr.GetNextFeature()

    if count != 1:
        gdaltest.post_reason('did not get expected feature count (1)')
        print(count)
        return 'fail'

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason('did not get expected feature count (2)')
        print(count)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_couchdb_init,
    ogr_couchdb_GetFeatureCount,
    ogr_couchdb_GetNextFeature,
    ogr_couchdb_GetFeature,
    ogr_couchdb_GetSpatialRef,
    ogr_couchdb_GetExtent,
    ogr_couchdb_SetSpatialFilter,
    ogr_couchdb_SetAttributeFilter
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_couchdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
