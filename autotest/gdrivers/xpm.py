#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for XPM driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
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
