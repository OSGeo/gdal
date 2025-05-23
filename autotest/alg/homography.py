#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Various tests of GDAL homography.
# Author:   Nathan Olson <nathanmolson at gmail dot com>
#
###############################################################################
# Copyright (c) 2025, Nathan Olson <nathanmolson at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

###############################################################################
# Test gdal.InvHomography()


def test_homography_1():

    h = (10, 0.1, 0, 20, 0, -1.0, 1.0, 0.0, 0.0)
    res = gdal.InvHomography(h)
    expected_inv_h = (-100.0, 10.0, 0.0, 20.0, 0.0, -1.0, 1.0, 0.0, 0.0)
    assert res == pytest.approx(expected_inv_h, abs=1e-6), res

    h = (3, 1, 2, 6, 4, 5, 11, 7, 8)
    res = gdal.InvHomography(h)
    expected_inv_h = (
        0.5,
        -1.166666666666666666,
        -0.333333333333333333,
        -1,
        0.3333333333333333333,
        1.6666666666666666666,
        0.5,
        0.5,
        -1,
    )
    assert res == pytest.approx(expected_inv_h, abs=1e-6), res

    h = (10, 1, 1, 20, 2, 2, 1, 0, 0)
    with pytest.raises(Exception, match="null determinant"):
        gdal.InvHomography(h)

    h = (10, 1e10, 1e10, 20, 2e10, 2e10, 1, 0, 0)
    with pytest.raises(Exception, match="null determinant"):
        gdal.InvHomography(h)

    h = (10, 1e-10, 1e-10, 20, 2e-10, 2e-10, 1, 0, 0)
    with pytest.raises(Exception, match="null determinant"):
        gdal.InvHomography(h)

    # Test fix for #1615
    h = (-2, 1e-8, 1e-9, 52, 1e-9, -1e-8, 1, 0, 0)
    res = gdal.InvHomography(h)
    expected_inv_h = (
        -316831683.16831684,
        99009900.990099,
        9900990.099009901,
        5168316831.683168,
        9900990.099009901,
        -99009900.990099,
        1.0,
        0.0,
        0.0,
    )
    assert res == pytest.approx(expected_inv_h, abs=1e-6), res
    res2 = gdal.InvHomography(res)
    assert res2 == pytest.approx(h, abs=1e-6), res2


###############################################################################
# Test gdal.ApplyHomography()


def test_homography_2():

    h = (10.0, 0.1, 0.0, 20.0, 0.0, -1.0, 1.0, 0.0, 0.0)
    success, x, y = gdal.ApplyHomography(h, 10, 1)
    assert success
    assert [x, y] == [11.0, 19.0]

    h = (10.0, 0.1, 3.0, 20.0, 7.0, -1.0, 2.0, 5.0, 4.0)
    success, x, y = gdal.ApplyHomography(h, 10, 1)
    assert success
    assert [x, y] == [0.25, 1.5892857142857142]
