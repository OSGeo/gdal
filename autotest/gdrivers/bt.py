#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for BT driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("BT")


@pytest.fixture(scope="module", autouse=True)
def setup_and_cleanup():

    yield

    for f in ("/vsimem/int16.tif.prj", "tmp/int32.tif.prj", "tmp/float32.tif.prj"):
        try:
            gdal.Unlink(f)
        except RuntimeError:
            pass


###############################################################################
# Test CreateCopy()


@pytest.mark.parametrize(
    "fname,to_vsimem",
    [("int16.tif", True), ("int32.tif", False), ("float32.tif", False)],
)
def test_bt_create_copy(fname, to_vsimem):

    tst = gdaltest.GDALTest("BT", fname, 1, 4672)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    tst.testCreateCopy(
        vsimem=to_vsimem,
        check_srs=srs.ExportToWkt(),
        check_gt=(-67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333),
    )


###############################################################################
# Test Create() of float32.tif


def test_bt_create():

    tst = gdaltest.GDALTest("BT", "float32.tif", 1, 4672)
    tst.testCreate(out_bands=1)


###############################################################################
# Test testSetProjection() of float32.tif


def test_bt_set_projection():

    tst = gdaltest.GDALTest("BT", "float32.tif", 1, 4672)
    tst.testSetProjection()


###############################################################################
# Test testSetGeoTransform() of float32.tif


def test_bt_set_geotransform():

    tst = gdaltest.GDALTest("BT", "float32.tif", 1, 4672)
    tst.testSetGeoTransform()
