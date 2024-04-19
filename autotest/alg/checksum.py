#!/usr/bin/env pytest
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  GDALChecksumImage() testing
# Author:   Even Rouault <even.rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even.rouault @ spatialys.com>
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

import pytest

from osgeo import gdal


@pytest.mark.parametrize("source", ["byte", "float32"])
def test_checksum(tmp_vsimem, source):

    tmpfilename = str(tmp_vsimem / "tmp.tif")

    src_ds = gdal.Open(f"../gcore/data/{source}.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tmpfilename, src_ds)
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmpfilename, src_ds, options=["TILED=YES", "BLOCKXSIZE=16", "BLOCKYSIZE=16"]
    )
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmpfilename, src_ds, options=["TILED=YES", "BLOCKXSIZE=32", "BLOCKYSIZE=16"]
    )
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmpfilename, src_ds, options=["TILED=YES", "BLOCKXSIZE=16", "BLOCKYSIZE=32"]
    )
    assert ds.GetRasterBand(1).Checksum() == 4672

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", 21, 21, 1, src_ds.GetRasterBand(1).DataType
    )
    mem_ds.WriteRaster(1, 1, 20, 20, src_ds.ReadRaster())
    assert mem_ds.GetRasterBand(1).Checksum(1, 1, 20, 20) == 4672
    assert mem_ds.GetRasterBand(1).Checksum() == 4568
