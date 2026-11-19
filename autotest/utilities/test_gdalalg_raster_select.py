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

import struct

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

    with gdal.Open(out_filename) as ds:
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


def test_gdalalg_raster_select_negative(tmp_vsimem):

    with gdal.alg.raster.select(
        input="../gcore/data/rgbsmall.tif", output="", output_format="MEM", band=[-1, 1]
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 2
        assert ds.GetRasterBand(1).Checksum() == 21349
        assert ds.GetRasterBand(2).Checksum() == 21212


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


@pytest.mark.parametrize(
    "bands,expected",
    (
        ["17:", (17, 18, 19, 20)],
        [":3", (1, 2, 3)],
        ["13:15", (13, 14, 15)],
        ["17::3", (17, 20)],
        [":4:3", (1, 4)],
        ["4:16:4", (4, 8, 12, 16)],
        ["::5", (1, 6, 11, 16)],
        ["-4:", (17, 18, 19, 20)],
        ["-4:-6", (17, 16, 15)],
        ["-4:-6:-2", (17, 15)],
        [["1:3:2", "2:4:2"], (1, 3, 2, 4)],
        ["2::100", (2,)],
        [":", tuple(range(1, 21))],
        [" : ", tuple(range(1, 21))],
        ["21:", "Invalid band: 21"],
        ["-21:", "Invalid band: -21"],
        ["0:15", "Invalid band: 0"],
        ["1:3:0", "Step value must be positive"],
        ["1:3:-1", "Step value must be positive"],
        ["3:1:0", "Step value must be negative"],
        ["3:1:1", "Step value must be negative"],
        ["1:3:5:7", "Invalid value for --band"],
        ["2.2:3", "Failed to parse start value of --band range"],
        ["2:3.2", "Failed to parse stop value of --band range"],
        ["2:3:0.1", "Failed to parse step value of --band range"],
    ),
)
def test_gdalalg_raster_select_range(bands, expected):

    nBands = 20
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, nBands)
    for i in range(nBands):
        src_ds.GetRasterBand(i + 1).Fill(i + 1)

    alg = gdal.Algorithm("raster", "select")
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["band"] = bands

    if type(expected) is tuple:
        assert alg.Run()
        dst_ds = alg.Output()
        assert dst_ds.RasterCount == len(expected)
        dat = struct.unpack("B" * len(expected), dst_ds.ReadRaster())
        assert dat == expected
    else:
        with pytest.raises(Exception, match=expected):
            alg.Run()


def test_gdalalg_raster_select_autocomplete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster select ../gcore/data/byte.tif --band last_word_is_complete=true"
    )
    assert out == ["1", "mask", "gray"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster select --band last_word_is_complete=true"
    )
    assert "description" in out
