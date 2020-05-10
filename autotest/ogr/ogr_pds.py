#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDS driver.
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



import gdaltest
import ogrtest
from osgeo import ogr
import pytest

###############################################################################
# Basic test


def test_ogr_pds_1():

    ds = ogr.Open('data/pds/ap01578l.lbl')
    assert ds is not None, 'cannot open dataset'

    lyr = ds.GetLayerByName('RAMAPPING')
    assert lyr is not None, 'cannot find layer'

    assert lyr.GetFeatureCount() == 74786, 'did not get expected feature count'

    with gdaltest.error_handler():
        feat = lyr.GetNextFeature()
    if feat.GetField('NOISE_COUNTS_1') != 96:
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeometryRef()
    if ogrtest.check_feature_geometry(feat, 'POINT (146.1325 -55.648)',
                                      max_error=0.000000001) != 0:
        print('did not get expected geom')
        pytest.fail(geom.ExportToWkt())

    with gdaltest.error_handler():
        feat = lyr.GetFeature(1)
    if feat.GetField('MARS_RADIUS') != 3385310.2:
        feat.DumpReadable()
        pytest.fail()

    
###############################################################################
# Read IEEE_FLOAT columns (see https://github.com/OSGeo/gdal/issues/570)


def test_ogr_pds_2():

    ds = ogr.Open('data/pds/virsvd_orb_11187_050618.lbl')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['INCIDENCE_ANGLE'] != pytest.approx(3.56775538, abs=1e-7) or f['TEMP_2'] != pytest.approx(28.1240005493164, abs=1e-7):
        f.DumpReadable()
        pytest.fail()

    


