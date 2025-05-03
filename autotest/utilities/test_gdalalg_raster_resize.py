#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster resize' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_resize_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("resize")


def test_gdalalg_raster_resize(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_resize_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--size=10,0",
            "../gcore/data/byte.tif",
            out_filename,
        ],
        my_progress,
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterXSize == 10
        assert ds.RasterYSize == 10
        assert ds.GetRasterBand(1).Checksum() == 1192


def test_gdalalg_raster_resize_resampling(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_resize_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--size=0,10",
            "-r",
            "cubic",
            "../gcore/data/byte.tif",
            out_filename,
        ],
        my_progress,
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterXSize == 10
        assert ds.RasterYSize == 10
        assert ds.GetRasterBand(1).Checksum() == 1059


def test_gdalalg_raster_resize_percent(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_resize_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--size=50 %,25%",
            "-r",
            "cubic",
            "../gcore/data/byte.tif",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterXSize == 10
        assert ds.RasterYSize == 5


def test_gdalalg_raster_illegal_size():

    alg = get_resize_alg()
    with pytest.raises(Exception, match="Invalid size value"):
        alg["size"] = ["-1", "1"]
    with pytest.raises(Exception, match="Invalid size value"):
        alg["size"] = ["1.5", "1"]
    with pytest.raises(Exception, match="Invalid size value"):
        alg["size"] = ["1.5x", "1"]
    with pytest.raises(Exception, match="Invalid size value"):
        alg["size"] = ["1.5 x", "1"]
    with pytest.raises(Exception, match="Invalid size value"):
        alg["size"] = ["2147483648", "1"]
