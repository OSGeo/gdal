#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster tpi' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["tpi"]


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({}, 1147),
        ({"no-edges": True}, 60504),
    ],
)
def test_gdalalg_raster_tpi(options, checksum):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    for k in options:
        alg[k] = options[k]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == checksum


def test_gdalalg_raster_tpi_band():

    src_ds = gdal.Translate(
        "", "../gdrivers/data/n43.tif", format="MEM", bandList=[1, 1]
    )
    src_ds.GetRasterBand(1).Fill(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["band"] = 2
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == 1147


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_tpi_gdalg(tmp_vsimem):

    out_filename = tmp_vsimem / "tmp.gdalg.json"

    alg = get_alg()
    alg["input"] = os.path.join(os.getcwd(), "../gdrivers/data/n43.tif")
    alg["output"] = out_filename
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 1147


def test_gdalalg_raster_tpi_vrt_output_from_format():

    alg = get_alg()
    with pytest.raises(
        Exception,
        match=r"tpi: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg["output-format"] = "VRT"


def test_gdalalg_raster_tpi_vrt_output_from_filename():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = "i_do/not/exist/out.vrt"
    with pytest.raises(
        Exception,
        match=r"tpi: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_tpi_vrt_output_pipeline_from_format():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg["pipeline"] = (
        "read ../gdrivers/data/n43.tif ! tpi ! write i_do/not/exist/out.foo --output-format=vrt"
    )
    with pytest.raises(
        Exception,
        match=r"tpi: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_tpi_vrt_output_pipeline_from_filename():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg["pipeline"] = (
        "read ../gdrivers/data/n43.tif ! tpi ! write i_do/not/exist/out.vrt"
    )
    with pytest.raises(
        Exception,
        match=r"tpi: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()
