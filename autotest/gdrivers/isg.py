#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ISG support.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ISG")

###############################################################################
# Perform simple read test.


def test_isg_1():

    tst = gdaltest.GDALTest("ISG", "isg/test.isg", 1, 159)
    expected_gt = [120.0, 0.25, 0.0, 41.0, 0.0, -0.25]
    tst.testOpen(check_gt=expected_gt)


###############################################################################
# Test if we can accept approximate georeferencing


def test_isg_approx_georeferencing_auto_corrected():

    gdal.ErrorReset()
    # Header of https://www.isgeoid.polimi.it/Geoid/America/Argentina/public/GEOIDEAR16_20160419.isg
    ds = gdal.Open("data/isg/approx_georeferencing_auto_corrected.isg")
    assert gdal.GetLastErrorMsg() == ""
    expected_gt = [-76.0098535, 0.016667, 0.0, -20.0087335, 0.0, -0.016667]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-8)


###############################################################################
# Test if we can accept approximate georeferencing


def test_isg_approx_georeferencing_with_warning():

    gdal.ErrorReset()
    # Header of https://www.isgeoid.polimi.it/Geoid/America/Argentina/public/GEOIDEAR16_20160419.isg
    # with delta_lon modified
    with gdal.quiet_errors():
        ds = gdal.Open("data/isg/approx_georeferencing_warning.isg")
        assert gdal.GetLastErrorMsg() != ""
    expected_gt = [
        -76.0083,
        0.01666694444444445,
        0.0,
        -20.0083,
        0.0,
        -0.016667027027027027,
    ]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-8)


###############################################################################
# Test if we can accept approximate georeferencing


def test_isg_approx_georeferencing_rejected_by_default():

    # Header of https://www.isgeoid.polimi.it/Geoid/America/Argentina/public/GEOIDEAR16_20160419.isg
    # with delta_lon modified
    filename = "data/isg/approx_georeferencing_rejected_by_default.isg"
    with pytest.raises(Exception, match="ISG_SKIP_GEOREF_CONSISTENCY_CHECK"):
        gdal.Open(filename)

    gdal.ErrorReset()
    with gdal.config_option(
        "ISG_SKIP_GEOREF_CONSISTENCY_CHECK", "YES"
    ), gdal.quiet_errors():
        ds = gdal.Open(filename)
        assert gdal.GetLastErrorMsg() != ""
    expected_gt = [
        -76.0083,
        0.01666694444444445,
        0.0,
        -20.0083,
        0.0,
        -0.016667027027027027,
    ]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-8)


###############################################################################
# Test reading a header larger than 1024 bytes


def test_isg_header_larger_than_1024bytes():

    # Header of https://isgeoid.polimi.it/Geoid/Europe/Slovenia/public/Slovenia_2016_SLO_VRP2016_Koper_hybrQ_20221122.isg
    with gdal.quiet_errors():
        ds = gdal.Open("data/isg/header_larger_than_1024bytes.isg")
    expected_gt = [12.99375, 0.0125, 0.0, 47.00416666666666, 0.0, -0.008333333333333333]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-8)


###############################################################################
# Test if we can read dms angles


def test_isg_dms():

    gdal.ErrorReset()
    # Header of https://www.gsi.go.jp/butsuri/data/GSIGEO2024beta.zip
    ds = gdal.Open("data/isg/header_dms.isg")
    assert gdal.GetLastErrorMsg() == ""
    expected_gt = [119.9875, 0.025, 0.0, 50.0083333333, 0.0, -0.01666666666]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-8)
