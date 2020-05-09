#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR Idrisi driver.
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

from osgeo import ogr


import ogrtest
import pytest

###############################################################################
# Test point layer


def test_ogr_idrisi_1():

    ds = ogr.Open('data/idrisi/points.vct')
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == 'IntegerField'

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger

    sr = lyr.GetSpatialRef()
    assert sr.ExportToWkt().find('PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]') > 0

    assert lyr.GetFeatureCount() == 2

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

    assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1

    assert lyr.GetExtent() == (400000.0, 600000.0, 4000000.0, 5000000.0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 1.0:
        feat.DumpReadable()
        pytest.fail()

    if feat.GetFieldAsInteger(1) != 2:
        feat.DumpReadable()
        pytest.fail()

    if feat.GetFieldAsDouble(2) != 3.45:
        feat.DumpReadable()
        pytest.fail()

    if feat.GetFieldAsString(3) != 'foo':
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(400000 5000000)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (600000 4000000)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(600000 - 1, 4000000 - 1, 600000 + 1, 4000000 + 1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(0, 0, 1, 1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    ds = None

###############################################################################
# Test linestring layer


def test_ogr_idrisi_2():

    ds = ogr.Open('data/idrisi/lines.vct')
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbLineString

    assert lyr.GetFeatureCount() == 2

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

    assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1

    assert lyr.GetExtent() == (400000.0, 600000.0, 4000000.0, 5000000.0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 10.0:
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (400000 5000000,600000 4500000)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 20.0:
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (450000 4000000,550000 4500000)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(0, 0, 1, 1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    ds = None

###############################################################################
# Test polygon layer


def test_ogr_idrisi_3():

    ds = ogr.Open('data/idrisi/polygons.vct')
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon

    assert lyr.GetFeatureCount() == 2

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

    assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1

    assert lyr.GetExtent() == (400000.0, 600000.0, 4000000.0, 5000000.0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 1.0:
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((400000 4000000,400000 5000000,600000 5000000,600000 4000000,400000 4000000),(450000 4250000,450000 4750000,550000 4750000,550000 4250000,450000 4250000))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsDouble(0) != 2.0:
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((400000 4000000,400000 5000000,600000 5000000,600000 4000000,400000 4000000))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(0, 0, 1, 1)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    ds = None



