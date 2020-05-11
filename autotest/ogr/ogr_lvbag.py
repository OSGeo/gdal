#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LVBAG driver functionality.
# Author:   Laixer B.V., info at laixer dot com
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


def test_ogr_lvbag_1():

    drv = ogr.GetDriverByName('LVBAG')
    if drv is None:
        pytest.skip()

    assert drv.TestCapability("foo") == 0

    ds = ogr.Open('data/lvbag/lig.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.TestCapability("foo") == 0
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Ligplaats', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    # assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.GetFeatureCount() == 3
    assert lyr.TestCapability("foo") == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 10

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTString)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != '1' or \
       feat.GetFieldAsString(1) != '2009-05-26' or \
       feat.GetFieldAsString(2) != '2009-11-06T13:37:22.000' or \
       feat.GetFieldAsString(3) != '2009-11-06T14:07:51.498' or \
       feat.GetFieldAsString(4) != 'NL.IMBAG.Ligplaats' or \
       feat.GetFieldAsString(5) != '0106020000000003' or \
       feat.GetFieldAsString(6) != 'Plaats aangewezen' or \
       feat.GetFieldAsString(7) != 'N' or \
       feat.GetFieldAsString(8) != '2009-05-26' or \
       feat.GetFieldAsString(9) != '2009-01000':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat is None

###############################################################################
# Run test_ogrsf


def test_ogr_lvbag_4():

    drv = ogr.GetDriverByName('LVBAG')
    if drv is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/lvbag/wpl.xml')

    assert 'INFO' not in ret and 'ERROR' not in ret


