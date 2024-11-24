#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for XPM driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

xpm_list = [("http://download.osgeo.org/gdal/data/xpm", "utm.xpm", 44206, -1)]

pytestmark = pytest.mark.require_driver("XPM")


@pytest.mark.parametrize(
    "downloadURL,fileName,checksum,download_size",
    xpm_list,
    ids=[item[1] for item in xpm_list],
)
def test_xpm(downloadURL, fileName, checksum, download_size):
    gdaltest.download_or_skip(downloadURL + "/" + fileName, fileName, download_size)

    ds = gdal.Open("tmp/cache/" + fileName)

    assert (
        ds.GetRasterBand(1).Checksum() == checksum
    ), "Bad checksum. Expected %d, got %d" % (checksum, ds.GetRasterBand(1).Checksum())


def test_xpm_1():
    tst = gdaltest.GDALTest("XPM", "byte.tif", 1, 4583)
    tst.testCreateCopy(vsimem=1, check_minmax=False)
