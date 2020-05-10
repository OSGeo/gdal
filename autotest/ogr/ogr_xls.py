#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR XLS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
from osgeo import ogr
import pytest

###############################################################################
# Basic tests


def test_ogr_xls_1():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        pytest.skip()

    assert drv.TestCapability("foo") == 0

    ds = ogr.Open('data/xls/test972000xp.xls')
    assert ds is not None, 'cannot open dataset'

    assert ds.TestCapability("foo") == 0

    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Feuille1', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbNone, 'bad layer geometry type'

    assert lyr.GetSpatialRef() is None, 'bad spatial ref'

    assert lyr.GetFeatureCount() == 3

    assert lyr.TestCapability("foo") == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 5

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTReal and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTDateTime)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsInteger(0) != 1 or \
       feat.GetFieldAsDouble(1) != 1.0 or \
       feat.IsFieldSet(2) or \
       feat.GetFieldAsString(3) != '1980/01/01' or \
       feat.GetFieldAsString(4) != '1980/01/01 00:00:00':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat is None

###############################################################################
# Test OGR_XLS_HEADERS = DISABLE


def test_ogr_xls_2():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        pytest.skip()

    gdal.SetConfigOption('OGR_XLS_HEADERS', 'DISABLE')
    ds = ogr.Open('data/xls/test972000xp.xls')

    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 4

    gdal.SetConfigOption('OGR_XLS_HEADERS', None)

###############################################################################
# Test OGR_XLS_FIELD_TYPES = STRING


def test_ogr_xls_3():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        pytest.skip()

    gdal.SetConfigOption('OGR_XLS_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/xls/test972000xp.xls')

    lyr = ds.GetLayer(0)

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString

    gdal.SetConfigOption('OGR_XLS_FIELD_TYPES', None)

###############################################################################
# Run test_ogrsf


def test_ogr_xls_4():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/xls/test972000xp.xls')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1



