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
        assert gdal.Open(filename) is None

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
