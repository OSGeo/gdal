#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver for Japanese FGD GML v4 testing.
# Author:   Hiroshi Miura <miurahr@linux.com>
#
###############################################################################
# Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>
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
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest


###############################################################################
# Test reading Japanese FGD GML (v4) files
###############################################################################

_fgd_dir = 'data/gml_jpfgd/'

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + 'ElevPt.xml')

    if ds is None:
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            pytest.skip()
        pytest.fail('failed to open test file.')


###############################################################################
# Test reading Japanese FGD GML (v4) ElevPt file

def test_ogr_gml_fgd_1():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + 'ElevPt.xml')

    # check number of layers
    assert ds.GetLayerCount() == 1, 'Wrong layer count'

    lyr = ds.GetLayer(0)

    # check the SRS
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)   # JGD2011
    assert sr.IsSame(lyr.GetSpatialRef(), options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'Wrong SRS'

    # check the first feature
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, 'POINT (133.123456789 34.123456789)'), \
        'Wrong geometry'

    assert feat.GetField('devDate') == '2015-01-07', 'Wrong attribute value'


###############################################################################
# Test reading Japanese FGD GML (v4) BldA file

def test_ogr_gml_fgd_2():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + 'BldA.xml')

    # check number of layers
    assert ds.GetLayerCount() == 1, 'Wrong layer count'

    lyr = ds.GetLayer(0)

    # check the SRS
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)   # JGD2011
    assert sr.IsSame(lyr.GetSpatialRef(), options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'Wrong SRS'

    wkt = 'POLYGON ((139.718509733734 35.6952171397133,139.718444177734 35.6953121947133,139.718496754142 35.6953498949667,139.718550483734 35.6952359447133,139.718509733734 35.6952171397133))'

    # check the first feature
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Wrong geometry'

    assert feat.GetField('devDate') == '2017-03-07', 'Wrong attribute value'
