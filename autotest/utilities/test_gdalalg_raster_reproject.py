#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_reproject_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("reproject")


def test_gdalalg_raster_reproject(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_reproject_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "--src-crs=EPSG:32611",
            "--dst-crs=EPSG:4326",
            "../gcore/data/byte.tif",
            out_filename,
        ],
        my_progress,
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4727


def test_gdalalg_raster_reproject_failure(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_reproject_alg()
    with pytest.raises(Exception, match="Unable to compute a transformation"):
        pipeline.ParseRunAndFinalize(
            [
                "--src-crs=EPSG:32611",
                "--dst-crs=EPSG:4326",
                "../gcore/data/nan32.tif",
                out_filename,
            ],
        )
