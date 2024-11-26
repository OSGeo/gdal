#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAFE driver
# Author:   Delfim Rego <delfimrego@gmail.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("SAFE")

###############################################################################
# Test reading a - fake - SAFE dataset. Note: the tiff files are fake,
# reduced to 1% of their initial size and metadata stripped


def test_safe_1():

    tst = gdaltest.GDALTest("SAFE", "SAFE_FAKE/test.SAFE/manifest.safe", 1, 65372)
    tst.testOpen()

    ds = gdal.Open("data/SAFE_FAKE/test.SAFE/manifest.safe")
    assert (
        ds.GetMetadataItem("FOOTPRINT")
        == "POLYGON((-8.407759 38.130520,-11.335915 38.535374,-11.026125 40.036644,-8.035001 39.633217, -8.407759 38.130520))"
    )


def test_safe_2():

    tst = gdaltest.GDALTest("SAFE", "SAFE_FAKE/test.SAFE/manifest.safe", 2, 3732)
    tst.testOpen()


def test_safe_3():

    tst = gdaltest.GDALTest(
        "SAFE",
        "SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE/manifest.safe:IW_VH:AMPLITUDE",
        1,
        65372,
        filename_absolute=1,
    )
    tst.testOpen()


def test_safe_4():

    tst = gdaltest.GDALTest(
        "SAFE",
        "SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE/manifest.safe:IW_VV:AMPLITUDE",
        1,
        3732,
        filename_absolute=1,
    )
    tst.testOpen()


def test_safe_5():

    tst = gdaltest.GDALTest(
        "SAFE",
        "SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE:IW:AMPLITUDE",
        1,
        65372,
        filename_absolute=1,
    )
    tst.testOpen()


def test_safe_WV():

    ds = gdal.Open("data/SAFE_FAKE_WV")
    assert ds is not None
    subds = ds.GetSubDatasets()
    assert len(subds) == 10

    assert (
        "SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV1_VV_001:INTENSITY"
        in [x[0].replace("\\", "/") for x in subds]
    )

    ds = gdal.Open(
        "SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV1_VV_001:INTENSITY"
    )
    assert ds is not None
    assert len(ds.GetSubDatasets()) == 0
    assert len(ds.GetGCPs()) == 1

    assert (
        "SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV2_VV_002:INTENSITY"
        in [x[0].replace("\\", "/") for x in subds]
    )

    ds = gdal.Open(
        "SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV2_VV_002:INTENSITY"
    )
    assert ds is not None
    assert len(ds.GetSubDatasets()) == 0
    assert len(ds.GetGCPs()) == 2

    with pytest.raises(Exception):
        gdal.Open(subds[0][0] + "xxxx")
