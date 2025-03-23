#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster unscale' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

from osgeo import gdal


def get_unscale_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("unscale")


def test_gdalalg_raster_unscale_no_option():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(1).SetScale(2)
    src_ds.GetRasterBand(1).SetOffset(3)

    alg = get_unscale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (1 * 2 + 3, 1 * 2 + 3)


def test_gdalalg_raster_unscale_datatype():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(1).SetScale(2.1)
    src_ds.GetRasterBand(1).SetOffset(3)

    alg = get_unscale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-data-type"] = "Float64"
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (1 * 2.1 + 3, 1 * 2.1 + 3)
