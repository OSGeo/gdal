#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster stack' testing
# Author:  Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


@pytest.fixture()
def stack():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["stack"]


def test_gdalalg_raster_stack_at_filename_error(stack):

    stack["input"] = ["@i_do_not_exist"]
    stack["output-format"] = "stream"
    with pytest.raises(Exception, match="stack: Cannot open i_do_not_exist"):
        stack.Run()


def test_gdalalg_raster_stack_resolution_common(stack):

    # resolution = 3
    src1_ds = gdal.GetDriverByName("MEM").Create("", 5, 5)
    src1_ds.SetGeoTransform([2, 3, 0, 49, 0, -3])

    # resolution = 5
    src2_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src2_ds.SetGeoTransform([2, 5, 0, 49, 0, -5])

    stack["input"] = [src1_ds, src2_ds]
    stack["output-format"] = "stream"
    stack["resolution"] = "common"
    assert stack.Run()
    ds = stack["output"].GetDataset()

    assert ds.RasterCount == 2
    assert ds.RasterXSize == 15
    assert ds.RasterYSize == 15
    assert ds.GetGeoTransform() == pytest.approx((2.0, 1.0, 0.0, 49.0, 0.0, -1.0))


def test_gdalalg_raster_stack_abolute_path(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.vrt")

    gdal.Translate(tmp_vsimem / "byte.tif", "../gcore/data/byte.tif")

    gdal.Run(
        "raster",
        "stack",
        input=tmp_vsimem / "byte.tif",
        output=out_filename,
        absolute_path=True,
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    with gdal.VSIFile(out_filename, "rb") as f:
        content = f.read().decode("utf-8")
    assert (
        '<SourceFilename relativeToVRT="0">' + str(tmp_vsimem / "byte.tif") in content
    )
    assert '<SourceFilename relativeToVRT="1">byte.tif' not in content


def test_gdalalg_raster_stack_pipeline():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src1_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src1_ds.GetRasterBand(1).Fill(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src2_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src2_ds.GetRasterBand(1).Fill(2)

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline="stack ! write --of=stream streamed_output",
        input=[src1_ds, src2_ds],
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.GetRasterBand(1).Checksum() == 3
        assert out_ds.GetRasterBand(2).Checksum() == 6
