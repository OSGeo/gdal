#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OpenAir driver.
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



import ogrtest
from osgeo import ogr
import pytest

###############################################################################
# Basic test


def test_ogr_openair_1():

    ds = ogr.Open('data/openair/openair_test.txt')
    assert ds is not None, 'cannot open dataset'

    lyr = ds.GetLayerByName('airspaces')
    assert lyr is not None, 'cannot find layer airspaces'

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POLYGON ((49.75 2.75,49.75 3.0,49.5 3.0,49.5 2.75,49.75 2.75))',
                                      max_error=0.0000001) != 0:
        print('did not get expected first geom')
        pytest.fail(geom.ExportToWkt())
    style = feat.GetStyleString()
    assert style == 'PEN(c:#0000FF,w:2pt,p:"5px 5px");BRUSH(fc:#00FF00)', \
        'did not get expected style'

    lyr = ds.GetLayerByName('labels')
    assert lyr is not None, 'cannot find layer labels'
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POINT (49.2625 2.504166666666667)',
                                      max_error=0.0000001) != 0:
        print('did not get expected geom on labels layer')
        pytest.fail(geom.ExportToWkt())

    



