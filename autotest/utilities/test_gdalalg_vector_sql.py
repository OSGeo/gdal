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

import gdaltest
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
        ["../ogr/data/poly.shp", out_filename, "--sql=select * from poly limit 1"]
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
            "--format=ESRI Shapefile",
            "../ogr/data/poly.shp",
            out_filename,
            "--sql=select * from poly limit 1",
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
            ["../ogr/data/poly.shp", out_filename, "--sql=select * from poly", "error"]
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
                "--sql=select * from poly limit 1",
            ]
        )


def test_gdalalg_vector_sql_several(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out")

    sql_alg = get_sql_alg()
    assert sql_alg.ParseRunAndFinalize(
        [
            "../ogr/data/poly.shp",
            out_filename,
            "--format=ESRI Shapefile",
            "--sql=select * from poly limit 1",
            "--sql=select * from poly limit 2",
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
            "--sql=select *, sqlite_version() as version from poly limit 1",
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
            "--format=ESRI Shapefile",
            "--sql=select * from poly limit 1",
            "--sql=select * from poly limit 2",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerCount() == 2
        assert ds.GetLayer(0).GetFeatureCount() == 1
        assert ds.GetLayer(0).GetDescription() == "lyr1"
        assert ds.GetLayer(1).GetFeatureCount() == 2
        assert ds.GetLayer(1).GetDescription() == "lyr2"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_sql_no_result_set(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.gpkg"

    gdal.VectorTranslate(out_filename, "../ogr/data/poly.shp")

    with pytest.raises(
        Exception,
        match="Execution of the SQL statement 'PRAGMA foo=bar' did not result in a result layer",
    ):
        gdal.Run(
            "vector",
            "sql",
            input=out_filename,
            output=tmp_vsimem / "out.gpkg",
            sql="PRAGMA foo=bar",
        )

    with pytest.raises(
        Exception,
        match="Execution of the SQL statement 'PRAGMA foo=bar' did not result in a result layer",
    ):
        gdal.Run(
            "vector",
            "sql",
            input=out_filename,
            output=tmp_vsimem / "out.gpkg",
            sql=["SELECT 1", "PRAGMA foo=bar"],
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_sql_update_without_output(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.gpkg"

    gdal.VectorTranslate(out_filename, "../ogr/data/poly.shp")

    gdal.Run(
        "vector",
        "sql",
        update=True,
        input=out_filename,
        sql="DELETE FROM poly WHERE EAS_ID=170",
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 9


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_sql_overwrite_layer(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.gpkg"

    gdal.VectorTranslate(out_filename, "../ogr/data/poly.shp")

    with pytest.raises(
        Exception,
        match="already exists. You may specify the --overwrite/--overwrite-layer/--append/--update option",
    ):
        gdal.Run(
            "vector",
            "sql",
            input="../ogr/data/poly.shp",
            output=out_filename,
            sql="SELECT * FROM poly",
        )

    gdal.Run(
        "vector",
        "sql",
        input="../ogr/data/poly.shp",
        output=out_filename,
        sql="SELECT * FROM poly LIMIT 1",
        overwrite_layer=True,
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_sql_missing_update(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.gpkg"

    gdal.VectorTranslate(out_filename, "../ogr/data/poly.shp")

    with pytest.raises(
        Exception, match="Perhaps you need to specify the 'update' argument"
    ):
        gdal.Run("vector", "sql", input=out_filename, sql="DELETE FROM poly")


def test_gdalalg_vector_sql_update_return_result_set(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.shp"

    gdal.VectorTranslate(out_filename, "../ogr/data/poly.shp")

    with gdaltest.error_raised(gdal.CE_Warning, match="returned a result set"):
        gdal.Run("vector", "sql", input=out_filename, sql="SELECT * FROM poly")


def test_gdalalg_vector_sql_in_pipeline(tmp_vsimem):

    out_filename = tmp_vsimem / "poly.shp"

    gdal.Run(
        "vector",
        "pipeline",
        pipeline=f'read ../ogr/data/poly.shp | sql "SELECT * FROM poly WHERE eas_id=170" ! write {out_filename}',
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1
