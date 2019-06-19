#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR TIGER driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
from osgeo import ogr


import gdaltest
import ogrtest
import pytest

###############################################################################


def test_ogr_tiger_1():

    ogrtest.tiger_ds = None

    if not gdaltest.download_file('http://www2.census.gov/geo/tiger/tiger2006se/AL/TGR01001.ZIP', 'TGR01001.ZIP'):
        pytest.skip()

    try:
        os.stat('tmp/cache/TGR01001/TGR01001.MET')
    except OSError:
        try:
            try:
                os.stat('tmp/cache/TGR01001')
            except OSError:
                os.mkdir('tmp/cache/TGR01001')
            gdaltest.unzip('tmp/cache/TGR01001', 'tmp/cache/TGR01001.ZIP')
            try:
                os.stat('tmp/cache/TGR01001/TGR01001.MET')
            except OSError:
                pytest.skip()
        except:
            pytest.skip()

    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001')
    assert ogrtest.tiger_ds is not None

    ogrtest.tiger_ds = None
    # also test opening with a filename (#4443)
    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001/TGR01001.RT1')
    assert ogrtest.tiger_ds is not None

    # Check a few features.
    cc_layer = ogrtest.tiger_ds.GetLayerByName('CompleteChain')
    assert cc_layer.GetFeatureCount() == 19289, 'wrong cc feature count'

    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    assert feat.TLID == 2833200 and feat.FRIADDL is None and feat.BLOCKL == 5000, \
        'wrong attribute on cc feature.'

    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)', max_error=0.000001) == 0

    feat = ogrtest.tiger_ds.GetLayerByName('TLIDRange').GetNextFeature()
    assert feat.MODULE == 'TGR01001' and feat.TLMINID == 2822718, \
        'got wrong TLIDRange attributes'

###############################################################################
# Run test_ogrsf


def test_ogr_tiger_2():

    if ogrtest.tiger_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/cache/TGR01001')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test TIGER writing


def test_ogr_tiger_3():

    if ogrtest.tiger_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/outtiger')
    except OSError:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f TIGER tmp/outtiger tmp/cache/TGR01001 -dsco VERSION=1006')

    ret = 'success'

    filelist = os.listdir('tmp/cache/TGR01001')
    exceptions = ['TGR01001.RTA', 'TGR01001.RTC', 'TGR01001.MET', 'TGR01001.RTZ', 'TGR01001.RTS']
    for filename in filelist:
        if filename in exceptions:
            continue
        f = open('tmp/cache/TGR01001/' + filename, 'rb')
        data1 = f.read()
        f.close()
        try:
            f = open('tmp/outtiger/' + filename, 'rb')
            data2 = f.read()
            f.close()
            if data1 != data2:
                # gdaltest.post_reason('%s is different' % filename)
                print('%s is different' % filename)
                ret = 'fail'
        except:
            # gdaltest.post_reason('could not find %s' % filename)
            print('could not find %s' % filename)
            ret = 'fail'

    try:
        shutil.rmtree('tmp/outtiger')
    except OSError:
        pass

    return ret

###############################################################################
# Load into a /vsimem instance to test virtualization.


def test_ogr_tiger_4():

    if ogrtest.tiger_ds is None:
        pytest.skip()

    # load all the files into memory.
    for filename in gdal.ReadDir('tmp/cache/TGR01001'):

        if filename.startswith('.'):
            continue

        data = open('tmp/cache/TGR01001/' + filename, 'r').read()

        f = gdal.VSIFOpenL('/vsimem/tigertest/' + filename, 'wb')
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

    # Try reading.
    ogrtest.tiger_ds = ogr.Open('/vsimem/tigertest/TGR01001.RT1')
    assert ogrtest.tiger_ds is not None, 'fail to open.'

    ogrtest.tiger_ds = None
    # also test opening with a filename (#4443)
    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001/TGR01001.RT1')
    assert ogrtest.tiger_ds is not None

    # Check a few features.
    cc_layer = ogrtest.tiger_ds.GetLayerByName('CompleteChain')
    assert cc_layer.GetFeatureCount() == 19289, 'wrong cc feature count'

    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    assert feat.TLID == 2833200 and feat.FRIADDL is None and feat.BLOCKL == 5000, \
        'wrong attribute on cc feature.'

    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)', max_error=0.000001) == 0

    feat = ogrtest.tiger_ds.GetLayerByName('TLIDRange').GetNextFeature()
    assert feat.MODULE == 'TGR01001' and feat.TLMINID == 2822718, \
        'got wrong TLIDRange attributes'

    # Try to recover memory from /vsimem.
    for filename in gdal.ReadDir('tmp/cache/TGR01001'):

        if filename.startswith('.'):
            continue

        gdal.Unlink('/vsimem/tigertest/' + filename)

    
###############################################################################


def test_ogr_tiger_cleanup():

    if ogrtest.tiger_ds is None:
        pytest.skip()

    ogrtest.tiger_ds = None



