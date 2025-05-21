#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the GDALGCPsToHomography() method.
# Author:   Nathan Olson <nathanmolson at gmail dot com>
#
###############################################################################
# Copyright (c) 2025, Nathan Olson <nathanmolson at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Helper to make gcps


def _list2gcps(src_list):
    gcp_list = []
    for src_tuple in src_list:
        gcp = gdal.GCP()
        gcp.GCPPixel = src_tuple[0]
        gcp.GCPLine = src_tuple[1]
        gcp.GCPX = src_tuple[2]
        gcp.GCPY = src_tuple[3]
        gcp_list.append(gcp)
    return gcp_list


###############################################################################
# Test if homographies are equal with an epsilon tolerance
def check_homography(h1, h2, h_epsilon):
    return gdaltest.check_geotransform(h1, h2, h_epsilon)


###############################################################################
# Test if homography satisfies the GCP with an epsilon tolerance
def check_gcp(h, gcp, h_epsilon):
    success, x, y = gdal.ApplyHomography(h, gcp.GCPPixel, gcp.GCPLine)
    assert success
    assert x == pytest.approx(gcp.GCPX, h_epsilon)
    assert y == pytest.approx(gcp.GCPY, h_epsilon)


###############################################################################
# Test if homography satisfies all GCPs in list with an epsilon tolerance
def check_gcps(h, gcps, h_epsilon):
    for gcp in gcps:
        check_gcp(h, gcp, h_epsilon)


###############################################################################
# Test simple exact case of turning GCPs into a Homography.


def test_gcps2h_1():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (0.0, 0.0, 400000, 370000),
                (100.0, 0.0, 410000, 370000),
                (100.0, 200.0, 410000, 368000),
            ]
        )
    )
    check_homography(
        h, (400000.0, 100.0, 0.0, 370000.0, 0.0, -10.0, 1.0, 0.0, 0.0), 0.000001
    )


###############################################################################
# Similar but non-exact.


def test_gcps2h_2():

    gcps = _list2gcps(
        [
            (0.0, 0.0, 400000, 370000),
            (100.0, 0.0, 410000, 370000),
            (100.0, 200.0, 410000, 368000),
            (0.0, 200.0, 400000, 368000.01),
        ]
    )

    h = gdal.GCPsToHomography(gcps)
    assert h is not None
    check_gcps(h, gcps, 0.000001)


###############################################################################
# Not affine.


def test_gcps2h_3():

    gcps = _list2gcps(
        [
            (0.0, 0.0, 400000, 370000),
            (100.0, 0.0, 410000, 370000),
            (100.0, 200.0, 410000, 368000),
            (0.0, 200.0, 400000, 360000),
        ]
    )

    h = gdal.GCPsToHomography(gcps)
    assert h is not None
    check_gcps(h, gcps, 0.000001)


###############################################################################
# Single point - Should return None.


def test_gcps2h_4():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (0.0, 0.0, 400000, 370000),
            ]
        )
    )
    assert h is None, "Expected failure for single GCP."


###############################################################################
# Two points - simple offset and scale, no rotation.


def test_gcps2h_5():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (0.0, 0.0, 400000, 370000),
                (100.0, 200.0, 410000, 368000),
            ]
        )
    )
    check_homography(
        h, (400000.0, 100.0, 0.0, 370000.0, 0.0, -10.0, 1.0, 0.0, 0.0), 0.000001
    )


###############################################################################
# Special case for four points in a particular order.  Exact result.


def test_gcps2h_6():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (400000, 370000, 400000, 370000),
                (410000, 370000, 410000, 370000),
                (410000, 368000, 410000, 368000),
                (400000, 368000, 400000, 368000),
            ]
        )
    )
    check_homography(h, (0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0), 0.000001)


###############################################################################
# Try a case that is hard to do without normalization.


def test_gcps2h_7():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (400000, 370000, 400000, 370000),
                (410000, 368000, 410000, 368000),
                (410000, 370000, 410000, 370000),
                (400000, 368000, 400000, 368000),
            ]
        )
    )
    check_homography(h, (0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0), 0.000001)


###############################################################################
# A fairly messy real world case without a easy to predict result.


def test_gcps2h_8():

    h = gdal.GCPsToHomography(
        _list2gcps(
            [
                (0.01, 0.04, -87.05528672907, 39.22759504228),
                (0.01, 2688.02, -86.97079900719, 39.27075713986),
                (4031.99, 2688.04, -87.05960736744, 39.37569137000),
                (1988.16, 1540.80, -87.055069186699924, 39.304963106777514),
                (1477.41, 2400.83, -87.013419295885001, 39.304705030894979),
                (1466.02, 2376.92, -87.013906298363295, 39.304056190007913),
            ]
        )
    )
    h_expected = (
        -86.9154734797766,
        -0.000822802708802448,
        0.0016903358388202546,
        39.16439874542655,
        0.00038733423466157704,
        -0.0007330693484379306,
        0.9983801902671235,
        9.207539714141043e-06,
        -1.9069099634950863e-05,
    )
    check_homography(h, h_expected, 0.00001)


###############################################################################
# Test case of https://github.com/OSGeo/gdal/issues/11618


def test_gcps2h_broken_hour_glass():

    with pytest.raises(Exception, match=r"cross12 \* cross23 <= 0.0"):
        gdal.GCPsToHomography(
            _list2gcps(
                [
                    (0, 0, 0, 0),
                    (0, 10, 0, 10),
                    (10, 0, 10, 10),
                    (10, 10, 10, 0),
                ]
            )
        )

    with pytest.raises(Exception, match=r"cross12 \* cross23 <= 0.0"):
        gdal.GCPsToHomography(
            _list2gcps(
                [
                    (0, 0, 0, 0),
                    (0, 10, 10, 10),
                    (10, 0, 10, 0),
                    (10, 10, 0, 10),
                ]
            )
        )
