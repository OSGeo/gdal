#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LERC driver.
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault, <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("LERC")


def test_lerc_byte_v2():
    ds = gdal.Open("data/mrf/lerc/byte.lrc")
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 512
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4819


def test_lerc_int8_v2():
    ds = gdal.Open("/vsisubfile/504_237,../gcore/data/gtiff/lerc_int8.tif")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int8
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4776


def test_lerc_uint16_v2():
    ds = gdal.Open("/vsisubfile/504_465,../gcore/data/gtiff/lerc_uint16.tif")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_lerc_int16_v2():
    ds = gdal.Open("/vsisubfile/504_465,../gcore/data/gtiff/lerc_int16.tif")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_lerc_uint32_v2():
    ds = gdal.Open("/vsisubfile/504_469,../gcore/data/gtiff/lerc_uint32.tif")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt32
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_lerc_int32_v2():
    ds = gdal.Open("/vsisubfile/504_469,../gcore/data/gtiff/lerc_int32.tif")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int32
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_lerc_float32_with_mask_v1():

    drv = gdal.GetDriverByName("LERC")
    if drv.GetMetadataItem("LIBLERC_VERSION") is None:
        pytest.skip("Internal liblerc doesn't support LercV1")

    ds = gdal.Open("data/mrf/lerc_v1/byte.lrc")
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 512
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_NODATA
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (0, 255)
    assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())
    assert ds.GetRasterBand(1).Checksum() == 26813


def test_lerc_byte_rgb_v2():
    ds = gdal.Open("/vsisubfile/524_5586,../gcore/data/gtiff/rgbsmall_LERC.tif")
    assert ds.RasterXSize == 50
    assert ds.RasterYSize == 50
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 21212
    assert ds.GetRasterBand(2).Checksum() == 21053
    assert ds.GetRasterBand(3).Checksum() == 21349


def test_lerc_float32_with_mask_v2():

    ds = gdal.Open(
        "/vsisubfile/304_1741,../gcore/data/gtiff/lerc_float32_with_mask.tif"
    )
    assert ds.RasterXSize == 22
    assert ds.RasterYSize == 24
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_NODATA
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (0, 255)
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4420
    assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())
    assert ds.GetRasterBand(1).Checksum() == 2755


def test_lerc_float32_with_mask_v2_as_mask():

    ds = gdal.Open(
        "/vsisubfile/304_1741,../gcore/data/gtiff/lerc_float32_with_mask.tif",
        open_options=["NDV=NONE"],
    )
    assert ds.RasterXSize == 22
    assert ds.RasterYSize == 24
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (0, 255)
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4420
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == 4591


def test_lerc_float64_with_mask_v2():

    ds = gdal.Open(
        "/vsisubfile/304_3349,../gcore/data/gtiff/lerc_float64_with_mask.tif"
    )
    assert ds.RasterXSize == 22
    assert ds.RasterYSize == 24
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_NODATA
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (0, 255)
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4420
    assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())
    assert ds.GetRasterBand(1).Checksum() == 2755
