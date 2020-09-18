#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PCRaster driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

from osgeo import gdal


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('PCRaster')

###############################################################################
# Perform simple read test.


def test_pcraster_1():

    tst = gdaltest.GDALTest('PCRaster', 'pcraster/ldd.map', 1, 4528)
    return tst.testOpen()

###############################################################################
# Verify some auxiliary data.


def test_pcraster_2():

    ds = gdal.Open('data/pcraster/ldd.map')

    gt = ds.GetGeoTransform()

    assert gt[0] == 182140.0 and gt[1] == 10 and gt[2] == 0 and gt[3] == 327880.0 and gt[4] == 0 and gt[5] == -10, \
        'PCRaster geotransform wrong.'

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == 255, 'PCRaster NODATA value wrong or missing.'

###############################################################################

def test_pcraster_createcopy():

    tst = gdaltest.GDALTest('PCRaster', 'pcraster/ldd.map', 1, 4528)
    return tst.testCreateCopy(new_filename = 'tmp/ldd.map')

###############################################################################

def test_pcraster_create():

    tst = gdaltest.GDALTest('PCRaster', 'float32.tif', 1, 4672, options=['PCRASTER_VALUESCALE=VS_SCALAR'])
    return tst.testCreate(new_filename = 'tmp/float32.map')
