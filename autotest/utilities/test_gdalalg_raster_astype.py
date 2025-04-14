#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster astype' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

from osgeo import gdal


def get_astype_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["astype"]


def test_gdalalg_raster_astype(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_astype_alg()
    alg["datatype"] = "UInt16"
    alg["input"] = "../gcore/data/rgbsmall.tif"
    alg["output"] = out_filename
    assert alg.Run() and alg.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16


def test_gdalalg_raster_astype_as_gdt(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_astype_alg()
    alg["datatype"] = gdal.GDT_UInt16
    alg["input"] = "../gcore/data/rgbsmall.tif"
    alg["output"] = out_filename
    assert alg.Run() and alg.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
