#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RS2 driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("RS2")

###############################################################################
# Test reading a - fake - RS2 dataset. Note: the XML file was written by studying
# the code of the driver. It is really not meant as being used by other readers. If RS2 code
# evolves, this might break the test legitimately !


def test_rs2_1():
    tst = gdaltest.GDALTest("RS2", "rs2/product.xml", 1, 4672)
    tst.testOpen()


def test_rs2_2():
    tst = gdaltest.GDALTest(
        "RS2",
        "RADARSAT_2_CALIB:BETA0:data/rs2/product.xml",
        1,
        4848,
        filename_absolute=1,
    )
    tst.testOpen()


# Test reading our dummy RPC


def test_rs2_3():
    ds = gdal.Open("data/rs2/product.xml")
    got_rpc = ds.GetMetadata("RPC")
    expected_rpc = {
        "ERR_BIAS": "biasError",
        "ERR_RAND": "randomError",
        "HEIGHT_OFF": "heightOffset",
        "HEIGHT_SCALE": "heightScale",
        "LAT_OFF": "latitudeOffset",
        "LAT_SCALE": "latitudeScale",
        "LINE_DEN_COEFF": "lineDenominatorCoefficients",
        "LINE_NUM_COEFF": "lineNumeratorCoefficients",
        "LINE_OFF": "lineOffset",
        "LINE_SCALE": "lineScale",
        "LONG_OFF": "longitudeOffset",
        "LONG_SCALE": "longitudeScale",
        "SAMP_DEN_COEFF": "pixelDenominatorCoefficients",
        "SAMP_NUM_COEFF": "pixelNumeratorCoefficients",
        "SAMP_OFF": "pixelOffset",
        "SAMP_SCALE": "pixelScale",
    }
    assert got_rpc == expected_rpc
