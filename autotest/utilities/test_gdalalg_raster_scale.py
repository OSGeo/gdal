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

import struct

import pytest

from osgeo import gdal


def get_scale_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("scale")


@pytest.mark.parametrize(
    "dt,min,max",
    [
        (gdal.GDT_UInt8, 0, 255),
        (gdal.GDT_Int8, -128, 127),
        (gdal.GDT_UInt16, 0, 65535),
        (gdal.GDT_Int16, -32768, 32767),
        (gdal.GDT_UInt32, 0, (1 << 32) - 1),
        (gdal.GDT_Int32, -(1 << 31), (1 << 31) - 1),
        (gdal.GDT_UInt64, 0, 1.844674407370955e19),
        (gdal.GDT_Int64, -9.223372036854775e18, 9.223372036854773e18),
    ],
)
def test_gdalalg_raster_scale_no_option(dt, min, max):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2, 1, dt)
    src_ds.GetRasterBand(1).WriteRaster(
        0, 0, 1, 2, b"\x01\x02", buf_type=gdal.GDT_UInt8
    )

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (min, max)


def test_gdalalg_raster_scale_srcmin_srcmax_only():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 2, b"\x01\x02")

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 0
    alg["src-max"] = 3
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (85, 170)


def test_gdalalg_raster_scale_dstcmin_dstmax_only():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 2, b"\x01\x02")

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-min"] = 0
    alg["dst-max"] = 3
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (0, 3)


def test_gdalalg_raster_scale_missing_srcmin():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-max"] = 0
    with pytest.raises(Exception, match="scale: src-min must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_srcmax():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 0
    with pytest.raises(Exception, match="scale: src-max must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_dstmin():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-max"] = 0
    with pytest.raises(Exception, match="scale: dst-min must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_missing_dstmax():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(15)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-min"] = 0
    with pytest.raises(Exception, match="scale: dst-max must be specified"):
        alg.Run()


def test_gdalalg_raster_scale_srcmin_srcmax_destmin_dstmax():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(1).Fill(15)
    src_ds.GetRasterBand(2).Fill(10)

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 10
    alg["src-max"] = 20
    alg["dst-min"] = 100
    alg["dst-max"] = 200
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
    alg["src-min"] = 10
    alg["src-max"] = 20
    alg["dst-min"] = 100
    alg["dst-max"] = 200
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
    alg["src-min"] = 10
    alg["src-max"] = 20
    alg["dst-min"] = 100
    alg["dst-max"] = 200
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
    alg["src-min"] = 10
    alg["src-max"] = 20
    alg["dst-min"] = 100
    alg["dst-max"] = 200
    alg["exponent"] = 1.5
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (15, 15)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (125, 125)


def test_gdalalg_raster_scale_clip():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 5)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 5, b"\x00\x01\x02\x03\x04")

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 1
    alg["src-max"] = 3
    alg["dst-min"] = 100
    alg["dst-max"] = 200
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert struct.unpack("B" * 5, out_ds.GetRasterBand(1).ReadRaster()) == (
        100,
        100,
        150,
        200,
        200,
    )


def test_gdalalg_raster_scale_no_clip():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 5)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 5, b"\x00\x01\x02\x03\x04")

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 1
    alg["src-max"] = 3
    alg["dst-min"] = 100
    alg["dst-max"] = 200
    alg["no-clip"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert struct.unpack("B" * 5, out_ds.GetRasterBand(1).ReadRaster()) == (
        50,
        100,
        150,
        200,
        250,
    )


def test_gdalalg_raster_scale_no_clip_exponent():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 5)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 5, b"\x00\x01\x02\x03\x04")

    alg = get_scale_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-min"] = 1
    alg["src-max"] = 3
    alg["dst-min"] = 100
    alg["dst-max"] = 200
    alg["exponent"] = 1.1
    alg["no-clip"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert struct.unpack("B" * 5, out_ds.GetRasterBand(1).ReadRaster()) == (
        0,
        100,
        147,
        200,
        255,
    )
