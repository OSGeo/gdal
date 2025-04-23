#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector sql' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_sql_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    vector = reg.InstantiateAlg("vector")
    return vector.InstantiateSubAlgorithm("sql")


def test_gdalalg_vector_sql_base(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename, "select * from poly limit 1"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(-1) is None
        assert ds.GetLayer(1) is None


def test_gdalalg_vector_sql_layer_name(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        [
            "--output-layer=foo",
            "../ogr/data/poly.shp",
            out_filename,
            "select * from poly limit 1",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(0).GetName() == "foo"


def test_gdalalg_vector_sql_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    sql_alg = get_sql_alg()
    with pytest.raises(Exception):
        sql_alg.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename, "error"])


def test_gdalalg_vector_sql_error_2_layers(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    sql_alg = get_sql_alg()
    with pytest.raises(Exception):
        sql_alg.ParseRunAndFinalize(
            ["../ogr/data/poly.shp", out_filename, "select * from poly", "error"]
        )


def test_gdalalg_vector_sql_layer_name_inconsistent_number(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out")

    sql_alg = get_sql_alg()
    with pytest.raises(
        Exception,
        match="sql: There should be as many layer names in --output-layer as in --statement",
    ):
        sql_alg.ParseRunAndFinalize(
            [
                "--output-layer=foo,bar",
                "../ogr/data/poly.shp",
                out_filename,
                "select * from poly limit 1",
            ]
        )


def test_gdalalg_vector_sql_several(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        [
            "../ogr/data/poly.shp",
            out_filename,
            "select * from poly limit 1",
            "select * from poly limit 2",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerCount() == 2
        assert ds.GetLayer(-1) is None
        assert ds.GetLayer(2) is None
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(0).GetDescription() == "poly"
        assert ds.GetLayer(1).GetFeatureCount() == 2
        assert ds.GetLayer(1).GetDescription() == "poly2"
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(0).GetDescription() == "poly"


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_sql_dialect(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        [
            "--dialect",
            "SQLite",
            "../ogr/data/poly.shp",
            out_filename,
            "select *, sqlite_version() from poly limit 1",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_vector_sql_layer_names(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        [
            "--output-layer",
            "lyr1,lyr2",
            "../ogr/data/poly.shp",
            out_filename,
            "select * from poly limit 1",
            "select * from poly limit 2",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerCount() == 2
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(0).GetDescription() == "lyr1"
        assert ds.GetLayer(1).GetFeatureCount() == 2
        assert ds.GetLayer(1).GetDescription() == "lyr2"
