#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR HTF driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
import ogrtest
from osgeo import ogr
import pytest

###############################################################################
# Basic test


def test_ogr_htf_1():

    ds = ogr.Open('data/htf/test.htf')
    assert ds is not None, 'cannot open dataset'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'polygon', 'layer 0 is not polygon'

    lyr = ds.GetLayerByName('polygon')
    assert lyr is not None, 'cannot find layer polygon'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POLYGON ((320830 7678810,350840 7658030,308130 7595560,278310 7616820,320830 7678810))',
                                      max_error=0.0000001) != 0:
        print(geom.ExportToWkt())
        pytest.fail('did not get expected first geom')

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POLYGON ((320830 7678810,350840 7658030,308130 7595560,278310 7616820,320830 7678810),(0 0,0 1,1 1,0 0))',
                                      max_error=0.0000001) != 0:
        print(geom.ExportToWkt())
        pytest.fail('did not get expected first geom')

    assert feat.GetField('IDENTIFIER') == 2, 'did not get expected identifier'

    lyr = ds.GetLayerByName('sounding')
    assert lyr is not None, 'cannot find layer sounding'

    assert lyr.GetFeatureCount() == 2, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POINT (278670 7616330)',
                                      max_error=0.0000001) != 0:
        print(geom.ExportToWkt())
        pytest.fail('did not get expected first geom')

    assert feat.GetField('other3') == 'other3', 'did not get expected other3 val'

###############################################################################
# Run test_ogrsf


def test_ogr_htf_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/htf/test.htf')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/htf/test.htf metadata')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1




