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

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ESRIC")

###############################################################################
# Open the dataset


@pytest.fixture()
def esric_ds():

    ds = gdal.Open("data/esric/Layers/conf.xml")
    assert ds is not None, "open failed"

    return ds


###############################################################################
# Check that the configuration was read as expected


def test_esric_2(esric_ds):

    ds = esric_ds
    b1 = ds.GetRasterBand(1)

    assert (
        ds.RasterCount == 4 and ds.RasterXSize == 2048 and ds.RasterYSize == 2048
    ), "wrong size or band count"

    assert b1.GetOverviewCount() == 3, "Wrong number of overviews"

    wkt = ds.GetProjectionRef()
    assert 'AUTHORITY["EPSG","3857"]' in wkt, "wrong SRS"

    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-20037508, abs=1), "wrong geolocation"
    assert gt[1] == pytest.approx(20037508 / 1024, abs=1), "wrong geolocation"
    assert gt[2] == 0 and gt[4] == 0, "wrong geolocation"
    assert gt[3] == pytest.approx(20037508, abs=1), "wrong geolocation"
    assert gt[5] == pytest.approx(-20037508 / 1024, abs=1), "wrong geolocation"


###############################################################################
# Check that the read a missing level generates black


def test_esric_3(esric_ds):

    ds = esric_ds
    # There are no tiles at this level, driver will return black
    b1 = ds.GetRasterBand(1)
    cs = b1.Checksum()
    assert cs == 0, "wrong checksum from missing level"


###############################################################################
# Check that the read of PNG tiles returns the right checksum


@pytest.mark.require_driver("PNG")
def test_esric_4(esric_ds):

    ds = esric_ds

    # Read from level 1, band 2, where we have data
    # Overviews are counted from zero, in reverse order from levels

    l1b2 = ds.GetRasterBand(2).GetOverview(1)
    assert l1b2.XSize == 512 and l1b2.YSize == 512

    # There are four PNG tiles at this level, one is grayscale

    cs = l1b2.Checksum()
    expectedcs = 46857
    assert cs == expectedcs, "wrong data checksum"


###############################################################################
# Open the tpkx dataset


@pytest.fixture
def tpkx_ds():
    return gdal.Open("data/esric/Usa.tpkx")


###############################################################################
# Check that the configuration was read as expected


def test_tpkx_2(tpkx_ds):
    ds = tpkx_ds
    b1 = ds.GetRasterBand(1)

    assert (
        ds.RasterCount == 4 and ds.RasterXSize == 8192 and ds.RasterYSize == 8192
    ), "wrong size or band count"

    assert b1.GetOverviewCount() == 5, "Wrong number of overviews"

    wkt = ds.GetProjectionRef()
    assert 'AUTHORITY["EPSG","3857"]' in wkt, "wrong SRS"

    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-20037508, abs=1), "wrong geolocation"
    assert gt[1] == pytest.approx(20037508 / 4096, abs=1), "wrong geolocation"
    assert gt[2] == 0 and gt[4] == 0, "wrong geolocation"
    assert gt[3] == pytest.approx(20037508, abs=1), "wrong geolocation"
    assert gt[5] == pytest.approx(-20037508 / 4096, abs=1), "wrong geolocation"


###############################################################################
# Check that the raster returns right checksums


def test_tpkx_3(tpkx_ds):
    ds = tpkx_ds
    # There are no tiles at this level, driver will return black
    b1 = ds.GetRasterBand(1)
    b2 = ds.GetRasterBand(2)
    b3 = ds.GetRasterBand(3)
    b4 = ds.GetRasterBand(4)
    cs1 = b1.Checksum()
    cs2 = b2.Checksum()
    cs3 = b3.Checksum()
    cs4 = b4.Checksum()
    assert cs1 == 61275, "wrong checksum at band 1"
    assert cs2 == 57672, "wrong checksum at band 2"
    assert cs3 == 61542, "wrong checksum at band 3"
    assert cs4 == 19476, "wrong checksum at band 4"


###############################################################################
# Check that the read of PNG tiles returns the right checksum


@pytest.mark.require_driver("PNG")
def test_tpkx_4(tpkx_ds):
    ds = tpkx_ds

    # Read from level 1, band 2, where we have data
    # Overviews are counted from zero, in reverse order from levels

    l1b2 = ds.GetRasterBand(2).GetOverview(1)
    assert l1b2.XSize == 2048 and l1b2.YSize == 2048

    # There are four PNG tiles at this level, one is grayscale

    cs = l1b2.Checksum()
    expectedcs = 53503
    assert cs == expectedcs, "wrong data checksum"


###############################################################################
# Open a tpkx dataset where we need to ingest more bytes


def test_tpkx_ingest_more_bytes(tmp_vsimem):
    filename = str(tmp_vsimem / "root.json")
    f = gdal.VSIFOpenL("/vsizip/{data/esric/Usa.tpkx}/root.json", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    # Append spaces at the beginning of the root.json file to test we try
    # to ingest more bytes
    data = b"{" + (b" " * 900) + data[1:]
    gdal.FileFromMemBuffer(filename, data)
    gdal.Open(filename)
