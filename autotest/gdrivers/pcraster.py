#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PCRaster driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PCRaster")

###############################################################################
# Perform simple read test.


def test_pcraster_1():

    tst = gdaltest.GDALTest("PCRaster", "pcraster/ldd.map", 1, 4528)
    tst.testOpen()


###############################################################################
# Verify some auxiliary data.


def test_pcraster_2():

    ds = gdal.Open("data/pcraster/ldd.map")

    gt = ds.GetGeoTransform()

    assert (
        gt[0] == 182140.0
        and gt[1] == 10
        and gt[2] == 0
        and gt[3] == 327880.0
        and gt[4] == 0
        and gt[5] == -10
    ), "PCRaster geotransform wrong."

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == 255, "PCRaster NODATA value wrong or missing."


###############################################################################


def test_pcraster_createcopy():

    tst = gdaltest.GDALTest("PCRaster", "pcraster/ldd.map", 1, 4528)
    tst.testCreateCopy(new_filename="tmp/ldd.map")


###############################################################################


def test_pcraster_create():

    tst = gdaltest.GDALTest(
        "PCRaster", "float32.tif", 1, 4672, options=["PCRASTER_VALUESCALE=VS_SCALAR"]
    )
    tst.testCreate(new_filename="tmp/float32.map")
