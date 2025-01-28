#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Even Rouault, <even dot rouault at spatialys.com>
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("KRO")

###############################################################################
# Create 3-band byte


def test_kro_1():

    tst = gdaltest.GDALTest("KRO", "rgbsmall.tif", 2, 21053)

    tst.testCreate()


###############################################################################
# Create 1-band uint16


def test_kro_2():

    tst = gdaltest.GDALTest("KRO", "../../gcore/data/uint16.tif", 1, 4672)

    tst.testCreate()


###############################################################################
# Create 1-band float32


def test_kro_3():

    tst = gdaltest.GDALTest("KRO", "../../gcore/data/float32.tif", 1, 4672)

    tst.testCreate()


###############################################################################
# Create 4-band rgba uint16


def test_kro_4():

    tst = gdaltest.GDALTest("KRO", "png/rgba16.png", 1, 1886)

    tst.testCreate()


###############################################################################
# Test optimized IO


def test_kro_5():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files("tmp"):
        pytest.skip()

    ds = gdal.GetDriverByName("KRO").Create("tmp/kro_5.kro", 100000, 10000, 4)
    ds = None

    ds = gdal.Open("tmp/kro_5.kro")
    ds.ReadRaster(int(ds.RasterXSize / 2), int(ds.RasterYSize / 2), 100, 100)
    ds = None

    gdal.Unlink("tmp/kro_5.kro")
