#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster nodata-to-alph' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_raster_nodata_to_alpha_noop():

    with gdal.Run(
        "raster", "nodata-to-alpha", input="../gcore/data/byte.tif", output_format="MEM"
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert out_ds.GetRasterBand(1).Checksum() == 4672
        assert out_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID


def test_gdalalg_raster_nodata_to_alpha_nominal():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")

    with gdal.Run(
        "raster", "nodata-to-alpha", input=src_ds, output_format="MEM"
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        assert (
            out_ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(1).GetMaskFlags()
            == gdal.GMF_ALPHA | gdal.GMF_PER_DATASET
        )
        assert out_ds.GetRasterBand(2).ReadRaster() == b"\x00\xff"
        assert out_ds.GetRasterBand(2).GetMaskFlags() == gdal.GMF_ALL_VALID


def test_gdalalg_raster_nodata_override_nodata_single_value():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")

    with gdal.Run(
        "raster", "nodata-to-alpha", input=src_ds, output_format="MEM", nodata=[1]
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        assert (
            out_ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(1).GetMaskFlags()
            == gdal.GMF_ALPHA | gdal.GMF_PER_DATASET
        )
        assert out_ds.GetRasterBand(2).ReadRaster() == b"\x00\xff"
        assert out_ds.GetRasterBand(2).GetMaskFlags() == gdal.GMF_ALL_VALID


def test_gdalalg_raster_nodata_override_nodata_several_values():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 3)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, b"\x01\x02\x03")
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 3, 1, b"\x03\x01\x02")
    src_ds.GetRasterBand(3).WriteRaster(0, 0, 3, 1, b"\x03\x02\x01")

    with gdal.Run(
        "raster", "nodata-to-alpha", input=src_ds, output_format="MEM", nodata=[3, 2, 1]
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        assert (
            out_ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(1).GetMaskFlags()
            == gdal.GMF_ALPHA | gdal.GMF_PER_DATASET
        )
        assert (
            out_ds.GetRasterBand(2).ReadRaster() == src_ds.GetRasterBand(2).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(2).GetMaskFlags()
            == gdal.GMF_ALPHA | gdal.GMF_PER_DATASET
        )
        assert (
            out_ds.GetRasterBand(3).ReadRaster() == src_ds.GetRasterBand(3).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(3).GetMaskFlags()
            == gdal.GMF_ALPHA | gdal.GMF_PER_DATASET
        )
        assert out_ds.GetRasterBand(4).ReadRaster() == b"\xff\xff\x00"
        assert out_ds.GetRasterBand(4).GetMaskFlags() == gdal.GMF_ALL_VALID

    with pytest.raises(
        Exception,
        match="There should be 3 nodata values given the input dataset has 3 bands",
    ):
        gdal.Run(
            "raster",
            "nodata-to-alpha",
            input=src_ds,
            output_format="MEM",
            nodata=[3, 2],
        )
