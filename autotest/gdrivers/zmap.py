#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ZMap driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver("ZMap")

###############################################################################
# Test CreateCopy() of byte.tif


def test_zmap_1():

    tst = gdaltest.GDALTest("ZMap", "byte.tif", 1, 4672)
    tst.testCreateCopy(
        vsimem=1,
        check_gt=(-67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333),
    )


###############################################################################
# Test setting nodata value to -3.402823E+38


def test_zmap_nodata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetNoDataValue(-3.402823e38)
    filename = "/vsimem/out.zmap"
    gdal.GetDriverByName("ZMap").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert ds
    assert (
        ds.GetRasterBand(1).GetNoDataValue() == src_ds.GetRasterBand(1).GetNoDataValue()
    )
    ds = None
    gdal.GetDriverByName("ZMap").Delete(filename)


###############################################################################
# Test variant of the format where there is no flush at end of column


def test_zmap_no_flush_end_of_column(tmp_path):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 5)
    src_ds.WriteRaster(0, 0, 2, 5, b"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09")
    filename = str(tmp_path / "out.zmap")
    with gdaltest.config_option("ZMAP_EMIT_EOL_AT_END_OF_COLUMN", "NO"):
        gdal.GetDriverByName("ZMap").CreateCopy(filename, src_ds)
    f = gdal.VSIFOpenL(filename, "rb")
    assert f
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)
    assert (
        data
        == b"!\n! Created by GDAL.\n!\n@GRID FILE, GRID, 4\n        20,               1E+30,          ,         7,         1\n         5,         2,     0.0000000,     2.0000000,    -5.0000000,     0.0000000\n0.0, 0.0, 0.0\n@\n           0.0000000           2.0000000           4.0000000           6.0000000\n           8.0000000           1.0000000           3.0000000           5.0000000\n           7.0000000           9.0000000\n"
    )
    ds = gdal.Open(filename)
    assert ds.ReadRaster(buf_type=gdal.GDT_Byte) == src_ds.ReadRaster()
