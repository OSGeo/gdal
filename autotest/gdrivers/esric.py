#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for Esri Compact Cache driver.
# Author:   Lucian Plesea <lplesea at esri.com>
#
###############################################################################
# Copyright (c) 2020, Esri
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

# from osgeo import osr

from osgeo import gdal
import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('ESRIC')

###############################################################################
# Open the dataset

def test_esric_1():

    gdaltest.esric_ds = gdal.Open("data/esric/Layers/conf.xml")
    assert gdaltest.esric_ds is not None, "open failed"


###############################################################################
# Check that the configuration was read as expected

def test_esric_2():

    if gdaltest.esric_ds is None:
        pytest.skip()

    ds = gdaltest.esric_ds
    b1 = ds.GetRasterBand(1)

    assert ds.RasterCount == 4 and ds.RasterXSize == 2048 and ds.RasterYSize == 2048, \
            "wrong size or band count"

    assert b1.GetOverviewCount() == 3, "Wrong number of overviews"

    wkt = ds.GetProjectionRef()
    assert 'AUTHORITY["EPSG","3857"]' in wkt, 'wrong SRS'

    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-20037508, abs=1), 'wrong geolocation'
    assert gt[1] == pytest.approx( 20037508 / 1024, abs=1), 'wrong geolocation'
    assert gt[2] == 0 and gt[4] == 0, 'wrong geolocation'
    assert gt[3] == pytest.approx( 20037508, abs=1), 'wrong geolocation'
    assert gt[5] == pytest.approx(-20037508 / 1024, abs=1), 'wrong geolocation'


###############################################################################
# Check that the read a missing level generates black

def test_esric_3():

    if gdaltest.esric_ds is None:
        pytest.skip()

    ds = gdaltest.esric_ds
    # There are no tiles at this level, driver will return black
    b1 = ds.GetRasterBand(1)
    cs = b1.Checksum()
    assert cs == 0, 'wrong checksum from missing level'

###############################################################################
# Check that the read of PNG tiles returns the right checksum

def test_esric_4():

    if gdaltest.esric_ds is None:
        pytest.skip()

    # Check that the PNG driver is available
    if not gdal.GetDriverByName('PNG'):
        pytest.skip()

    ds = gdaltest.esric_ds

    # Read from level 1, band 2, where we have data
    # Overviews are counted from zero, in reverse order from levels

    l1b2 = ds.GetRasterBand(2).GetOverview(1)
    assert l1b2.XSize == 512 and l1b2.YSize == 512

    # There are four PNG tiles at this level, one is grayscale

    cs = l1b2.Checksum()
    expectedcs = 46857
    assert cs == expectedcs, 'wrong data checksum'

def test_esric_cleanup():

    gdaltest.esric_ds = None
