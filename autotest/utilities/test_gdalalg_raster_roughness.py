#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster roughness' testing
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
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["roughness"]


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({}, 48806),
        ({"no-edges": True}, 38624),
    ],
)
def test_gdalalg_raster_roughness(options, checksum):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    for k in options:
        alg[k] = options[k]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == checksum


def test_gdalalg_raster_roughness_band():

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
    assert out_ds.GetRasterBand(1).Checksum() == 48806


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_roughness_gdalg(tmp_vsimem):

    out_filename = tmp_vsimem / "tmp.gdalg.json"

    alg = get_alg()
    alg["input"] = os.path.join(os.getcwd(), "../gdrivers/data/n43.tif")
    alg["output"] = out_filename
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 48806


def test_gdalalg_raster_roughness_vrt_output_from_format():

    alg = get_alg()
    with pytest.raises(
        Exception,
        match=r"roughness: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg["output-format"] = "VRT"


def test_gdalalg_raster_roughness_vrt_output_from_filename():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = "i_do/not/exist/out.vrt"
    with pytest.raises(
        Exception,
        match=r"roughness: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_roughness_vrt_output_pipeline_from_format():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg[
        "pipeline"
    ] = "read ../gdrivers/data/n43.tif ! roughness ! write i_do/not/exist/out.foo --output-format=vrt"
    with pytest.raises(
        Exception,
        match=r"roughness: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_roughness_vrt_output_pipeline_from_filename():

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    alg[
        "pipeline"
    ] = "read ../gdrivers/data/n43.tif ! roughness ! write i_do/not/exist/out.vrt"
    with pytest.raises(
        Exception,
        match=r"roughness: VRT output is not supported. Consider using the GDALG driver instead \(files with \.gdalg\.json extension\)",
    ):
        alg.Run()


def test_gdalalg_raster_roughness_overview():

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/n43.tif",
        format="MEM",
        width=61,
        resampleAlg=gdal.GRIORA_Bilinear,
    )

    with gdal.Run(
        "raster",
        "roughness",
        input=src_ds,
        output="",
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        cs = out_ds.GetRasterBand(1).Checksum()
        stats = out_ds.GetRasterBand(1).ComputeStatistics(False)

    src_ds = gdal.Translate("", "../gdrivers/data/n43.tif", format="MEM")
    src_ds.BuildOverviews("BILINEAR", [2, 4])

    with gdal.Run(
        "raster",
        "roughness",
        input=src_ds,
        output="",
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
    del src_ds

    assert out_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert out_ds.GetRasterBand(1).GetOverview(-1) is None
    assert out_ds.GetRasterBand(1).GetOverview(2) is None
    assert out_ds.GetRasterBand(1).GetOverview(0).XSize == 61
    assert out_ds.GetRasterBand(1).GetOverview(0).YSize == 61
    assert out_ds.GetRasterBand(1).GetOverview(1).XSize == 31
    assert out_ds.GetRasterBand(1).GetOverview(1).YSize == 31
    assert out_ds.GetRasterBand(1).GetOverview(0).Checksum() == cs
    assert out_ds.GetRasterBand(1).GetOverview(0).ComputeStatistics(False) == stats
