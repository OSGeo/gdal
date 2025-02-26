#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  NTv2 Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("NTV2")

###############################################################################
# Open a little-endian NTv2 grid


def test_ntv2_1():

    tst = gdaltest.GDALTest("NTV2", "ntv2/test_ntv2_le.gsb", 2, 10)
    gt = (-5.52, 7.8, 0.0, 52.05, 0.0, -5.55)
    tst.testOpen(check_gt=gt, check_prj="WGS84")


###############################################################################
# Open a big-endian NTv2 grid


def test_ntv2_2():

    tst = gdaltest.GDALTest("NTV2", "ntv2/test_ntv2_be.gsb", 2, 10)
    gt = (-5.52, 7.8, 0.0, 52.05, 0.0, -5.55)
    tst.testOpen(check_gt=gt, check_prj="WGS84")


###############################################################################


def test_ntv2_online_1():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/proj/nzgd2kgrid0005.gsb", "nzgd2kgrid0005.gsb"
    )

    try:
        os.stat("tmp/cache/nzgd2kgrid0005.gsb")
    except OSError:
        pytest.skip()

    tst = gdaltest.GDALTest(
        "NTV2", "tmp/cache/nzgd2kgrid0005.gsb", 1, 54971, filename_absolute=1
    )
    gt = (165.95, 0.1, 0.0, -33.95, 0.0, -0.1)
    tst.testOpen(check_gt=gt, check_prj="WGS84")
