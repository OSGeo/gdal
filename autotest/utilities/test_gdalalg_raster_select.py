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

import gdaltest
import pytest
import test_cli_utilities

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


def test_gdalalg_raster_select_exclude():

    with gdal.alg.raster.select(
        input="../gcore/data/rgbsmall.tif",
        output="",
        output_format="MEM",
        exclude=True,
        band=1,
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 2
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand

    with pytest.raises(Exception, match="Cannot exclude all input bands"):
        gdal.alg.raster.select(
            input="../gcore/data/rgbsmall.tif",
            output="",
            output_format="MEM",
            exclude=True,
            band=[1, 2, 3],
        )


def test_gdalalg_raster_select_exclude_wrong_color():

    with pytest.raises(Exception, match="Invalid band specification"):
        gdal.alg.raster.select(
            input="../gcore/data/rgbsmall.tif",
            output="",
            output_format="MEM",
            exclude=True,
            band="violet",
        )


def test_gdalalg_raster_select_by_band_color():

    with gdal.alg.raster.select(
        input="../gcore/data/rgbsmall.tif",
        output="",
        output_format="MEM",
        band=["green", "blue"],
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 2
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand

    with gdal.alg.raster.select(
        input="../gcore/data/rgbsmall.tif",
        output="",
        output_format="MEM",
        exclude=True,
        band=["red"],
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 2
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand

    with pytest.raises(Exception, match="Invalid band specification"):
        gdal.alg.raster.select(
            input="../gcore/data/rgbsmall.tif",
            output="",
            output_format="MEM",
            band="invalid",
        )

    with pytest.raises(Exception, match="No band has color interpretation alpha"):
        gdal.alg.raster.select(
            input="../gcore/data/rgbsmall.tif",
            output="",
            output_format="MEM",
            band="alpha",
        )

    with pytest.raises(Exception, match="No band has color interpretation undefined"):
        gdal.alg.raster.select(
            input="../gcore/data/rgbsmall.tif",
            output="",
            output_format="MEM",
            band="undefined",
        )


def test_gdalalg_raster_select_autocomplete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster select ../gcore/data/byte.tif --band last_word_is_complete=true"
    ).split(" ")
    assert out == ["1", "mask", "gray"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster select --band last_word_is_complete=true"
    )
    assert "description" in out
