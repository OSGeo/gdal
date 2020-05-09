#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SEG-P1 / UKOOA P1/90 driver.
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
# Read SEGP1


def test_ogr_segp1_points():

    ds = ogr.Open('data/segp1/test.segp1')
    assert ds is not None, 'cannot open dataset'

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'

    feat = lyr.GetNextFeature()

    expected_values = [
        ('LINENAME', 'firstline'),
        ('POINTNUMBER', 10),
        ('RESHOOTCODE', ' '),
        ('LONGITUDE', 2),
        ('LATITUDE', 49),
        ('EASTING', 426857),
        ('NORTHING', 5427937),
        ('DEPTH', 1234)
    ]

    for values in expected_values:
        if feat.GetField(values[0]) != values[1]:
            feat.DumpReadable()
            pytest.fail('did not get expected value for %s' % values[0])

    if ogrtest.check_feature_geometry(feat, 'POINT (2 49)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    
###############################################################################
# Read SEGP1 lines


def test_ogr_segp1_lines():

    ds = ogr.Open('data/segp1/test.segp1')
    assert ds is not None, 'cannot open dataset'

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(1)
    assert lyr.GetGeomType() == ogr.wkbLineString, 'bad layer geometry type'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'firstline':
        feat.DumpReadable()
        pytest.fail('did not get expected value for LINENAME')

    if ogrtest.check_feature_geometry(feat, 'LINESTRING (2 49,2.0 49.5)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'secondline':
        feat.DumpReadable()
        pytest.fail('did not get expected value for LINENAME')

    if ogrtest.check_feature_geometry(feat, 'LINESTRING (-2 -49,-2.5 -49.0)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    
###############################################################################
# Read UKOOA


def test_ogr_ukooa_points():

    ds = ogr.Open('data/segukooa/test.ukooa')
    assert ds is not None, 'cannot open dataset'

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'

    feat = lyr.GetNextFeature()

    expected_values = [
        ('LINENAME', 'firstline'),
        ('POINTNUMBER', 10),
        ('LONGITUDE', 2),
        ('LATITUDE', 49),
        ('EASTING', 426857),
        ('NORTHING', 5427937),
        ('DEPTH', 1234)
    ]

    for values in expected_values:
        if feat.GetField(values[0]) != values[1]:
            feat.DumpReadable()
            pytest.fail('did not get expected value for %s' % values[0])

    if ogrtest.check_feature_geometry(feat, 'POINT (2 49)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    
###############################################################################
# Read UKOOA lines


def test_ogr_ukooa_lines():

    ds = ogr.Open('data/segukooa/test.ukooa')
    assert ds is not None, 'cannot open dataset'

    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(1)
    assert lyr.GetGeomType() == ogr.wkbLineString, 'bad layer geometry type'

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'firstline':
        feat.DumpReadable()
        pytest.fail('did not get expected value for LINENAME')

    if ogrtest.check_feature_geometry(feat, 'LINESTRING (2 49,2.0 49.5)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    feat = lyr.GetNextFeature()

    if feat.GetField('LINENAME') != 'secondline':
        feat.DumpReadable()
        pytest.fail('did not get expected value for LINENAME')

    if ogrtest.check_feature_geometry(feat, 'LINESTRING (-2 -49,-2.5 -49.0)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected first geom')

    



