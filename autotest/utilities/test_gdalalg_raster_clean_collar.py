#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster clean-collar' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["clean-collar"]


def test_gdalalg_raster_clean_collar():

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_clean_collar_update_output(tmp_vsimem):

    out_filename = tmp_vsimem / "out.tif"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert alg.Finalize()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["creation-option"] = {"COMPRESS": "LZW"}
    alg["overwrite"] = True
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename, gdal.GA_Update) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
        ds.GetRasterBand(1).Fill(0)

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["update"] = True
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_clean_collar_update_input_as_object():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x02\xff")

    alg = get_alg()
    alg["input"] = ds
    with pytest.raises(
        Exception,
        match="Output dataset is not specified. If you intend to update the input dataset, set the 'update' option",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    assert alg.Run()
    assert ds.GetRasterBand(1).ReadRaster() == b"\x00\xff"


def test_gdalalg_raster_clean_collar_update_input_as_name(tmp_vsimem):

    filename = tmp_vsimem / "test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 2, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x02\xff")
    ds.Close()

    alg = get_alg()
    alg["input"] = filename
    with pytest.raises(
        Exception,
        match="Output dataset is not specified. If you intend to update the input dataset, set the 'update' option",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = filename
    alg["update"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).ReadRaster() == b"\x00\xff"


def test_gdalalg_raster_clean_collar_color_and_pixel_distance():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\x7f\xd0\xff")

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    alg["color"] = ["black", "white", 0xD0]
    alg["pixel-distance"] = 0
    assert alg.Run()
    assert ds.GetRasterBand(1).ReadRaster() == b"\x00\x7f\x00\x00"

    alg = get_alg()
    with pytest.raises(Exception, match="Value for 'color' should be"):
        alg["color"] = "invalid"


def test_gdalalg_raster_clean_collar_color_tuple():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 2)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\x00\x01\xff")
    ds.GetRasterBand(2).WriteRaster(0, 0, 4, 1, b"\x01\x00\x01\xfe")

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    alg["color"] = ["0,1"]
    alg["pixel-distance"] = 0
    alg["color-threshold"] = 0
    assert alg.Run()
    assert ds.GetRasterBand(1).ReadRaster() == b"\x00\x00\x01\xff"
    assert ds.GetRasterBand(2).ReadRaster() == b"\x00\x00\x01\xfe"


def test_gdalalg_raster_clean_collar_color_threshold():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\x01\x02\x00")

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    alg["color"] = ["black", "white", 0xD0]
    alg["pixel-distance"] = 0
    alg["color-threshold"] = 1
    assert alg.Run()
    assert ds.GetRasterBand(1).ReadRaster() == b"\x00\x00\x02\x00"


def test_gdalalg_raster_clean_collar_add_alpha():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\xfe")

    alg = get_alg()
    alg["input"] = ds
    alg["output"] = "my_name"
    alg["output-format"] = "MEM"
    alg["add-alpha"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.RasterCount == 2
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\xfe"
    assert out_ds.GetRasterBand(2).ReadRaster() == b"\x00\xff"
    assert out_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand

    out_ds.GetRasterBand(2).Fill(0x7F)

    alg = get_alg()
    alg["input"] = ds
    alg["output"] = out_ds
    alg["update"] = True
    assert alg.Run()
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\xfe"
    assert out_ds.GetRasterBand(2).ReadRaster() == b"\x00\xff"


def test_gdalalg_raster_clean_collar_add_mask():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\xfe")

    alg = get_alg()
    alg["input"] = ds
    alg["output"] = "my_name"
    alg["output-format"] = "MEM"
    alg["add-mask"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.RasterCount == 1
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\xfe"
    assert out_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\x00\xff"

    out_ds.GetRasterBand(1).GetMaskBand().Fill(0x7F)

    alg = get_alg()
    alg["input"] = ds
    alg["output"] = out_ds
    alg["update"] = True
    assert alg.Run()
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\xfe"
    assert out_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\x00\xff"


def test_gdalalg_raster_clean_collar_add_mask_from_alpha():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\xfe")
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\xff\xff")

    alg = get_alg()
    alg["input"] = ds
    alg["output"] = "my_name"
    alg["output-format"] = "MEM"
    alg["add-mask"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.RasterCount == 1
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\xfe"
    assert out_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\x00\xff"


def test_gdalalg_raster_clean_collar_algorithm_default_is_floodfill():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 4)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\xff\xff\xff")
    ds.GetRasterBand(1).WriteRaster(0, 1, 4, 1, b"\x00\xff\x01\xff")
    ds.GetRasterBand(1).WriteRaster(0, 2, 4, 1, b"\x00\x00\x01\xff")
    ds.GetRasterBand(1).WriteRaster(0, 3, 4, 1, b"\x00\xff\xff\xff")

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    alg["pixel-distance"] = 0
    assert alg.Run()

    assert ds.ReadRaster(0, 0, 4, 1) == b"\x00\xff\xff\xff"
    assert ds.ReadRaster(0, 1, 4, 1) == b"\x00\xff\x00\xff"
    assert ds.ReadRaster(0, 2, 4, 1) == b"\x00\x00\x00\xff"
    assert ds.ReadRaster(0, 3, 4, 1) == b"\x00\xff\xff\xff"


def test_gdalalg_raster_clean_collar_algorithm_twopasses():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 4)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\xff\xff\xff")
    ds.GetRasterBand(1).WriteRaster(0, 1, 4, 1, b"\x00\xff\x01\xff")
    ds.GetRasterBand(1).WriteRaster(0, 2, 4, 1, b"\x00\x00\x01\xff")
    ds.GetRasterBand(1).WriteRaster(0, 3, 4, 1, b"\x00\xff\xff\xff")

    alg = get_alg()
    alg["input"] = ds
    alg["update"] = True
    alg["pixel-distance"] = 0
    alg["algorithm"] = "twopasses"
    assert alg.Run()

    assert ds.ReadRaster(0, 0, 4, 1) == b"\x00\xff\xff\xff"
    assert ds.ReadRaster(0, 1, 4, 1) == b"\x00\xff\x01\xff"
    assert ds.ReadRaster(0, 2, 4, 1) == b"\x00\x00\x00\xff"
    assert ds.ReadRaster(0, 3, 4, 1) == b"\x00\xff\xff\xff"
