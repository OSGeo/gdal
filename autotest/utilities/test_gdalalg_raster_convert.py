#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster convert' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_convert_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("convert")


def test_gdalalg_raster_convert(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["data/utmsmall.tif", out_filename])

    with gdal.Open(out_filename, gdal.GA_Update) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054
        ds.GetRasterBand(1).Fill(0)

    convert = get_convert_alg()
    with pytest.raises(
        Exception, match="already exists. Specify the --overwrite option"
    ):
        convert.ParseRunAndFinalize(["data/utmsmall.tif", out_filename])

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--overwrite", "--co=TILED=YES", "data/utmsmall.tif", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
        assert ds.GetRasterBand(1).Checksum() == 50054


def test_gdalalg_raster_convert_to_mem():
    convert = get_convert_alg()
    assert convert.ParseCommandLineArguments(["--of=MEM", "data/utmsmall.tif", ""])
    assert convert.Run()
    out_ds = convert.GetArg("output").Get().GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == 50054


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_convert_append(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["data/utmsmall.tif", out_filename])

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        [
            "data/utmsmall.tif",
            out_filename,
            "--append",
            "--co",
            "RASTER_TABLE=new_table",
        ]
    )

    with gdal.Open(out_filename) as ds:
        assert len(ds.GetSubDatasets()) == 2


def test_gdalalg_raster_convert_error_output_already_set():
    convert = get_convert_alg()
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    convert.GetArg("output").Get().SetDataset(ds)
    assert convert.ParseCommandLineArguments(["data/utmsmall.tif"])
    with pytest.raises(
        Exception,
        match="gdal raster convert does not support outputting to an already opened output dataset",
    ):
        convert.Run()
