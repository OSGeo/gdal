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

import gdaltest
import pytest

from osgeo import gdal


@pytest.fixture(scope="module")
def spatialite_version():

    version = None

    with gdaltest.disable_exceptions(), gdal.quiet_errors():
        ds = gdal.GetDriverByName("SQLite").CreateDataSource(
            "/vsimem/foo.db", options=["SPATIALITE=YES"]
        )

        if ds is not None:
            sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
            feat = sql_lyr.GetNextFeature()
            version = feat.GetFieldAsString(0)
            ds.ReleaseResultSet(sql_lyr)
        ds = None
        gdal.Unlink("/vsimem/foo.db")

    return version


@pytest.fixture()
def require_spatialite(spatialite_version):

    if spatialite_version is None:
        pytest.skip("SpatiaLite not available")


def get_info_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["info"]


def test_gdalalg_vector_info_stdout_text_default_format():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, _ = gdaltest.runexternal_out_and_err(f"{gdal_path} vector info data/path.shp")
    assert out.startswith("INFO: Open of")


def test_gdalalg_vector_info_stdout_json():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector info --format json data/path.shp"
    )
    j = json.loads(out)
    assert j["layers"][0]["name"] == "path"
    assert "features" not in j["layers"][0]


def test_gdalalg_vector_info_text():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--format=text", "data/path.shp"])
    output_string = info["output-string"]
    assert output_string.startswith("INFO: Open of")
    assert "Layer name: path" in output_string
    assert "Geometry: Line String" in output_string
    assert "Feature Count: 1" in output_string
    assert "OGRFeature" not in output_string


def test_gdalalg_vector_info_json():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["layers"][0]["name"] == "path"
    assert "features" not in j["layers"][0]


def test_gdalalg_vector_info_features_text():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--format=text", "--features", "data/path.shp"])
    output_string = info["output-string"]
    assert output_string.startswith("INFO: Open of")
    assert "Layer name: path" in output_string
    assert "Geometry: Line String" in output_string
    assert "Feature Count: 1" in output_string
    assert "OGRFeature" in output_string


def test_gdalalg_vector_info_features_json():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--features", "data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "features" in j["layers"][0]


def test_gdalalg_vector_info_features_limit_json():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--limit=2", "../ogr/data/poly.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert len(j["layers"][0]["features"]) == 2


def test_gdalalg_vector_info_sql():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(
        ["--sql", "SELECT 1 AS foo FROM path", "data/path.shp"]
    )
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert len(j["layers"][0]["fields"]) == 1
    assert j["layers"][0]["fields"][0]["name"] == "foo"


def test_gdalalg_vector_info_layer():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["-l", "path", "data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["layers"][0]["name"] == "path"


def test_gdalalg_vector_info_wrong_layer():
    info = get_info_alg()
    with pytest.raises(Exception, match="Cannot find source layer 'invalid'"):
        info.ParseRunAndFinalize(["-l", "invalid", "data/path.shp"])


@pytest.mark.parametrize("cond,featureCount", [("0", 0), ("1", 1)])
def test_gdalalg_vector_info_where(cond, featureCount):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--where", cond, "data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["layers"][0]["featureCount"] == featureCount


def test_gdalalg_vector_info_summary():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--summary", "data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "features" not in j["layers"][0]
    assert "featureCount" not in j["layers"][0]
    assert "geometryFields" not in j["layers"][0]

    with pytest.raises(RuntimeError, match="mutually exclusive with"):
        info = get_info_alg()
        info.ParseRunAndFinalize(["--summary", "--features", "data/poly.shp"])

    # To check that featureCount is normally present unless --summary is used
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "features" not in j["layers"][0]
    assert "features" not in j["layers"][0]
    assert "geometryFields" in j["layers"][0]

    # Test text output
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--summary", "--format=text", "data/path.shp"])
    output_string = info["output-string"]
    assert (
        output_string
        == "INFO: Open of `data/path.shp'\n      using driver `ESRI Shapefile' successful.\n1: path (Line String)\n"
    )


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_info_summary_multi_geometry(tmp_vsimem, require_spatialite):

    sqlite_multi = tmp_vsimem / "multi_geometry.db"
    # Create a temporary gpkg with multiple geometry types
    ds = gdal.VectorTranslate(
        sqlite_multi,
        "data/path.shp",
        format="SQLite",
        SQLStatement="select geometry as geom, st_centroid(geometry) as center, geometry as geom3, st_buffer(geometry,1) as geom4 from path",
        SQLDialect="SQLite",
    )
    # Exec sql to alter center and geom4
    ds.ExecuteSQL(
        "UPDATE geometry_columns SET geometry_type = 1 where f_geometry_column = 'center'"
    )
    ds.ExecuteSQL(
        "UPDATE geometry_columns SET geometry_type = 3 where f_geometry_column = 'geom4'"
    )
    # Check that the layer has multiple geometry types
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--summary", sqlite_multi])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["layers"][0]["geometryType"] == [
        "Line String",
        "Point",
        "Line String",
        "Polygon",
    ]

    # Check text format
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--summary", "--format=text", sqlite_multi])
    output_string = info["output-string"]
    assert "(Line String, Point, Line String, Polygon)" in output_string


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
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["layers"][0]["features"][0]["properties"]["version"].startswith("3.")


def test_gdalalg_vector_info_sql_where_mutually_exclusive():

    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Run(
            "vector",
            "info",
            dataset="data/path.shp",
            sql="select * from path",
            where="1=1",
        )


def test_gdalalg_vector_info_in_pipeline():

    with gdal.alg.vector.pipeline(pipeline="read data/path.shp ! info") as alg:
        j = alg.Output()
        assert j["layers"][0]["name"] == "path"
        assert "features" not in j["layers"][0]
