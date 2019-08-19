#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR AVCE00 and AVCBin drivers
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
import pytest

###############################################################################
#


def check_content(ds):

    lyr = ds.GetLayerByName('ARC')
    expect = ['1', '2', '3', '4', '5', '6', '7']

    tr = ogrtest.check_features_against_list(lyr, 'UserID', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'LINESTRING (340099.875 4100200.0,340400.0625 4100399.5,340900.125 4100200.0,340700.03125 4100199.5)',
                                      max_error=0.01) == 0)

###############################################################################
# Open AVCE00 datasource.


def test_ogr_avc_1():

    # Example given at Annex A of http://avce00.maptools.org/docs/v7_e00_cover.html
    avc_ds = ogr.Open('data/test.e00')
    assert avc_ds.GetLayer(0).GetSpatialRef() is not None, 'expected SRS'

    if avc_ds is not None:
        return check_content(avc_ds)
    pytest.fail()

###############################################################################
# Open AVCBin datasource.


def test_ogr_avc_2():

    avc_ds = ogr.Open('data/testavc/testavc')
    assert avc_ds.GetLayer(0).GetSpatialRef() is not None, 'expected SRS'

    if avc_ds is not None:
        return check_content(avc_ds)
    pytest.fail()

###############################################################################
# Try opening a compressed E00 (which is not supported)


def test_ogr_avc_3():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    avc_ds = ogr.Open('data/compressed.e00')
    gdal.PopErrorHandler()
    last_error_msg = gdal.GetLastErrorMsg()

    assert avc_ds is None, 'expected failure'

    assert last_error_msg != '', 'expected error message'

###############################################################################
# Open larger AVCBin datasource.


def test_ogr_avc_4():

    for filename in ['data/testpointavc/testpointavc', 'data/testpoint.e00']:
        avc_ds = ogr.Open(filename)
        lyr = avc_ds.GetLayer(0)
        last_feature = None
        count = 0
        for f in lyr:
            count += 1
            last_feature = f
        assert count == 80, filename
        count = lyr.GetFeatureCount()
        assert count == 80, filename
        if last_feature.GetFieldCount() != 7:
            f.DumpReadable()
            pytest.fail(filename)
        if filename == 'data/testpointavc/testpointavc':
            fld_name = 'TESTPOINTAVC-ID'
        else:
            fld_name = 'WELLS-ID'
        if last_feature.GetField('ValueId') != 80 or last_feature.GetField(fld_name) != 80:
            f.DumpReadable()
            pytest.fail(filename)

    
###############################################################################
# Open AVCBin datasource with polygon


def test_ogr_avc_5():

    for filename in ['data/testpolyavc/testpolyavc', 'data/testpoly.e00']:
        avc_ds = ogr.Open(filename)
        lyr = avc_ds.GetLayerByName('PAL')
        last_feature = None
        count = 0
        for f in lyr:
            count += 1
            last_feature = f
        assert count == 3, filename
        count = lyr.GetFeatureCount()
        assert count == 3, filename
        if last_feature.GetFieldCount() != 5:
            f.DumpReadable()
            pytest.fail(filename)
        if last_feature.GetField('ArcIds') != [-4, -5] or last_feature.GetField('AREA') != pytest.approx(9939.059, abs=1e-3):
            f.DumpReadable()
            pytest.fail(filename)
        if filename == 'data/testpolyavc/testpolyavc':
            expected_wkt = 'POLYGON ((340700.03125 4100199.5,340500.0 4100199.75,340599.96875 4100100.25,340700.03125 4100199.5))'
        else:
            expected_wkt = 'POLYGON ((340700.03 4100199.5,340500.0 4100199.8,340599.97 4100100.2,340700.03 4100199.5))'
        if last_feature.GetGeometryRef().ExportToWkt() != expected_wkt:
            f.DumpReadable()
            pytest.fail(filename)

    


