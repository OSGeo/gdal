#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SEG-Y driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
# Read SEG-Y


def test_ogr_segy_1():

    ds = ogr.Open('data/segy/testsegy.segy')
    assert ds is not None, 'cannot open dataset'

    assert ds.TestCapability("foo") == 0

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'

    assert lyr.GetSpatialRef() is None, 'bad spatial ref'

    assert lyr.TestCapability("foo") == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 71

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT (500000 4500000)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    feat = lyr.GetNextFeature()
    assert feat is None

    lyr = ds.GetLayer(1)
    assert lyr.GetGeomType() == ogr.wkbNone, 'bad layer geometry type'

    assert lyr.GetSpatialRef() is None, 'bad spatial ref'

    assert lyr.TestCapability("foo") == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 32

    feat = lyr.GetNextFeature()
    assert feat is not None

    feat = lyr.GetNextFeature()
    assert feat is None

###############################################################################
# Read ASCII header SEG-Y


def test_ogr_segy_2():
    ds = ogr.Open('data/segy/ascii-header-with-nuls.sgy')
    assert ds is not None, 'cannot open dataset'

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'




