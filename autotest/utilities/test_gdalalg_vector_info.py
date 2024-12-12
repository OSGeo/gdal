#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector info' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal


def get_info_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    vector = reg.InstantiateAlg("vector")
    return vector.InstantiateSubAlgorithm("info")


def test_gdalalg_vector_info_stdout():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector info data/path.shp"
    )
    j = json.loads(out)
    assert j["layers"][0]["name"] == "path"
    assert "features" not in j["layers"][0]


def test_gdalalg_vector_info_text():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--format=text", "data/path.shp"])
    output_string = info.GetArg("output-string").Get()
    assert output_string.startswith("INFO: Open of")


def test_gdalalg_vector_info_json():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["data/path.shp"])
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert j["layers"][0]["name"] == "path"
    assert "features" not in j["layers"][0]


def test_gdalalg_vector_info_features():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--features", "data/path.shp"])
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert "features" in j["layers"][0]


def test_gdalalg_vector_info_sql():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(
        ["--sql", "SELECT 1 AS foo FROM path", "data/path.shp"]
    )
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert len(j["layers"][0]["fields"]) == 1
    assert j["layers"][0]["fields"][0]["name"] == "foo"


def test_gdalalg_vector_info_layer():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["-l", "path", "data/path.shp"])
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert j["layers"][0]["name"] == "path"


def test_gdalalg_vector_info_wrong_layer():
    info = get_info_alg()
    with pytest.raises(Exception, match="Couldn't fetch requested layer"):
        info.ParseRunAndFinalize(["-l", "invalid", "data/path.shp"])


@pytest.mark.parametrize("cond,featureCount", [("0", 0), ("1", 1)])
def test_gdalalg_vector_info_where(cond, featureCount):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--where", cond, "data/path.shp"])
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert j["layers"][0]["featureCount"] == featureCount


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_info_dialect():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(
        [
            "--features",
            "--dialect",
            "SQLite",
            "--sql",
            "SELECT sqlite_version() AS version",
            "data/path.shp",
        ]
    )
    output_string = info.GetArg("output-string").Get()
    j = json.loads(output_string)
    assert j["layers"][0]["features"][0]["properties"]["version"].startswith("3.")
