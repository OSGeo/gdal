#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Northwood GRD driver
# Author:   Chaitanya kumar CH, <chaitanya at osgeo dot in>
#
###############################################################################
# Copyright (c) 2009, Chaitanya kumar CH, <chaitanya at osgeo dot in>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("NWT_GRD")

###############################################################################
# Test a GRD dataset with three bands + Z


def test_nwt_grd_1():

    tst1 = gdaltest.GDALTest("NWT_GRD", "nwt_grd/nwt_grd.grd", 1, 28093)
    tst1.testOpen()
    tst2 = gdaltest.GDALTest("NWT_GRD", "nwt_grd/nwt_grd.grd", 2, 33690)
    tst2.testOpen()
    tst3 = gdaltest.GDALTest("NWT_GRD", "nwt_grd/nwt_grd.grd", 3, 20365)
    tst3.testOpen()
    tst4 = gdaltest.GDALTest("NWT_GRD", "nwt_grd/nwt_grd.grd", 4, 25856)
    tst4.testOpen()


def test_nwt_grd_2():

    if (
        gdal.GetDriverByName("NWT_GRD").GetMetadataItem(gdal.DMD_CREATIONDATATYPES)
        is None
    ):
        pytest.skip("NWT_GRD driver has no write support due to missing MITAB driver")

    """
    Test writing a GRD via CreateCopy
    """
    shutil.copy("data/nwt_grd/nwt_grd.grd", "tmp/nwt_grd.grd")
    tst1 = gdaltest.GDALTest(
        "NWT_GRD",
        "tmp/nwt_grd.grd",
        1,
        25856,
        filename_absolute=1,
        open_options=["BAND_COUNT=1"],
    )
    ret = tst1.testCreateCopy(
        new_filename="tmp/out.grd", check_minmax=0, dest_open_options=["BAND_COUNT=1"]
    )
    gdal.Unlink("tmp/nwt_grd.grd")
    gdal.Unlink("tmp/nwt_grd.grd.aux.xml")
    return ret
