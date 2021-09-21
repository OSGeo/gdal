#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR MDB driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import ogr


import gdaltest
import ogrtest
import pytest


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    driver = ogr.GetDriverByName('MDB')
    if driver is not None:
        driver.Register()
    else:
        pytest.skip("MDB driver not available", allow_module_level=True)

    # remove PGeo driver
    pgeo_driver = ogr.GetDriverByName('PGeo')
    if pgeo_driver is not None:
        pgeo_driver.Deregister()

    yield

    if pgeo_driver is not None:
        print('Reregistering PGeo driver')
        pgeo_driver.Register()


@pytest.fixture()
def download_test_data():
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip', 'PGeoTest.zip'):
        pytest.skip("Test data could not be downloaded")

    try:
        os.stat('tmp/cache/Autodesk Test.mdb')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/PGeoTest.zip')
            try:
                os.stat('tmp/cache/Autodesk Test.mdb')
            except OSError:
                pytest.skip()
        except:
            pytest.skip()

    pgeo_ds = ogr.Open('tmp/cache/Autodesk Test.mdb')
    if pgeo_ds is None:
        pytest.skip('could not open DB. Driver probably misconfigured')

    return pgeo_ds

###############################################################################
# Basic testing


def test_ogr_mdb_1(download_test_data):
    assert download_test_data.GetLayerCount() == 3, 'did not get expected layer count'

    lyr = download_test_data.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if ogrtest.check_feature_geometry(feat, 'MULTILINESTRING ((1910941.703951031 445833.57942859828 0,1910947.927691862 445786.43811868131 0))', max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'


###############################################################################
# Test spatial filter


def test_ogr_mdb_2(download_test_data):
    lyr = download_test_data.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    bbox = geom.GetEnvelope()

    lyr.SetSpatialFilterRect(bbox[0], bbox[1], bbox[2], bbox[3])

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 6957, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    # Check that geometry filter is well cleared
    lyr.SetSpatialFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'

###############################################################################
# Test attribute filter


def test_ogr_mdb_3(download_test_data):
    lyr = download_test_data.GetLayer(0)
    lyr.SetAttributeFilter('OBJECTID=1')

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    # Check that attribute filter is well cleared (#3706)
    lyr.SetAttributeFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'

###############################################################################
# Test ExecuteSQL()


def test_ogr_mdb_4(download_test_data):
    sql_lyr = download_test_data.ExecuteSQL('SELECT * FROM SDPipes WHERE OBJECTID = 1')

    feat_count = sql_lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    download_test_data.ReleaseResultSet(sql_lyr)

###############################################################################
# Test GetFeature()


def test_ogr_mdb_5(download_test_data):
    lyr = download_test_data.GetLayer(0)
    feat = lyr.GetFeature(9418)
    if feat.GetField('OBJECTID') != 9418:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')


###############################################################################
# Run test_ogrsf


def test_ogr_mdb_6(download_test_data):
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "tmp/cache/Autodesk Test.mdb"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Run test_ogrsf with -sql


def test_ogr_mdb_7(download_test_data):
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "tmp/cache/Autodesk Test.mdb" -sql "SELECT * FROM SDPipes"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

