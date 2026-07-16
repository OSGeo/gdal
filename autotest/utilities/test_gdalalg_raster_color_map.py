#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster color-map' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["color-map"]


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({}, [55066, 37594, 47768]),
        ({"add-alpha": True}, [55066, 37594, 47768, 48613]),
        ({"color-selection": "exact"}, [8073, 53707, 59536]),
        ({"color-selection": "nearest"}, [57296, 42926, 47181]),
    ],
)
@pytest.mark.parametrize("output_format", ["MEM", "VRT"])
def test_gdalalg_raster_color_map(options, checksum, output_format):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["color-map"] = "data/color_file.txt"
    alg["output"] = ""
    alg["output-format"] = output_format
    for k in options:
        alg[k] = options[k]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert [
        out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)
    ] == checksum


def test_gdalalg_raster_color_map_wrong_file():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["color-map"] = "i_do_not_exist.txt"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(Exception, match="Cannot find i_do_not_exist.txt"):
        alg.Run()


def test_gdalalg_raster_color_map_missing():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(
        Exception,
        match="color-map: Input dataset has no color table and 'color-map' option was not specified.",
    ):
        alg.Run()


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_color_map_gdalg(tmp_vsimem):

    out_filename = tmp_vsimem / "tmp.gdalg.json"

    alg = get_alg()
    alg["input"] = os.path.join(os.getcwd(), "../gdrivers/data/n43.tif")
    alg["output"] = out_filename
    alg["color-map"] = os.path.join(os.getcwd(), "data/color_file.txt")
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(out_filename) as ds:
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)] == [
            55066,
            37594,
            47768,
        ]


@pytest.mark.require_driver("BMP")
@pytest.mark.parametrize(
    "options,checksum",
    [
        ({}, [4672, 4672, 4672]),
        ({"color-selection": "nearest"}, [4672, 4672, 4672]),
        ({"add-alpha": True}, [4672, 4672, 4672, 4873]),
    ],
)
@pytest.mark.parametrize("output_format", ["MEM", "VRT"])
def test_gdalalg_raster_color_map_from_color_table(options, checksum, output_format):

    alg = get_alg()
    alg["input"] = "../gcore/data/8bit_pal.bmp"
    alg["output"] = ""
    alg["output-format"] = output_format
    for k in options:
        alg[k] = options[k]
    if "color-selection" in options:
        gdal.ErrorReset()
        with gdaltest.disable_exceptions(), gdal.quiet_errors():
            assert alg.Run()
            assert gdal.GetLastErrorMsg() != ""
    else:
        assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert [
        out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)
    ] == checksum


@pytest.mark.parametrize("output_format", ["MEM", "PNG"])
def test_gdalalg_raster_empty_color_map(tmp_vsimem, output_format):

    if gdal.GetDriverByName(output_format) is None:
        pytest.skip(f"{output_format} not available")

    gdal.FileFromMemBuffer(tmp_vsimem / "empty.txt", "")

    alg = get_alg()
    alg["input"] = "../gdrivers/data/n43.tif"
    alg["color-map"] = tmp_vsimem / "empty.txt"
    alg["output"] = tmp_vsimem / "out"
    alg["output-format"] = output_format
    with pytest.raises(Exception, match="No color association found"):
        alg.Run()


def test_gdalalg_raster_color_map_in_pipeline(tmp_vsimem):
    """Test fix for https://github.com/OSGeo/gdal/issues/13740"""

    with gdal.alg.raster.pipeline(
        input="../gcore/data/byte.tif",
        pipeline="read ! clip --bbox 440720.000,3750120.000,441920.000,3751320.000 ! color-map   --color-map data/color_file.txt ! clip --bbox 440720.000,3750120.000,441920.000,3751320.000 ! write",
        output_format="MEM",
    ) as alg:
        assert alg.Output().GetRasterBand(1).Checksum() == 4688
