#!/usr/bin/env pytest
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  GDALChecksumImage() testing
# Author:   Even Rouault <even.rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even.rouault @ spatialys.com>
#
# SPDX-License-Identifier: MIT
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
