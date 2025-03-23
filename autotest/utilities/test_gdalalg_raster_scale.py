#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster scale' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_scale_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("scale")


def test_gdalalg_raster_scale_no_option():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (0, 0)


def test_gdalalg_raster_scale_missing_srcmin():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmax"] = 0
    with pytest.raises(Exception, match="scale: srcmin must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_srcmax():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmin"] = 0
    with pytest.raises(Exception, match="scale: srcmax must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_dstmin():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmin"] = 0
    alg["srcmax"] = 0
    alg["dstmax"] = 0
    with pytest.raises(Exception, match="scale: dstmin must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_dstmax():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmin"] = 0
    alg["srcmax"] = 0
    alg["dstmin"] = 0
    with pytest.raises(Exception, match="scale: dstmax must be specified"):
        alg.Run()


def test_gdalalg_raster_scale():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(1).Fill(15)
    src_ds.GetRasterBand(2).Fill(10)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmin"] = 10
    alg["srcmax"] = 20
    alg["dstmin"] = 100
    alg["dstmax"] = 200
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (150, 150)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (100, 100)


def test_gdalalg_raster_scale_band():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(1).Fill(15)
    src_ds.GetRasterBand(2).Fill(10)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["band"] = 1
    alg["srcmin"] = 10
    alg["srcmax"] = 20
    alg["dstmin"] = 100
    alg["dstmax"] = 200
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (150, 150)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (10, 10)


def test_gdalalg_raster_exponent():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(1).Fill(15)
    src_ds.GetRasterBand(2).Fill(14)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["srcmin"] = 10
    alg["srcmax"] = 20
    alg["dstmin"] = 100
    alg["dstmax"] = 200
    alg["exponent"] = 1.5
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (135, 135)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (125, 125)


def test_gdalalg_raster_band_exponent_datatype():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(1).Fill(15)
    src_ds.GetRasterBand(2).Fill(14)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-data-type"] = "UInt16"
    alg["band"] = 2
    alg["srcmin"] = 10
    alg["srcmax"] = 20
    alg["dstmin"] = 100
    alg["dstmax"] = 200
    alg["exponent"] = 1.5
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (15, 15)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (125, 125)
