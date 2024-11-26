#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some prime meridian related issues with EPSG translation
#           and evaluation by PROJ.4.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import osr

###############################################################################
# Check that EPSG:27572 lookup has the prime meridian properly set,
# and the central meridian.


def test_osr_pm_1():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(27572)

    assert float(srs.GetAttrValue("PRIMEM", 1)) == pytest.approx(
        2.33722917, abs=0.0000005
    ), "Wrong prime meridian."

    assert srs.GetProjParm(osr.SRS_PP_CENTRAL_MERIDIAN) == pytest.approx(
        0.0, abs=0.0000005
    ), "Wrong central meridian."


###############################################################################
# Check that EPSG:27572 lookup has the prime meridian properly set,
# and the central meridian in the PROJ.4 string.


def test_osr_pm_2():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(27572)
    proj4_srs = srs.ExportToProj4()

    assert proj4_srs.find("+pm=paris") != -1, "prime meridian wrong or missing."

    assert proj4_srs.find("+lon_0=0") != -1, "+lon_0 is wrong."


###############################################################################
# Convert PROJ.4 format to WKT and verify that PM and central meridian
# are properly preserved.


def test_osr_pm_3():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=utm +zone=30 +datum=WGS84 +pm=bogota")

    assert (
        abs(float(srs.GetAttrValue("PRIMEM", 1)) + 74.08091666678081) <= 0.0000005
    ), "Wrong prime meridian."

    assert (
        abs(srs.GetProjParm(osr.SRS_PP_CENTRAL_MERIDIAN) + 3.0) <= 0.0000005
    ), "Wrong central meridian."


###############################################################################
