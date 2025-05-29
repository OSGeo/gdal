#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PNM (Portable Anyware Map) Testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PNM")

###############################################################################
# Read Test grayscale (PGM)


def test_pnm_1():

    tst = gdaltest.GDALTest("PNM", "pnm/byte.pgm", 1, 4672)

    tst.testOpen()


###############################################################################
# Write Test grayscale (PGM)


def test_pnm_2():

    tst = gdaltest.GDALTest("PNM", "pnm/byte.pgm", 1, 4672)

    tst.testCreateCopy(vsimem=1)


###############################################################################
# Read Test RGB (PPM)


def test_pnm_3():

    tst = gdaltest.GDALTest("PNM", "pnm/rgbsmall.ppm", 2, 21053)

    tst.testOpen()


###############################################################################
# Write Test RGB (PPM)


def test_pnm_4():

    tst = gdaltest.GDALTest("PNM", "pnm/rgbsmall.ppm", 2, 21053)

    tst.testCreateCopy()


@pytest.mark.parametrize("nbands", [1, 3])
@gdaltest.disable_exceptions()
def test_pnm_write_non_standard_extension(nbands):
    gdal.ErrorReset()
    with gdal.quiet_errors():
        gdal.GetDriverByName("PNM").Create("foo.foo", 1, 1, nbands)
    assert gdal.GetLastErrorType() != 0
    gdal.Unlink("foo.foo")


@gdaltest.disable_exceptions()
def test_pnm_read_int_max():

    with gdal.quiet_errors():
        ds = gdal.Open("data/pnm/int_max.pgm")
    if ds is None:
        pytest.skip("not enough memory")
    ds.GetRasterBand(1).Checksum()
