#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster hillshade' testing
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
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["hillshade"]


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({}, 63031),
        ({"zfactor": 30}, 50999),
        ({"xscale": 111120, "yscale": 111120}, 63905),
        ({"xscale": 90000, "yscale": 111120}, 63319),
        ({"azimuth": 180}, 2228),
        ({"altitude": 90}, 48616),
        ({"gradient-alg": "ZevenbergenThorne"}, 62530),
        ({"variant": "combined"}, 51495),
        ({"variant": "multidirectional"}, 62942),
        ({"variant": "Igor"}, 51362),
        ({"no-edges": True}, 58409),
    ],
)
def test_gdalalg_raster_hillshade(options, checksum):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    for k in options:
        alg[k] = options[k]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert abs(out_ds.GetRasterBand(1).Checksum() - checksum) <= 1


@pytest.mark.parametrize(
    "options",
    [
        {"azimuth": 180, "variant": "multidirectional"},
        {"altitude": 90, "variant": "Igor"},
    ],
)
def test_gdalalg_raster_hillshade_incompatible_options(options):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    for k in options:
        alg[k] = options[k]
    with pytest.raises(Exception):
        alg.Run()


def test_gdalalg_raster_hillshade_band():

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
    assert out_ds.GetRasterBand(1).Checksum() == 63031


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_hillshade_gdalg(tmp_vsimem):

    out_filename = tmp_vsimem / "tmp.gdalg.json"

    alg = get_alg()
    alg["input"] = os.path.join(os.getcwd(), "../gdrivers/data/n43.tif")
    alg["output"] = out_filename
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 63031


def test_gdalalg_raster_hillshade_vrt_output_from_format():

    alg = get_alg()
    with pytest.raises(
        Exception,
        match=r"hillshade: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg["output-format"] = "VRT"


def test_gdalalg_raster_hillshade_vrt_output_from_filename():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = "i_do/not/exist/out.vrt"
    with pytest.raises(
        Exception,
        match=r"hillshade: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_hillshade_vrt_output_pipeline_from_format():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg[
        "pipeline"
    ] = "read ../gdrivers/data/n43.tif ! hillshade ! write i_do/not/exist/out.foo --output-format=vrt"
    with pytest.raises(
        Exception,
        match=r"hillshade: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_hillshade_vrt_output_pipeline_from_filename():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg[
        "pipeline"
    ] = "read ../gdrivers/data/n43.tif ! hillshade ! write i_do/not/exist/out.vrt"
    with pytest.raises(
        Exception,
        match=r"hillshade: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()
