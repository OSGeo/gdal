#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LUT translation in VRT driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)

###############################################################################
# Simple test


def test_vrtlut_1():

    tst = gdaltest.GDALTest("VRT", "vrt/byte_lut.vrt", 1, 4655)
    tst.testOpen()


###############################################################################


@pytest.mark.require_driver("AAIGRID")
def test_vrtlut_with_nan():

    ds = gdal.Open("data/vrt/lut_with_nan.vrt")
    assert struct.unpack("B" * 2 * 3, ds.ReadRaster()) == (0, 10, 10, 15, 20, 20)
