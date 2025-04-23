#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Terrage Testing.
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault, <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("TERRAGEN")

###############################################################################


def test_terragen_1():

    tst = gdaltest.GDALTest("terragen", "terragen/float32.ter", 1, 1128)

    tst.testOpen()


###############################################################################
# Write


def test_terragen_2():

    gdal.Translate(
        "/vsimem/out.ter",
        "data/float32.tif",
        options="-of TERRAGEN -co MINUSERPIXELVALUE=74 -co MAXUSERPIXELVALUE=255",
    )
    gdal.Translate("/vsimem/out.tif", "/vsimem/out.ter", options="-unscale")
    ds = gdal.Open("/vsimem/out.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.GetDriverByName("TERRAGEN").Delete("/vsimem/out.ter")
    gdal.GetDriverByName("TERRAGEN").Delete("/vsimem/out.tif")
