#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal pipeline' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_pipeline_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    return reg.InstantiateAlg("pipeline")


def test_gdalalg_pipeline_read_and_write(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "write", out_filename], my_progress
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_pipeline_mixed_run_without_arg(tmp_vsimem):

    pipeline = get_pipeline_alg()
    pipeline.GetArg("input").Get().SetDataset(gdal.OpenEx("../ogr/data/poly.shp"))
    with pytest.raises(Exception, match="should not be called directly"):
        assert pipeline.Run()
