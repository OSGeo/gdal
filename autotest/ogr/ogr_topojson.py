#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  TopJSON driver test suite.
# Author:   Even Rouault
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import ogr

import ogrtest
import pytest

###############################################################################
# Test TopoJSON


def test_ogr_toposjon_objects_is_array():

    ds = ogr.Open('data/topojson/topojson1.topojson')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'a_layer'
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') == 0
    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'TopoJSON'
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'id'
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == 'name'
    expected_results = [
        ('foo', None, 'POINT EMPTY'),
        (None, None, 'POINT EMPTY'),
        (None, None, 'POINT EMPTY'),
        (None, None, 'POINT (100 1010)'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, None, 'LINESTRING EMPTY'),
        (None, '0', 'LINESTRING EMPTY'),
        (None, 'foo', 'LINESTRING EMPTY'),
        ('1', None, 'LINESTRING (100 1000,110 1000,110 1100)'),
        ('2', None, 'LINESTRING (110 1100,110 1000,100 1000)'),
        (None, None, 'POLYGON EMPTY'),
        (None, None, 'POLYGON EMPTY'),
        (None, None, 'POLYGON EMPTY'),
        (None, None, 'POLYGON ((100 1000,110 1000,110 1100,100 1100,100 1000),(101 1010,101 1090,109 1090,109 1010,101 1010))'),
        (None, None, 'POLYGON ((110 1100,110 1000,100 1000,100 1100,110 1100),(101 1010,109 1010,109 1090,101 1090,101 1010))'),
        (None, None, 'MULTIPOINT EMPTY'),
        (None, None, 'MULTIPOINT EMPTY'),
        (None, None, 'MULTIPOINT EMPTY'),
        (None, None, 'MULTIPOINT EMPTY'),
        (None, None, 'MULTIPOINT (100 1010,101 1020)'),
        (None, None, 'MULTIPOLYGON EMPTY'),
        (None, None, 'MULTIPOLYGON EMPTY'),
        (None, None, 'MULTIPOLYGON EMPTY'),
        (None, None, 'MULTIPOLYGON (((110 1100,110 1000,100 1000,100 1100,110 1100)),((101 1010,109 1010,109 1090,101 1090,101 1010)))'),
        (None, None, 'MULTILINESTRING EMPTY'),
        (None, None, 'MULTILINESTRING EMPTY'),
        (None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100))'),
        (None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100,100 1100,100 1000))'),
        (None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100,100 1100,100 1000),(101 1010,101 1090,109 1090,109 1010,101 1010))'),
    ]
    assert lyr.GetFeatureCount() == len(expected_results)
    for i, exp_result in enumerate(expected_results):
        feat = lyr.GetNextFeature()
        if feat.GetField('id') != exp_result[0] or \
           feat.GetField('name') != exp_result[1] or \
           feat.GetGeometryRef().ExportToWkt() != exp_result[2]:
            feat.DumpReadable()
            print(exp_result)
            print(feat.GetField('name'))
            pytest.fail('failure at feat index %d' % i)
    ds = None


def test_ogr_toposjon_objects_is_dict():

    ds = ogr.Open('data/topojson/topojson2.topojson')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'a_layer'
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'id'
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == 'name'
    feat = lyr.GetNextFeature()
    assert feat['id'] == 'foo'
    assert feat['name'] == 'line'
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') == 0
    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'TopoJSON'
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') == 0
    ds = None


def test_ogr_toposjon_no_transform():

    ds = ogr.Open('data/topojson/topojson3.topojson')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'a_layer'
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (0 0,10 0,0 10,10 0,0 0)') == 0
    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'TopoJSON'
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (0 0,10 0,0 10,10 0,0 0)') == 0
    ds = None
