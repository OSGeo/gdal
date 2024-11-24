#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for BT driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
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
