#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGRAPISPY
# Author:   Even Rouault <even.rouault at spatialys.com>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import test_py_scripts
from osgeo import ogr
from osgeo import gdal

###############################################################################
# Basic test without snapshoting

def ograpispy_1():

    gdal.SetConfigOption('OGR_API_SPY_FILE', 'tmp/ograpispy_1.py')
    test_py_scripts.run_py_script('data', 'testograpispy', '')
    gdal.SetConfigOption('OGR_API_SPY_FILE', None)

    try:
        os.stat('tmp/ograpispy_1.py')
        ogrtest.has_apispy = True
    except:
        ogrtest.has_apispy = False
        return 'skip'

    ref_data = open('data/testograpispy.py', 'rt').read()
    got_data = open('tmp/ograpispy_1.py', 'rt').read()

    if ref_data != got_data:
        gdaltest.post_reason('did not get expected script')
        print(got_data)
        return 'fail'

    return 'success'

###############################################################################
# With snapshoting

def ograpispy_2():

    if not ogrtest.has_apispy:
        return 'skip'

    try:
        shutil.rmtree('tmp/snapshot_1')
    except:
        pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ograpispy_2.shp')
    lyr = ds.CreateLayer('ograpispy_2')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ds = None

    gdal.SetConfigOption('OGR_API_SPY_FILE', 'tmp/ograpispy_2.py')
    gdal.SetConfigOption('OGR_API_SPY_SNAPSHOT_PATH', 'tmp')
    ds = ogr.Open('tmp/ograpispy_2.shp', update = 1)
    lyr = ds.GetLayer(0)
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None
    gdal.SetConfigOption('OGR_API_SPY_FILE', None)
    gdal.SetConfigOption('OGR_API_SPY_SNAPSHOT_PATH', None)

    ds = ogr.Open('tmp/snapshot_1/source/ograpispy_2.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('tmp/snapshot_1/working/ograpispy_2.shp', update = 1)
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Add a feature to check that running the script will work
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None

    # Check script
    test_py_scripts.run_py_script('tmp', 'ograpispy_2', '')

    ds = ogr.Open('tmp/snapshot_1/working/ograpispy_2.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 1:
        print(lyr.GetFeatureCount())
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    shutil.rmtree('tmp/snapshot_1/working')

    return 'success'

###############################################################################
#

def ograpispy_cleanup():
    try:
        os.unlink('tmp/ograpispy_1.py')
    except:
        pass
    try:
        os.unlink('tmp/ograpispy_2.py')
    except:
        pass
    try:
        os.unlink('tmp/ograpispy_2.pyc')
    except:
        pass
    try:
        shutil.rmtree('tmp/snapshot_1')
    except:
        pass
    try:
        os.stat('tmp/ograpispy_2.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ograpispy_2.shp')
    except:
        pass

    return 'success'

gdaltest_list = [ 
    ograpispy_1,
    ograpispy_2,
    ograpispy_cleanup
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ograpispy' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
