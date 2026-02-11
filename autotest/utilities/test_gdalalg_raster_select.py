#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster select' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_select_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("select")


def test_gdalalg_raster_select(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_select_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--band=3,2,1,mask",
            "../gcore/data/rgbsmall.tif",
            out_filename,
        ],
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterCount == 4
        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
        )
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            21349,
            21053,
            21212,
            30658,
        ]


def test_gdalalg_raster_select_mask():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, b"\x00\x7f\xff")

    alg = get_select_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["band"] = ["1"]
    alg["mask"] = "1"
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\x7f\xff"
    assert out_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert out_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\x00\x7f\xff"


def test_gdalalg_raster_select_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_select_alg()
    with pytest.raises(Exception, match="Invalid band specification"):
        alg.ParseRunAndFinalize(
            [
                "--band=invalid",
                "../gcore/data/byte.tif",
                out_filename,
            ],
        )


def test_gdalalg_raster_select_mask_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_select_alg()
    with pytest.raises(Exception, match="Invalid mask band specification"):
        alg.ParseRunAndFinalize(
            [
                "--band=1",
                "--mask=invalid",
                "../gcore/data/byte.tif",
                out_filename,
            ],
        )
