#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  validate_geoparquet testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal, ogr

CURRENT_VERSION = "1.1.0"
GEOPARQUET_1_1_0_JSON_SCHEMA = "../ogr/data/parquet/schema_1_1_0.json"


pytestmark = [
    pytest.mark.require_driver("Parquet"),
    pytest.mark.skipif(
        test_py_scripts.get_py_script("validate_geoparquet") is None,
        reason="validate_geoparquet.py not available",
    ),
    pytest.mark.skipif(
        pytest.importorskip("jsonschema") is None, reason="jsonschema module missing"
    ),
]


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("validate_geoparquet")


###############################################################################
# Validate a GeoParquet file


def _validate(filename, check_data=False, local_schema=GEOPARQUET_1_1_0_JSON_SCHEMA):
    import sys

    from test_py_scripts import samples_path

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    import validate_geoparquet

    return validate_geoparquet.check(
        filename, check_data=check_data, local_schema=local_schema
    )


###############################################################################


def test_validate_geoparquet_not_parquet_file(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "validate_geoparquet",
        "../ogr/data/poly.shp",
    )
    assert "ERROR ret code = 1" in ret
    assert "is not a Parquet file" in ret


###############################################################################


def test_validate_geoparquet_cannot_download_schema(script_path):

    # The file uses GeoParquet 0.1.0 which has no JSON schema
    ret = _validate("../ogr/data/parquet/test.parquet", local_schema=None)
    assert ret and "Cannot download GeoParquet JSON schema" in str(ret)


###############################################################################


def test_validate_geoparquet_ok(script_path, tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
    ds.CreateLayer("test")
    ds = None

    ret = test_py_scripts.run_py_script(
        script_path,
        "validate_geoparquet",
        test_dir,
    )
    assert ret == ""


###############################################################################


def test_validate_geoparquet_invalid_json(tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", "wrong"):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    with gdal.quiet_errors():
        ret = _validate(test_dir)
    assert ret and """'geo' metadata item is not valid JSON""" in str(ret)


###############################################################################


def test_validate_geoparquet_does_not_validate_schema(tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    j = {}
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    ret = _validate(test_dir)
    assert ret and """'geo' metadata item lacks a 'version' member""" in str(ret)
    assert ret and """'geo' metadata item fails to validate its schema""" in str(ret)
    assert ret and """geo["primary_column"] missing""" in str(ret)
    assert ret and len(ret) == 3


###############################################################################


def test_validate_geoparquet_primary_column_not_in_columns(tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "invalid",
        "columns": {"geometry": {"encoding": "WKB", "geometry_types": []}},
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    ret = _validate(test_dir)
    assert ret and """invalid is not in listed in geo["columns"']""" in str(ret)


###############################################################################


def test_validate_geoparquet_minimum_valid_metadata(tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "geometry",
        "columns": {"geometry": {"encoding": "WKB", "geometry_types": []}},
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    ret = _validate(test_dir)
    assert not ret


###############################################################################


def test_validate_geoparquet_column_name_not_found(tmp_path):

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "invalid",
        "columns": {"invalid": {"encoding": "WKB", "geometry_types": []}},
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    ret = _validate(test_dir)
    assert ret and """column which is not found in the Parquet fields""" in str(ret)


###############################################################################


@pytest.mark.parametrize(
    "bbox,error_msg",
    [
        ([-180, -90, 180, 90], None),
        ([-180, -90, 0, 180, 90, 10], None),
        ([-200, -90, 180, 90], "abs(bbox[0]) > 180"),
        ([-180, -100, 180, 90], "abs(bbox[1]) > 90"),
        ([-180, -90, 200, 90], "abs(bbox[2]) > 180"),
        ([-180, -90, 180, 100], "abs(bbox[3]) > 90"),
        ([-180, 90, 180, -90], "bbox[3] < bbox[1]"),
        ([-180, -90, 10, 180, 90, 0], "bbox[5] < bbox[2]"),
    ],
)
def test_validate_geoparquet_invalid_bbox(tmp_path, bbox, error_msg):

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "geometry",
        "columns": {
            "geometry": {"encoding": "WKB", "bbox": bbox, "geometry_types": []}
        },
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        ds.CreateLayer("test")
        ds = None

    ret = _validate(test_dir)
    if error_msg:
        assert ret and error_msg in str(ret)
    else:
        assert not ret


###############################################################################


def test_validate_geoparquet_invalid_wkb(tmp_path):

    pytest.importorskip("numpy")

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "geometry",
        "columns": {"geometry": {"encoding": "WKB", "geometry_types": []}},
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("geometry", ogr.OFTBinary))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFieldBinaryFromHexString("geometry", "01")
        lyr.CreateFeature(f)
        ds = None

    ret = _validate(test_dir, check_data=True)
    assert ret and """Invalid WKB geometry at row 0""" in str(ret)


###############################################################################


def test_validate_geoparquet_geom_type_not_consistent_with_declaration(tmp_path):

    pytest.importorskip("numpy")

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "geometry",
        "columns": {"geometry": {"encoding": "WKB", "geometry_types": ["Point"]}},
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        lyr = ds.CreateLayer("test")
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.Geometry(ogr.wkbPolygon))
        lyr.CreateFeature(f)
        ds = None

    ret = _validate(test_dir, check_data=True)
    assert (
        ret
        and """Geometry at row 0 is of type Polygon, but not listed in geometry_types[]"""
        in str(ret)
    )


###############################################################################


def test_validate_geoparquet_invalid_winding_order(tmp_path):

    pytest.importorskip("numpy")

    test_dir = str(tmp_path / "tmp.parquet")

    j = {
        "version": CURRENT_VERSION,
        "primary_column": "geometry",
        "columns": {
            "geometry": {
                "encoding": "WKB",
                "orientation": "counterclockwise",
                "geometry_types": ["Polygon", "MultiPolygon"],
            }
        },
    }
    with gdaltest.config_option("OGR_PARQUET_GEO_METADATA", json.dumps(j)):
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(test_dir)
        lyr = ds.CreateLayer("test", options=["POLYGON_ORIENTATION=UNMODIFIED"])
        f = ogr.Feature(lyr.GetLayerDefn())
        g = ogr.CreateGeometryFromWkt(
            "POLYGON((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.2,0.8 0.8,0.2 0.2))"
        )
        assert g.GetGeometryRef(0).IsClockwise()
        assert not g.GetGeometryRef(1).IsClockwise()
        f.SetGeometry(g)
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        g = ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.2,0.8 0.8,0.2 0.2)))"
        )
        f.SetGeometry(g)
        lyr.CreateFeature(f)
        ds = None

    ret = _validate(test_dir, check_data=True)
    assert (
        ret
        and """Exterior ring of geometry at row 0 has invalid orientation""" in str(ret)
    )
    assert (
        ret
        and """Interior ring of geometry at row 0 has invalid orientation""" in str(ret)
    )
    assert (
        ret
        and """Exterior ring of geometry at row 1 has invalid orientation""" in str(ret)
    )
    assert (
        ret
        and """Interior ring of geometry at row 1 has invalid orientation""" in str(ret)
    )
