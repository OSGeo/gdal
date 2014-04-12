#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GME driver testing.
# Author:   Wolf Bergenheim <wolf+grass@bergenheim.net>
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
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Test if driver is available

def ogr_gme_init():

    ogrtest.gme_drv = None

    try:
        ogrtest.gme_refresh = os.environ['GME_REFRESH_TOKEN']
        ogrtest.gme_drv = ogr.GetDriverByName('GME')
    except:
        return 'skip'

    if gdaltest.gdalurlopen('http://www.google.com') is None:
        print('cannot open http://www.google.com')
        ogrtest.gme_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Read test on WORLD94

def ogr_gme_read():
    if ogrtest.gme_drv is None:
        return 'skip'

    table_id = '08857392491625866347-07947734823975336729'

    old_auth = gdal.GetConfigOption('GME_AUTH', None)
    old_access = gdal.GetConfigOption('GME_ACCESS_TOKEN', None)
    old_refresh = gdal.GetConfigOption('GME_REFRESH_TOKEN', None)
    gdal.SetConfigOption('GME_AUTH', None)
    gdal.SetConfigOption('GME_ACCESS_TOKEN', None)
    gdal.SetConfigOption('GME_REFRESH_TOKEN', None)
    ds = ogr.Open('GME:tables='+table_id)
    gdal.SetConfigOption('GME_AUTH', old_auth)
    gdal.SetConfigOption('GME_ACCESS_TOKEN', old_access)
    gdal.SetConfigOption('GME_REFRESH_TOKEN', old_refresh)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    lyr.SetSpatialFilterRect(14, 62.6, 30, 64.0)
    lyr.SetIgnoredFields( ['YR94_', 'YR94_ID'] )
    lyr.SetAttributeFilter("ABBREVNAME='Finland'")

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason('did not get expected feature count')
        print(count)
        return 'fail'
    feature = lyr.GetNextFeature()
    if feature.FIPS_CODE != 'FI':
        gdaltest.post_reason('got wrong feature')
        print(feature.FIPS_CODE)
        return 'fail'
    if feature.gx_id != '163':
        gdaltest.post_reason('unexpected feature id %s (%d)' % (feature.gx_id.feature.GetFID()))
        return 'fail'
    if feature.YR94_ID is not None:
        gdaltest.post_reason('ignored field is not ignored')
        return 'fail'
 
    return 'success'


gdaltest_list = [ 
    ogr_gme_init,
    ogr_gme_read,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gme' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
