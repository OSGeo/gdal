#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test AsyncReader interface
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


from osgeo import gdal

###############################################################################
# Test AsyncReader interface on the default (synchronous) implementation


def test_asyncreader_1():

    ds = gdal.Open("data/rgbsmall.tif")
    asyncreader = ds.BeginAsyncReader(0, 0, ds.RasterXSize, ds.RasterYSize)
    buf = asyncreader.GetBuffer()
    result = asyncreader.GetNextUpdatedRegion(0)
    assert result == [
        gdal.GARIO_COMPLETE,
        0,
        0,
        ds.RasterXSize,
        ds.RasterYSize,
    ], "wrong return values for GetNextUpdatedRegion()"
    ds.EndAsyncReader(asyncreader)
    asyncreader = None

    out_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/asyncresult.tif", ds.RasterXSize, ds.RasterYSize, ds.RasterCount
    )
    out_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf)

    expected_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

    ds = None
    out_ds = None
    gdal.Unlink("/vsimem/asyncresult.tif")

    for i, csum in enumerate(cs):
        assert csum == expected_cs[i], "did not get expected checksum for band %d" % (
            i + 1
        )
