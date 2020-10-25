#!/usr/bin/env pytest
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
import shutil
from difflib import unified_diff


import ogrtest
import test_py_scripts
from osgeo import ogr
from osgeo import gdal
import pytest

###############################################################################
# Basic test without snapshoting


def test_ograpispy_1():

    os.environ['OGR_API_SPY_FILE'] = 'tmp/ograpispy_1.py'
    test_py_scripts.run_py_script('data', 'testograpispy', '')
    del os.environ['OGR_API_SPY_FILE']

    try:
        os.stat('tmp/ograpispy_1.py')
        ogrtest.has_apispy = True
    except OSError:
        ogrtest.has_apispy = False
        pytest.skip()

    ref_data = open('data/testograpispy.py', 'rt').read()
    got_data = open('tmp/ograpispy_1.py', 'rt').read()

    if ref_data != got_data:
        print()
        for line in unified_diff(ref_data.splitlines(), got_data.splitlines(),
                                 fromfile='expected', tofile='got',
                                 lineterm=""):
            print(line)
        pytest.fail('did not get expected script')

    
###############################################################################
# With snapshoting


def test_ograpispy_2():

    if not ogrtest.has_apispy:
        pytest.skip()

    try:
        shutil.rmtree('tmp/snapshot_1')
    except OSError:
        pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ograpispy_2.shp')
    lyr = ds.CreateLayer('ograpispy_2')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ds = None

    gdal.SetConfigOption('OGR_API_SPY_FILE', 'tmp/ograpispy_2.py')
    gdal.SetConfigOption('OGR_API_SPY_SNAPSHOT_PATH', 'tmp')
    ds = ogr.Open('tmp/ograpispy_2.shp', update=1)
    lyr = ds.GetLayer(0)
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None
    gdal.SetConfigOption('OGR_API_SPY_FILE', None)
    gdal.SetConfigOption('OGR_API_SPY_SNAPSHOT_PATH', None)

    ds = ogr.Open('tmp/snapshot_1/source/ograpispy_2.shp')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    ds = None

    ds = ogr.Open('tmp/snapshot_1/working/ograpispy_2.shp', update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    # Add a feature to check that running the script will work
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None

    # Check script
    test_py_scripts.run_py_script('tmp', 'ograpispy_2', '')

    ds = ogr.Open('tmp/snapshot_1/working/ograpispy_2.shp')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    shutil.rmtree('tmp/snapshot_1/working')

###############################################################################
#


def test_ograpispy_cleanup():
    gdal.Unlink('tmp/ograpispy_1.py')
    gdal.Unlink('tmp/ograpispy_2.py')
    gdal.Unlink('tmp/ograpispy_2.pyc')
    try:
        shutil.rmtree('tmp/snapshot_1')
    except OSError:
        pass
    try:
        os.stat('tmp/ograpispy_2.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ograpispy_2.shp')
    except (OSError, AttributeError):
        pass
    gdal.Unlink('/vsimem/test2.csv')



