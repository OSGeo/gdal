#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for OGR Parquet driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import math

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("Parquet")

GEOPARQUET_1_1_0_JSON_SCHEMA = "data/parquet/schema_1_1_0.json"


###############################################################################
# Validate a GeoParquet file


def _has_validate():
    import sys

    from test_py_scripts import samples_path

    try:
        import jsonschema

        jsonschema.validate
    except ImportError:
        print("jsonschema module not available")
        return False

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_geoparquet

        validate_geoparquet.check
    except ImportError:
        print("Cannot import validate_geoparquet")
        return False
    return True


def _validate(filename, check_data=False):
    if not _has_validate():
        return

    import validate_geoparquet

    ret = validate_geoparquet.check(
        filename, check_data=check_data, local_schema=GEOPARQUET_1_1_0_JSON_SCHEMA
    )
    assert not ret


###############################################################################


def _has_arrow_dataset():
    drv = gdal.GetDriverByName("Parquet")
    return drv is not None and drv.GetMetadataItem("ARROW_DATASET") is not None


###############################################################################


def _get_do_not_use_parquet_geo_types():
    options = []
    if "USE_PARQUET_GEO_TYPES" in gdal.GetDriverByName("PARQUET").GetMetadataItem(
        "DS_LAYER_CREATIONOPTIONLIST"
    ):
        options.append("USE_PARQUET_GEO_TYPES=NO")
    return options


###############################################################################


def _get_use_parquet_geo_types():
    options = []
    if "USE_PARQUET_GEO_TYPES" in gdal.GetDriverByName("PARQUET").GetMetadataItem(
        "DS_LAYER_CREATIONOPTIONLIST"
    ):
        options.append("USE_PARQUET_GEO_TYPES=YES")
    return options


###############################################################################


@pytest.fixture(scope="module", params=[True, False], ids=["arrow-dataset", "regular"])
def with_arrow_dataset_or_not(request):

    if request.param and not _has_arrow_dataset():
        pytest.skip("Test requires build with ArrowDataset")

    yield request.param


###############################################################################
# Read invalid file


def test_ogr_parquet_invalid():

    with pytest.raises(Exception):
        ogr.Open("data/parquet/invalid.parquet")


###############################################################################
# Basic tests


def _check_test_parquet(
    filename,
    expect_layer_geom_type=True,
    expect_fast_feature_count=True,
    expect_fast_get_extent=True,
    expect_ignore_fields=True,
    expect_domain=True,
    fid_reliable_after_spatial_filtering=True,
):
    with gdaltest.config_option("OGR_PARQUET_BATCH_SIZE", "2"):
        ds = gdal.OpenEx(filename)
    assert ds is not None, "cannot open dataset"
    assert ds.TestCapability("foo") == 0
    assert ds.GetLayerCount() == 1, "bad layer count"
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 1
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == "geometry"
    srs = lyr_defn.GetGeomFieldDefn(0).GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"
    if expect_layer_geom_type:
        assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    # import pprint
    # pprint.pprint(got_field_defns)
    expected_field_defns = [
        ("boolean", "Integer", "Boolean", 0, 0),
        ("uint8", "Integer", "None", 0, 0),
        ("int8", "Integer", "None", 0, 0),
        ("uint16", "Integer", "None", 0, 0),
        ("int16", "Integer", "Int16", 0, 0),
        ("uint32", "Integer64", "None", 0, 0),
        ("int32", "Integer", "None", 0, 0),
        ("uint64", "Real", "None", 0, 0),
        ("int64", "Integer64", "None", 0, 0),
        ("float32", "Real", "Float32", 0, 0),
        ("float64", "Real", "None", 0, 0),
        ("string", "String", "None", 0, 0),
        ("large_string", "String", "None", 0, 0),
        ("timestamp_ms_gmt", "DateTime", "None", 0, 0),
        ("timestamp_ms_gmt_plus_2", "DateTime", "None", 0, 0),
        ("timestamp_ms_gmt_minus_0215", "DateTime", "None", 0, 0),
        ("timestamp_s_no_tz", "DateTime", "None", 0, 0),
        ("timestamp_us_no_tz", "DateTime", "None", 0, 0),
        ("timestamp_ns_no_tz", "DateTime", "None", 0, 0),
        ("time32_s", "Time", "None", 0, 0),
        ("time32_ms", "Time", "None", 0, 0),
        ("time64_us", "Integer64", "None", 0, 0),
        ("time64_ns", "Integer64", "None", 0, 0),
        ("date32", "Date", "None", 0, 0),
        ("date64", "Date", "None", 0, 0),
        ("binary", "Binary", "None", 0, 0),
        ("large_binary", "Binary", "None", 0, 0),
        ("fixed_size_binary", "Binary", "None", 2, 0),
        ("decimal128", "Real", "None", 7, 3),
        ("decimal256", "Real", "None", 7, 3),
        ("list_boolean", "IntegerList", "Boolean", 0, 0),
        ("list_uint8", "IntegerList", "None", 0, 0),
        ("list_int8", "IntegerList", "None", 0, 0),
        ("list_uint16", "IntegerList", "None", 0, 0),
        ("list_int16", "IntegerList", "None", 0, 0),
        ("list_uint32", "Integer64List", "None", 0, 0),
        ("list_int32", "IntegerList", "None", 0, 0),
        ("list_uint64", "RealList", "None", 0, 0),
        ("list_int64", "Integer64List", "None", 0, 0),
        ("list_float32", "RealList", "Float32", 0, 0),
        ("list_float64", "RealList", "None", 0, 0),
        ("list_decimal128", "RealList", "None", 0, 0),
        ("list_decimal256", "RealList", "None", 0, 0),
        ("list_string", "StringList", "None", 0, 0),
        ("list_large_string", "StringList", "None", 0, 0),
        ("fixed_size_list_boolean", "IntegerList", "Boolean", 0, 0),
        ("fixed_size_list_uint8", "IntegerList", "None", 0, 0),
        ("fixed_size_list_int8", "IntegerList", "None", 0, 0),
        ("fixed_size_list_uint16", "IntegerList", "None", 0, 0),
        ("fixed_size_list_int16", "IntegerList", "None", 0, 0),
        ("fixed_size_list_uint32", "Integer64List", "None", 0, 0),
        ("fixed_size_list_int32", "IntegerList", "None", 0, 0),
        ("fixed_size_list_uint64", "RealList", "None", 0, 0),
        ("fixed_size_list_int64", "Integer64List", "None", 0, 0),
        ("fixed_size_list_float32", "RealList", "Float32", 0, 0),
        ("fixed_size_list_float64", "RealList", "None", 0, 0),
        ("fixed_size_list_string", "StringList", "None", 0, 0),
        ("struct_field.a", "Integer64", "None", 0, 0),
        ("struct_field.b", "Real", "None", 0, 0),
        ("struct_field.c.d", "String", "None", 0, 0),
        ("struct_field.c.f", "String", "None", 0, 0),
        ("struct_field.h", "Integer64List", "None", 0, 0),
        ("struct_field.i", "Integer64", "None", 0, 0),
        ("list_struct", "String", "JSON", 0, 0),
        ("map_boolean", "String", "JSON", 0, 0),
        ("map_uint8", "String", "JSON", 0, 0),
        ("map_int8", "String", "JSON", 0, 0),
        ("map_uint16", "String", "JSON", 0, 0),
        ("map_int16", "String", "JSON", 0, 0),
        ("map_uint32", "String", "JSON", 0, 0),
        ("map_int32", "String", "JSON", 0, 0),
        ("map_uint64", "String", "JSON", 0, 0),
        ("map_int64", "String", "JSON", 0, 0),
        ("map_float32", "String", "JSON", 0, 0),
        ("map_float64", "String", "JSON", 0, 0),
        ("map_decimal128", "String", "JSON", 0, 0),
        ("map_decimal256", "String", "JSON", 0, 0),
        ("map_string", "String", "JSON", 0, 0),
        ("map_large_string", "String", "JSON", 0, 0),
        ("map_list_string", "String", "JSON", 0, 0),
        ("map_large_list_string", "String", "JSON", 0, 0),
        ("map_fixed_size_list_string", "String", "JSON", 0, 0),
        ("dict", "Integer", "None", 0, 0),
    ]
    if not filename.endswith(".parquet"):
        expected_field_defns += [
            ("float16", "Real", "Float32", 0, 0),
            ("list_float16", "RealList", "Float32", 0, 0),
            ("list_list_float16", "String", "JSON", 0, 0),
            ("map_float16", "String", "JSON", 0, 0),
        ]
    assert lyr_defn.GetFieldCount() == len(expected_field_defns)
    got_field_defns = [
        (
            lyr_defn.GetFieldDefn(i).GetName(),
            ogr.GetFieldTypeName(lyr_defn.GetFieldDefn(i).GetType()),
            ogr.GetFieldSubTypeName(lyr_defn.GetFieldDefn(i).GetSubType()),
            lyr_defn.GetFieldDefn(i).GetWidth(),
            lyr_defn.GetFieldDefn(i).GetPrecision(),
        )
        for i in range(lyr_defn.GetFieldCount())
    ]
    assert got_field_defns == expected_field_defns
    if expect_fast_feature_count:
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1
    if expect_fast_get_extent:
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
    if expect_ignore_fields:
        assert lyr.TestCapability(ogr.OLCIgnoreFields) == 1
    assert lyr.GetFeatureCount() == 5
    assert lyr.GetExtent() == (0.0, 4.0, 2.0, 2.0)
    assert lyr.GetExtent(geom_field=0) == (0.0, 4.0, 2.0, 2.0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D) == 0
    assert lyr.GetExtent3D() == (0.0, 4.0, 2.0, 2.0, float("inf"), float("-inf"))
    with pytest.raises(Exception):
        lyr.GetExtent(geom_field=-1)
    with pytest.raises(Exception):
        lyr.GetExtent(geom_field=1)

    if expect_domain:
        assert ds.GetFieldDomainNames() == ["dictDomain"]
        assert ds.GetFieldDomain("not_existing") is None
        for _ in range(2):
            domain = ds.GetFieldDomain("dictDomain")
            assert domain is not None
            assert domain.GetName() == "dictDomain"
            assert domain.GetDescription() == ""
            assert domain.GetDomainType() == ogr.OFDT_CODED
            assert domain.GetFieldType() == ogr.OFTInteger
            assert domain.GetFieldSubType() == ogr.OFSTNone
            assert domain.GetEnumeration() == {"0": "foo", "1": "bar", "2": "baz"}

    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    assert f["boolean"]
    assert f["uint8"] == 1
    assert f["int8"] == -2
    assert f["uint16"] == 1
    assert f["int16"] == -20000
    assert f["uint32"] == 1
    assert f["int32"] == -2000000000
    assert f["uint64"] == 1
    assert f["int64"] == -200000000000
    assert f["float32"] == 1.5
    assert f["float64"] == 1.5
    assert f["string"] == "abcd"
    assert f["large_string"] == "abcd"
    assert f["timestamp_ms_gmt"] == "2019/01/01 14:00:00.500+00"
    assert f["timestamp_ms_gmt_plus_2"] == "2019/01/01 14:00:00.500+02"
    assert f["timestamp_ms_gmt_minus_0215"] == "2019/01/01 14:00:00.500-0215"
    assert f["timestamp_s_no_tz"] == "2019/01/01 14:00:00"
    assert f["timestamp_us_no_tz"] == "2019/01/01 14:00:00.001"
    assert f["timestamp_ns_no_tz"] == "2019/01/01 14:00:00"
    assert f["time32_s"] == "01:02:03"
    assert f["time32_ms"] == "01:02:03.456"
    assert f["time64_us"] == 3723000000
    assert f["time64_ns"] == 3723000000456
    assert f["date32"] == "1970/01/02"
    assert f["date64"] == "1970/01/02"
    assert f["binary"] == "0001"
    assert f["large_binary"] == "0001"
    assert f["fixed_size_binary"] == "0001"
    assert f["decimal128"] == 1234.567
    assert f["decimal256"] == 1234.567
    assert f["list_boolean"] == []
    assert f["list_uint8"] == []
    assert f["list_int8"] == []
    assert f["list_uint16"] == []
    assert f["list_int16"] == []
    assert f["list_uint32"] == []
    assert f["list_int32"] == []
    assert f["list_uint64"] == []
    assert f["list_int64"] == []
    assert f["list_float32"] == []
    assert f["list_float64"] == []
    assert f["list_decimal128"] == [1234.567]
    assert f["list_decimal256"] == [1234.567]
    assert f["list_string"] is None
    assert f["list_large_string"] is None
    assert f["fixed_size_list_boolean"] == [1, 0]
    assert f["fixed_size_list_uint8"] == [0, 1]
    assert f["fixed_size_list_int8"] == [0, 1]
    assert f["fixed_size_list_uint16"] == [0, 1]
    assert f["fixed_size_list_int16"] == [0, 1]
    assert f["fixed_size_list_uint32"] == [0, 1]
    assert f["fixed_size_list_int32"] == [0, 1]
    assert f["fixed_size_list_uint64"] == [0, 1]
    assert f["fixed_size_list_int64"] == [0, 1]
    assert f["fixed_size_list_float32"][0] == 0
    assert math.isnan(f["fixed_size_list_float32"][1])
    assert f["fixed_size_list_float64"][0] == 0
    assert math.isnan(f["fixed_size_list_float64"][1])
    assert f["fixed_size_list_string"] == ["a", "b"]
    assert f["struct_field.a"] == 1
    assert f["struct_field.b"] == 2.5
    assert f["struct_field.c.d"] == "e"
    assert f["struct_field.c.f"] == "g"
    assert f["struct_field.h"] == [5, 6]
    assert f["struct_field.i"] == 3
    assert f["list_struct"] == """[{"a":1,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]"""
    assert f["map_boolean"] == '{"x":null,"y":true}'
    assert f["map_uint8"] == '{"x":1,"y":null}'
    assert f["map_int8"] == '{"x":1,"y":null}'
    assert f["map_uint16"] == '{"x":1,"y":null}'
    assert f["map_int16"] == '{"x":1,"y":null}'
    assert f["map_uint32"] == '{"x":4000000000,"y":null}'
    assert f["map_int32"] == '{"x":2000000000,"y":null}'
    assert f["map_uint64"] == '{"x":4000000000000,"y":null}'
    assert f["map_int64"] == '{"x":-2000000000000,"y":null}'
    assert f["map_float32"] == '{"x":1.5,"y":null}'
    assert f["map_float64"] == '{"x":1.5,"y":null}'
    assert f["map_decimal128"] == '{"x":1234.567,"y":null}'
    assert f["map_decimal256"] == '{"x":1234.567,"y":null}'
    assert f["map_string"] == '{"x":"x_val","y":null}'
    assert f["map_large_string"] == '{"x":"x_val","y":null}'
    assert f["map_list_string"] == '{"x":["x_val"],"y":null}'
    assert f["map_large_list_string"] == '{"x":["x_val"],"y":null}'
    assert f["map_fixed_size_list_string"] == '{"x":["x_val",null],"y":[null,null]}'
    assert f["dict"] == 0
    if not filename.endswith(".parquet"):
        assert f["float16"] == 1.5
        assert f["list_float16"] == []
        assert f["list_list_float16"] == "[]"
        assert f["map_float16"] == '{"x":1.5,"y":null}'
    assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert not f["boolean"]
    assert f["uint8"] == 2
    if not filename.endswith(".parquet"):
        assert f["float16"] == 2.5
        assert str(f["list_float16"]) == str([float("nan")])
        assert f["list_list_float16"] == "[null]"
    assert f.GetGeometryRef() is None

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    assert f["uint8"] is None
    if not filename.endswith(".parquet"):
        assert f["float16"] is None
        assert f["list_float16"] is None
        assert f["list_list_float16"] is None
        assert f["map_float16"] is None
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 3
    assert f["uint8"] == 4
    assert str(f["list_decimal128"]) == str([float("nan")])
    if not filename.endswith(".parquet"):
        assert f["float16"] == 4.5
        assert str(f["list_float16"]) == str([float("nan"), 4.5, 5.5])
        assert f["list_list_float16"] == "[null,[4.5],[5.5]]"
        assert f["map_float16"] == "{}"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 4
    assert f["uint8"] == 5
    assert f.GetGeometryRef().ExportToWkt() == "POINT (4 2)"

    assert lyr.GetNextFeature() is None

    assert lyr.GetNextFeature() is None

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0

    lyr.SetSpatialFilterRect(4, 2, 4, 2)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if fid_reliable_after_spatial_filtering:
        assert f.GetFID() == 4
    assert f.GetGeometryRef().ExportToWkt() == "POINT (4 2)"

    lyr.SetSpatialFilterRect(-100, -100, -100, -100)
    lyr.ResetReading()
    assert lyr.GetNextFeature() is None

    lyr.SetSpatialFilter(None)

    if expect_ignore_fields:
        # Ignore just one member of a structure
        assert lyr.SetIgnoredFields(["struct_field.a"]) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f["fixed_size_list_string"] == ["a", "b"]
        assert f["struct_field.a"] is None
        assert f["struct_field.b"] == 2.5
        assert f["map_boolean"] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"

        # Ignore all members of a structure
        assert (
            lyr.SetIgnoredFields(
                [
                    "struct_field.a",
                    "struct_field.b",
                    "struct_field.c.d",
                    "struct_field.c.f",
                    "struct_field.h",
                    "struct_field.i",
                ]
            )
            == ogr.OGRERR_NONE
        )
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f["fixed_size_list_string"] == ["a", "b"]
        assert f["struct_field.a"] is None
        assert f["struct_field.b"] is None
        assert f["struct_field.c.d"] is None
        assert f["struct_field.c.f"] is None
        assert f["struct_field.h"] is None
        assert f["struct_field.i"] is None
        assert f["map_boolean"] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"

        # Ignore a map
        assert lyr.SetIgnoredFields(["map_boolean"]) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f["fixed_size_list_string"] == ["a", "b"]
        assert f["struct_field.a"] == 1
        assert f["struct_field.b"] == 2.5
        assert f["map_boolean"] is None
        assert f["map_uint8"] == '{"x":1,"y":null}'
        assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"

        # Ignore geometry
        assert lyr.SetIgnoredFields(["geometry"]) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f["fixed_size_list_string"] == ["a", "b"]
        assert f["struct_field.a"] == 1
        assert f["struct_field.b"] == 2.5
        assert f["map_boolean"] == '{"x":null,"y":true}'
        assert f.GetGeometryRef() is None

        # Cancel ignored fields
        assert lyr.SetIgnoredFields([]) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f["fixed_size_list_string"] == ["a", "b"]
        assert f["struct_field.a"] == 1
        assert f["struct_field.b"] == 2.5
        assert f["map_boolean"] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"


@pytest.mark.parametrize("use_vsi", [False, True])
def test_ogr_parquet_1(use_vsi):

    filename = "data/parquet/test.parquet"
    if use_vsi:
        vsifilename = "/vsimem/test.parquet"
        gdal.FileFromMemBuffer(vsifilename, open(filename, "rb").read())
        filename = vsifilename

    try:
        _check_test_parquet(filename)
    finally:
        if use_vsi:
            gdal.Unlink(vsifilename)


###############################################################################


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
@pytest.mark.parametrize("use_vsi", [False, True])
def test_ogr_parquet_check_dataset(use_vsi):

    filename = "data/parquet/test.parquet"
    if use_vsi:
        vsifilename = "/vsimem/test.parquet"
        gdal.FileFromMemBuffer(vsifilename, open(filename, "rb").read())
        filename = vsifilename

    try:
        _check_test_parquet(
            "PARQUET:" + filename,
            expect_layer_geom_type=False,
            expect_fast_feature_count=False,
            expect_fast_get_extent=False,
            expect_domain=False,
            fid_reliable_after_spatial_filtering=False,
        )
    finally:
        if use_vsi:
            gdal.Unlink(vsifilename)


###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_test():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/parquet/test.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_example():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/parquet/example.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_all_geoms():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/parquet/all_geoms.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_test_ogrsf_all_geoms_with_arrow_dataset():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro PARQUET:data/parquet/all_geoms.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test write support


@pytest.mark.parametrize(
    "use_vsi,row_group_size,fid,use_parquet_geo_types",
    [(False, None, None, True), (False, None, None, "ONLY"), (True, 2, "fid", False)],
)
def test_ogr_parquet_write_from_another_dataset(
    use_vsi, row_group_size, fid, use_parquet_geo_types
):

    outfilename = "/vsimem/out.parquet" if use_vsi else "tmp/out.parquet"
    layerCreationOptions = []
    if row_group_size:
        layerCreationOptions.append("ROW_GROUP_SIZE=" + str(row_group_size))
    if fid:
        layerCreationOptions.append("FID=" + fid)
    if use_parquet_geo_types == "ONLY":
        if "USE_PARQUET_GEO_TYPES" in gdal.GetDriverByName("PARQUET").GetMetadataItem(
            "DS_LAYER_CREATIONOPTIONLIST"
        ):
            layerCreationOptions.append("USE_PARQUET_GEO_TYPES=ONLY")
        else:
            pytest.skip("Skipping use_parquet_geo_types=ONLY due to libarrow < 21")
    elif use_parquet_geo_types:
        layerCreationOptions += _get_use_parquet_geo_types()
    elif not use_parquet_geo_types:
        layerCreationOptions += _get_do_not_use_parquet_geo_types()
    try:
        gdal.VectorTranslate(
            outfilename,
            "data/parquet/test.parquet",
            layerCreationOptions=layerCreationOptions,
        )

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.GetDataset().GetDescription() == ds.GetDescription()

        assert lyr.GetFIDColumn() == (fid if fid else "")

        if fid:
            f = lyr.GetFeature(4)
            assert f is not None
            assert f.GetFID() == 4

            assert lyr.GetFeature(5) is None

            lyr.SetIgnoredFields([lyr.GetLayerDefn().GetFieldDefn(0).GetName()])

            f = lyr.GetFeature(4)
            assert f is not None
            assert f.GetFID() == 4

            assert lyr.GetFeature(5) is None

            lyr.SetIgnoredFields([])

        if row_group_size:
            num_features = lyr.GetFeatureCount()
            expected_num_row_groups = int(math.ceil(num_features / row_group_size))
            assert lyr.GetMetadataItem("NUM_ROW_GROUPS", "_PARQUET_") == str(
                expected_num_row_groups
            )
            for i in range(expected_num_row_groups):
                got_num_rows = lyr.GetMetadataItem(
                    "ROW_GROUPS[%d].NUM_ROWS" % i, "_PARQUET_"
                )
                if i < expected_num_row_groups - 1:
                    assert got_num_rows == str(row_group_size)
                else:
                    assert got_num_rows == str(
                        num_features - (expected_num_row_groups - 1) * row_group_size
                    )

        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        if use_parquet_geo_types == "ONLY":
            assert geo is None
        else:
            assert geo is not None
            j = json.loads(geo)
            assert j is not None
            assert "version" in j
            assert j["version"] == "1.1.0"
            assert "primary_column" in j
            assert j["primary_column"] == "geometry"
            assert "columns" in j
            assert "geometry" in j["columns"]
            assert "encoding" in j["columns"]["geometry"]
            assert j["columns"]["geometry"]["encoding"] == "WKB"
            assert j["columns"]["geometry"]["covering"] == {
                "bbox": {
                    "xmax": ["geometry_bbox", "xmax"],
                    "xmin": ["geometry_bbox", "xmin"],
                    "ymax": ["geometry_bbox", "ymax"],
                    "ymin": ["geometry_bbox", "ymin"],
                }
            }

            md = lyr.GetMetadata("_PARQUET_METADATA_")
            assert "geo" in md

        ds = None

        _check_test_parquet(outfilename)

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test write support


def test_ogr_parquet_write_edge_cases():

    outfilename = "/vsimem/out.parquet"

    # No layer
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    assert ds.GetLayerCount() == 0
    assert ds.GetLayer(0) is None
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 1
    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 0
    domain = ogr.CreateCodedFieldDomain(
        "name", "desc", ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None}
    )
    with pytest.raises(Exception, match="Layer must be created"):
        ds.AddFieldDomain(domain)
    assert ds.GetFieldDomainNames() is None
    assert ds.GetFieldDomain("foo") is None
    ds = None
    gdal.Unlink(outfilename)

    # No field, no record
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    with pytest.raises(Exception):
        ds.CreateLayer("out", srs=srs, geom_type=ogr.wkbPointM)
    with pytest.raises(Exception):
        ds.CreateLayer(
            "out", srs=srs, geom_type=ogr.wkbPoint, options=["COMPRESSION=invalid"]
        )
    lyr = ds.CreateLayer("out", srs=srs, geom_type=ogr.wkbPoint)
    assert lyr is not None
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0) is not None
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 0
    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 1
    # Test creating a second layer
    with pytest.raises(Exception):
        ds.CreateLayer("out2", srs=srs, geom_type=ogr.wkbPoint)
    ds = None
    ds = gdal.OpenEx(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetNextFeature() is None
    lyr = None
    ds = None
    gdal.Unlink(outfilename)

    # No geometry field, one record
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    lyr = ds.CreateLayer("out", geom_type=ogr.wkbNone)
    assert lyr.TestCapability(ogr.OLCCreateField) == 1
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 1
    assert lyr.TestCapability(ogr.OLCSequentialWrite) == 1
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetNullable(False)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    with pytest.raises(Exception):
        # violation of not-null constraint
        lyr.CreateFeature(f)
    f["foo"] = "bar"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    assert lyr.TestCapability(ogr.OLCCreateField) == 0
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 0
    with pytest.raises(Exception):
        lyr.CreateField(ogr.FieldDefn("bar"))
    with pytest.raises(Exception):
        lyr.CreateGeomField(ogr.GeomFieldDefn("baz", ogr.wkbPoint))
    ds = None
    ds = gdal.OpenEx(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetNextFeature() is not None
    lyr = None
    ds = None
    gdal.Unlink(outfilename)


###############################################################################
# Test compression support


@pytest.mark.parametrize("compression", ["uncompressed", "snappy", "zstd"])
def test_ogr_parquet_write_compression(compression):

    lco = gdal.GetDriverByName("Parquet").GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST")
    if compression.upper() not in lco:
        pytest.skip()

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    options = ["FID=fid", "COMPRESSION=" + compression]
    lyr = ds.CreateLayer("out", geom_type=ogr.wkbNone, options=options)
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    lyr = None
    ds = None

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert (
        lyr.GetMetadataItem("ROW_GROUPS[0].COLUMNS[0].COMPRESSION", "_PARQUET_")
        == compression
    )
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test compression level support


def test_ogr_parquet_compression_level(tmp_vsimem):

    lco = gdal.GetDriverByName("Parquet").GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST")
    if "ZSTD" not in lco:
        pytest.skip("ZSTD codec missing")

    gdal.VectorTranslate(
        tmp_vsimem / "out1.parquet",
        "data/poly.shp",
        layerCreationOptions=["COMPRESSION=ZSTD", "COMPRESSION_LEVEL=1"],
    )
    gdal.VectorTranslate(
        tmp_vsimem / "out22.parquet",
        "data/poly.shp",
        layerCreationOptions=["COMPRESSION=ZSTD", "COMPRESSION_LEVEL=22"],
    )

    assert (
        gdal.VSIStatL(tmp_vsimem / "out22.parquet").size
        < gdal.VSIStatL(tmp_vsimem / "out1.parquet").size
    )


###############################################################################
# Test coordinate epoch support


@pytest.mark.parametrize("epsg_code", [4326, 9057])  # "WGS 84 (G1762)"
def test_ogr_parquet_coordinate_epoch(epsg_code):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(epsg_code)
    srs.SetCoordinateEpoch(2022.3)
    ds.CreateLayer("out", geom_type=ogr.wkbPoint, srs=srs)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    if epsg_code == 4326:
        assert "crs" not in j["columns"]["geometry"]
    else:
        assert "crs" in j["columns"]["geometry"]
        assert j["columns"]["geometry"]["crs"]["type"] == "GeographicCRS"
    assert "epoch" in j["columns"]["geometry"]

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert int(srs.GetAuthorityCode(None)) == epsg_code
    assert srs.GetCoordinateEpoch() == 2022.3
    lyr = None
    ds = None

    _validate(outfilename)

    gdal.Unlink(outfilename)


###############################################################################
# Test missing CRS member (on reading, implicitly fallback to EPSG:4326)


def test_ogr_parquet_missing_crs_member():

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    with gdaltest.config_option("OGR_PARQUET_WRITE_CRS", "NO"):
        ds.CreateLayer("out", geom_type=ogr.wkbPoint)
        ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    assert "crs" not in j["columns"]["geometry"]

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test writing a CRS and automatically identifying it

crs_84_wkt1 = """GEOGCS["WGS 84 (CRS84)",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["OGC","CRS84"]]"""


@pytest.mark.parametrize(
    "input_definition,expected_crs",
    [
        ("+proj=longlat +datum=WGS84", "4326"),
        (crs_84_wkt1, "4326"),
        ("OGC:CRS84", "4326"),
        ("EPSG:4326", "4326"),
        ("EPSG:4269", "4269"),
        ("+proj=longlat +datum=NAD83", "4269"),
        ("EPSG:32631", "32631"),
        ("+proj=utm +zone=31 +datum=WGS84", "32631"),
        ("+proj=longlat +ellps=GRS80", None),
    ],
)
def test_ogr_parquet_crs_identification_on_write(input_definition, expected_crs):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.SetFromUserInput(input_definition)
    ds.CreateLayer("out", geom_type=ogr.wkbPoint, srs=srs)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    if expected_crs == "4326":
        assert "crs" not in j["columns"]["geometry"]
    else:
        assert "crs" in j["columns"]["geometry"]

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == expected_crs
    lyr = None
    ds = None

    _validate(outfilename)

    gdal.Unlink(outfilename)


###############################################################################
# Test EDGES option


@pytest.mark.parametrize("edges", [None, "PLANAR", "SPHERICAL"])
def test_ogr_parquet_edges(edges):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    options = []
    if edges is not None:
        options = ["EDGES=" + edges]
    ds.CreateLayer("out", geom_type=ogr.wkbPoint, options=options)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    if edges == "SPHERICAL":
        assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"
    else:
        assert lyr.GetMetadataItem("EDGES") is None

    ds = None

    _validate(outfilename)

    gdal.Unlink(outfilename)


###############################################################################
# Test geometry_types support


@pytest.mark.parametrize(
    "written_geom_type,written_wkt,expected_ogr_geom_type,expected_wkts,expected_geometry_types",
    [
        (ogr.wkbPoint, ["POINT (1 2)"], ogr.wkbPoint, None, ["Point"]),
        (
            ogr.wkbLineString,
            ["LINESTRING (1 2,3 4)"],
            ogr.wkbLineString,
            None,
            ["LineString"],
        ),
        (
            ogr.wkbPolygon,
            [
                "POLYGON ((0 0,1 0,1 1,0 1,0 0))",
                "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
            ],  # winding order must be fixed
            ogr.wkbPolygon,
            [
                "POLYGON ((0 0,1 0,1 1,0 1,0 0))",
                "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))",
            ],
            ["Polygon"],
        ),
        (
            ogr.wkbMultiPoint,
            ["MULTIPOINT ((1 2))"],
            ogr.wkbMultiPoint,
            None,
            ["MultiPoint"],
        ),
        (
            ogr.wkbMultiLineString,
            ["MULTILINESTRING ((1 2,3 4))"],
            ogr.wkbMultiLineString,
            None,
            ["MultiLineString"],
        ),
        (
            ogr.wkbMultiPolygon,
            ["MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))"],
            ogr.wkbMultiPolygon,
            None,
            ["MultiPolygon"],
        ),
        (
            ogr.wkbGeometryCollection,
            ["GEOMETRYCOLLECTION (POINT (1 2))"],
            ogr.wkbGeometryCollection,
            None,
            ["GeometryCollection"],
        ),
        (ogr.wkbPoint25D, ["POINT Z (1 2 3)"], ogr.wkbPoint25D, None, ["Point Z"]),
        (ogr.wkbUnknown, ["POINT (1 2)"], ogr.wkbPoint, None, ["Point"]),
        (ogr.wkbUnknown, ["POINT Z (1 2 3)"], ogr.wkbPoint25D, None, ["Point Z"]),
        (
            ogr.wkbUnknown,
            [
                "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
                "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))",
            ],
            ogr.wkbMultiPolygon,
            [
                "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))",
                "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))",
            ],
            ["Polygon", "MultiPolygon"],
        ),
        (
            ogr.wkbUnknown,
            ["LINESTRING (1 2,3 4)", "MULTILINESTRING ((10 2,3 4))"],
            ogr.wkbMultiLineString,
            ["MULTILINESTRING ((1 2,3 4))", "MULTILINESTRING ((10 2,3 4))"],
            ["LineString", "MultiLineString"],
        ),
        (
            ogr.wkbUnknown,
            ["LINESTRING Z (1 2 10,3 4 20)", "MULTILINESTRING Z ((1 2 10,3 4 20))"],
            ogr.wkbMultiLineString25D,
            [
                "MULTILINESTRING Z ((1 2 10,3 4 20))",
                "MULTILINESTRING Z ((1 2 10,3 4 20))",
            ],
            ["LineString Z", "MultiLineString Z"],
        ),
        (
            ogr.wkbUnknown,
            ["POINT (1 2)", "LINESTRING (1 2,3 4)"],
            ogr.wkbUnknown,
            None,
            ["Point", "LineString"],
        ),
        (
            ogr.wkbUnknown,
            ["LINESTRING Z (1 2 10,3 4 20)", "MULTILINESTRING ((1 2,3 4))"],
            ogr.wkbMultiLineString25D,
            [
                "MULTILINESTRING Z ((1 2 10,3 4 20))",
                "MULTILINESTRING Z ((1 2 0,3 4 0))",
            ],
            ["LineString Z", "MultiLineString"],
        ),
    ],
)
def test_ogr_parquet_geometry_types(
    written_geom_type,
    written_wkt,
    expected_ogr_geom_type,
    expected_wkts,
    expected_geometry_types,
):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("out", geom_type=written_geom_type)
    for wkt in written_wkt:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == expected_ogr_geom_type
    assert lyr.GetSpatialRef() is None
    if expected_wkts is None:
        expected_wkts = written_wkt
    for wkt in expected_wkts:
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == wkt

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    assert "geometry_types" in j["columns"]["geometry"]
    assert set(j["columns"]["geometry"]["geometry_types"]) == set(
        expected_geometry_types
    )

    ds = None

    _validate(outfilename)

    gdal.Unlink(outfilename)


###############################################################################
# Test POLYGON_ORIENTATION option


@pytest.mark.parametrize(
    "option_value,written_wkt,expected_wkt",
    [
        (
            None,
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
            "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))",
        ),
        (
            "COUNTERCLOCKWISE",
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
            "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))",
        ),
        (
            "UNMODIFIED",
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
        ),
    ],
)
def test_ogr_parquet_polygon_orientation(option_value, written_wkt, expected_wkt):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    if option_value:
        options = ["POLYGON_ORIENTATION=" + option_value]
    else:
        options = []
    lyr = ds.CreateLayer("out", geom_type=ogr.wkbPolygon, options=options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(written_wkt))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == expected_wkt

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    if option_value != "UNMODIFIED":
        assert "orientation" in j["columns"]["geometry"]
        assert j["columns"]["geometry"]["orientation"] == "counterclockwise"
    else:
        assert "orientation" not in j["columns"]["geometry"]

    ds = None

    try:
        import numpy

        from osgeo import gdal_array

        str(gdal_array)

        numpy.zeros

        has_numpy = True
    except ImportError:
        has_numpy = False
    _validate(outfilename, check_data=has_numpy)

    gdal.Unlink(outfilename)


###############################################################################
# Test using statistics for SQL MIN, MAX, COUNT functions


def test_ogr_parquet_statistics():

    filename = "data/parquet/test.parquet"
    ds = ogr.Open(filename)
    expected_fields = [
        ("boolean", "Integer", "Boolean", 0, 1),
        ("uint8", "Integer", "None", 1, 5),
        ("int8", "Integer", "None", -2, 2),
        ("uint16", "Integer", "None", 1, 40001),
        ("int16", "Integer", "Int16", -20000, 20000),
        ("uint32", "Integer64", "None", 1, 4000000001),
        ("int32", "Integer", "None", -2000000000, 2000000000),
        ("uint64", "Integer64", "None", 1, 400000000001),
        ("int64", "Integer64", "None", -200000000000, 200000000000),
        ("float32", "Real", "Float32", 1.5, 5.5),
        ("float64", "Real", "None", 1.5, 5.5),
        ("string", "String", "None", "", "d"),
        (
            "timestamp_ms_gmt",
            "DateTime",
            "None",
            "2019/01/01 14:00:00.500+00",
            "2019/01/01 14:00:00.500+00",
        ),
    ]

    sql = ""
    for field in expected_fields:
        name = field[0]
        if sql:
            sql += ", "
        else:
            sql = "SELECT "
        sql += "MIN(" + name + "), MAX(" + name + ") AS my_max_" + name
    sql += ", COUNT(int32)"
    sql += " FROM test"
    sql_lyr = ds.ExecuteSQL(sql)
    try:
        f = sql_lyr.GetNextFeature()
        i = 0
        for name, type, subtype, minval, maxval in expected_fields:
            fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
            assert fld_defn.GetName() == "MIN_" + name
            assert ogr.GetFieldTypeName(fld_defn.GetType()) == type, name
            assert ogr.GetFieldSubTypeName(fld_defn.GetSubType()) == subtype, name
            assert f[fld_defn.GetName()] == minval, name
            i += 1

            fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
            assert fld_defn.GetName() == "my_max_" + name
            assert ogr.GetFieldTypeName(fld_defn.GetType()) == type, name
            assert ogr.GetFieldSubTypeName(fld_defn.GetSubType()) == subtype, name
            assert f[fld_defn.GetName()] == maxval, name
            i += 1

        fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
        assert fld_defn.GetName() == "COUNT_int32"
        assert f[fld_defn.GetName()] == 4
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Non-optimized due to WHERE condition
    sql_lyr = ds.ExecuteSQL("SELECT MIN(uint32) FROM test WHERE 1 = 0")
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) is None
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Non-optimized
    sql_lyr = ds.ExecuteSQL("SELECT AVG(uint8) FROM test")
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 3.0
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Should be optimized, but this Parquet file has a wrong value for
    # distinct_count
    sql_lyr = ds.ExecuteSQL("SELECT COUNT(DISTINCT int32) FROM test")
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 4
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Errors
    with pytest.raises(Exception):
        ds.ExecuteSQL("SELECT MIN(int32) FROM i_dont_exist")
    with pytest.raises(Exception):
        ds.ExecuteSQL("SELECT MIN(i_dont_exist) FROM test")

    # File without statistics
    outfilename = "/vsimem/out.parquet"
    try:
        gdal.VectorTranslate(
            outfilename, "data/parquet/test.parquet", options="-lco STATISTICS=NO"
        )
        ds = ogr.Open(outfilename)
        with ds.ExecuteSQL("SELECT MIN(string) FROM out") as sql_lyr:
            pass
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test MIN/MAX on a UINT32 field on a Parquet 2 file


def test_ogr_parquet_statistics_uint32_parquet2_file_format():

    ds = ogr.Open("data/parquet/uint32_parquet2.parquet")
    with ds.ExecuteSQL(
        "SELECT MIN(uint32), MAX(uint32) FROM uint32_parquet2"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_uint32"] == 1
        assert f["MAX_uint32"] == 4000000001


###############################################################################
# Test setting/getting creator


def test_ogr_parquet_creator():

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        ds.CreateLayer("test")
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadataItem("CREATOR", "_PARQUET_").startswith("GDAL ")
        ds = None

        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        ds.CreateLayer("test", options=["CREATOR=my_creator"])
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadataItem("CREATOR", "_PARQUET_") == "my_creator"
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test creating multiple geometry columns


def test_ogr_parquet_multiple_geom_columns():

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateGeomField(
            ogr.GeomFieldDefn("geom_point", ogr.wkbPoint)
        ) == ogr.OGRERR_NONE
        lyr.CreateGeomField(
            ogr.GeomFieldDefn("geom_line", ogr.wkbLineString)
        ) == ogr.OGRERR_NONE
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetGeomFieldCount() == 2
        assert lyr_defn.GetGeomFieldDefn(0).GetName() == "geom_point"
        assert lyr_defn.GetGeomFieldDefn(1).GetName() == "geom_line"
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test SetAttributeFilter()


@pytest.mark.parametrize(
    "filter",
    [
        "boolean = 0",
        "boolean = 1",
        "boolean = 1.5",
        # "boolean = '0'",
        "uint8 = 2",
        "uint8 = -1",
        "int8 = -1",
        "int8 = 0",
        "int8 != 0",
        "int8 < 0",
        "int8 > 0",
        "int8 <= 0",
        "int8 >= 0",
        "0 = int8",
        "0 != int8",
        "0 < int8",
        "0 > int8",
        "0 <= int8",
        "0 >= int8",
        "int8 IS NULL",
        "int8 IS NOT NULL",
        "uint16 = 10001",
        "uint32 = 1000000001",
        "uint32 != 1000000001",
        "uint32 < 1000000001",
        "uint32 > 1000000001",
        "uint32 <= 1000000001",
        "uint32 >= 1000000001",
        "int32 = -1000000000",
        "uint64 = 100000000001",
        "int64 = -100000000000",
        "float32 = 2.5",
        "float64 = 2.5",
        "float64 != 2.5",
        "float64 < 2.5",
        "float64 > 2.5",
        "float64 <= 2.5",
        "float64 >= 2.5",
        "string = ''",
        "string != ''",
        "string < 'l'",
        "string > 'l'",
        "string <= 'l'",
        "string >= 'l'",
        "decimal128 = -1234.567",
        "decimal256 = -1234.567",
        "uint8 = 5 AND int8 = 2",
        "uint8 = -5 AND int8 = 2",
        "int8 = 2 AND uint8 = -5",
        "uint8 = -5 AND int8 = 200",
        "NOT uint8 = 5 AND uint8 IS NOT NULL",
        "NOT uint8 = 50 AND uint8 IS NOT NULL",
        # not optimized for non-dataset layer
        "FID = 0",
        "boolean = 0 OR boolean = 1",
        "uint8 = 1 OR uint8 = -1",
        "uint8 = -1 OR uint8 = 1",
        "1 = 1",
        "boolean = boolean",
        "FID = 1",
        '"struct_field.a" = 1',
        '"struct_field.a" = 0',
        "string LIKE 'd'",
        "string LIKE 'D'",
        "string ILIKE 'D'",
        "string LIKE 'f'",
        "timestamp_ms_gmt = '2019/01/01 14:00:00.500Z'",
        "timestamp_ms_gmt < '2019/01/01 14:00:00.500Z'",
        "timestamp_s_no_tz = '2019/01/01 14:00:00'",
        "timestamp_s_no_tz < '2019/01/01 14:00:00'",
        # partially optimized
        "boolean = 0 AND OGR_GEOMETRY IS NOT NULL",
        "OGR_GEOMETRY IS NOT NULL AND boolean = 0",
        # not optimized
        "OGR_GEOMETRY IS NOT NULL AND OGR_GEOMETRY IS NOT NULL",
        "boolean = 0 OR OGR_GEOMETRY IS NOT NULL",
        "OGR_GEOMETRY IS NOT NULL OR boolean = 0",
    ],
)
def test_ogr_parquet_attribute_filter(filter, with_arrow_dataset_or_not):

    with gdaltest.config_option("OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "NO"):
        ds = ogr.Open("data/parquet/test.parquet")
        lyr = ds.GetLayer(0)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        ds = None

    prefix = "PARQUET:" if with_arrow_dataset_or_not else ""
    ds = ogr.Open(prefix + "data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


def test_ogr_parquet_attribute_filter_on_fid_column(with_arrow_dataset_or_not):

    filter = "fid = 1"

    prefix = "PARQUET:" if with_arrow_dataset_or_not else ""
    ds = ogr.Open(prefix + "data/parquet/test_with_fid_and_geometry_bbox.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert lyr.GetNextFeature() is None


def test_ogr_parquet_attribute_filter_and_then_ignored_fields():

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)

    assert lyr.SetAttributeFilter("string = 'd'") == ogr.OGRERR_NONE

    ignored_fields = []
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        name = lyr_defn.GetFieldDefn(i).GetName()
        if name != "string":
            ignored_fields.append(name)
    assert lyr.SetIgnoredFields(ignored_fields) == ogr.OGRERR_NONE

    assert lyr.GetFeatureCount() == 1

    with pytest.raises(
        Exception,
        match=r"Constraint on field string cannot be applied due to it being ignored",
    ):
        lyr.SetIgnoredFields(["string"])
    assert lyr.GetFeatureCount() == 0


def test_ogr_parquet_ignored_fields_and_then_attribute_filter():

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)

    ignored_fields = []
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        name = lyr_defn.GetFieldDefn(i).GetName()
        if name != "string":
            ignored_fields.append(name)
    assert lyr.SetIgnoredFields(ignored_fields) == ogr.OGRERR_NONE

    assert lyr.SetAttributeFilter("string = 'd'") == ogr.OGRERR_NONE

    assert lyr.GetFeatureCount() == 1


def test_ogr_parquet_attribute_filter_and_spatial_filter(with_arrow_dataset_or_not):

    filter = "int8 != 0"

    with gdaltest.config_option("OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "NO"):
        ds = ogr.Open("data/parquet/test.parquet")
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(4, 2, 4, 2)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        assert ref_fc > 0
        ds = None

    prefix = "PARQUET:" if with_arrow_dataset_or_not else ""
    ds = ogr.Open(prefix + "data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(4, 2, 4, 2)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


def test_ogr_parquet_attribute_filter_and_spatial_filter_with_spatial_index(
    tmp_path, with_arrow_dataset_or_not
):

    filename = str(tmp_path / "test.parquet")
    gdal.VectorTranslate(filename, "data/parquet/test.parquet")

    filter = "uint8 != 1"

    with gdaltest.config_option("OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "NO"):
        ds = ogr.Open("data/parquet/test.parquet")
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(1, 2, 3, 2)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        assert ref_fc > 0
        ds = None

    prefix = "PARQUET:" if with_arrow_dataset_or_not else ""
    ds = ogr.Open(prefix + filename)
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(1, 2, 3, 2)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


###############################################################################
# Test IS NULL / IS NOT NULL


def test_ogr_parquet_is_null():

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbNone, options=["ROW_GROUP_SIZE=1"]
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "bar"
        lyr.CreateFeature(f)
        ds = None
        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)
        with ogrtest.attribute_filter(lyr, "str IS NULL"):
            assert lyr.GetFeatureCount() == 1
        with ogrtest.attribute_filter(lyr, "str IS NOT NULL"):
            assert lyr.GetFeatureCount() == 2
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test reading a flat partitioned dataset


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
@pytest.mark.parametrize("use_vsi", [False, True])
@pytest.mark.parametrize("use_metadata_file", [False, True])
@pytest.mark.parametrize("prefix", ["", "PARQUET:"])
def test_ogr_parquet_read_partitioned_flat(use_vsi, use_metadata_file, prefix):

    opts = {
        "OGR_PARQUET_USE_VSI": "YES" if use_vsi else "NO",
        "OGR_PARQUET_USE_METADATA_FILE=NO": "YES" if use_metadata_file else "NO",
    }
    with gdaltest.config_options(opts):
        ds = ogr.Open(prefix + "data/parquet/partitioned_flat")
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 6
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        for _ in range(2):
            for i in range(6):
                f = lyr.GetNextFeature()
                assert f["one"] == i + 1
                assert f["two"] == -(i + 1)
            assert lyr.GetNextFeature() is None
            lyr.ResetReading()

        s = lyr.GetArrowStream()
        assert s.GetSchema().GetChildrenCount() == 2
        del s


###############################################################################
# Run test_ogrsf


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_test_ogrsf_dataset():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/parquet/partitioned_flat"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_test_ogrsf_dataset_on_file():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro PARQUET:data/parquet/test.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test reading a HIVE partitioned dataset


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
@pytest.mark.parametrize("use_vsi", [False, True])
@pytest.mark.parametrize("use_metadata_file", [False, True])
@pytest.mark.parametrize("prefix", ["", "PARQUET:"])
def test_ogr_parquet_read_partitioned_hive(use_vsi, use_metadata_file, prefix):

    opts = {
        "OGR_PARQUET_USE_VSI": "YES" if use_vsi else "NO",
        "OGR_PARQUET_USE_METADATA_FILE=NO": "YES" if use_metadata_file else "NO",
    }
    with gdaltest.config_options(opts):
        ds = ogr.Open(prefix + "data/parquet/partitioned_hive")
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 6
        assert lyr.GetLayerDefn().GetFieldCount() == 3
        for _ in range(2):
            for i in range(6):
                f = lyr.GetNextFeature()
                assert f["one"] == i + 1
                assert f["two"] == -(i + 1)
                assert f["foo"] == ("bar" if i < 3 else "baz")
            assert lyr.GetNextFeature() is None
            lyr.ResetReading()


def test_ogr_parquet_read_partitioned_hive_filter():

    with gdaltest.config_options({"OGR_PARQUET_USE_VSI": "YES"}):
        ds = ogr.Open("data/parquet/partitioned_hive")
        lyr = ds.GetLayer(0)

        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == ["data/parquet/partitioned_hive/_metadata"]

        lyr.SetAttributeFilter("foo = 'bar'")
        assert lyr.GetFeatureCount() == 3
        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == ["data/parquet/partitioned_hive/foo=bar/part.0.parquet"]

        lyr.SetAttributeFilter("foo = 'baz' AND two = -5")
        assert lyr.GetFeatureCount() == 1
        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == ["data/parquet/partitioned_hive/foo=baz/part.1.parquet"]


def test_ogr_parquet_read_partitioned_hive_integer_key():

    with gdaltest.config_options({"OGR_PARQUET_USE_VSI": "YES"}):
        ds = ogr.Open("data/parquet/partitioned_hive_integer_key")
        lyr = ds.GetLayer(0)

        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == ["data/parquet/partitioned_hive_integer_key/_metadata"]

        lyr.SetAttributeFilter("year = 2022")
        assert lyr.GetFeatureCount() == 2
        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == [
                "data/parquet/partitioned_hive_integer_key/year=2022/cffaf5cf4ab148d89a5a6047f2be2757-0.parquet"
            ]


###############################################################################
# Test reading a partitioned dataset with geo


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_read_partitioned_geo():

    gdal.Mkdir("/vsimem/somedir", 0o755)
    for name, wkt in [
        ("part.0.parquet", "POINT(1 2)"),
        ("part.1.parquet", "POINT(3 4)"),
    ]:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource("/vsimem/somedir/" + name)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

    ds = ogr.Open("/vsimem/somedir")
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "geometry"
    assert lyr.GetExtent() == (1, 3, 2, 4)
    assert lyr.GetExtent() == (1, 3, 2, 4)

    assert lyr.GetLayerDefn().GetFieldCount() == 0

    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1

    lyr.SetSpatialFilterRect(0, 0, 10, 10)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 2

    lyr.SetSpatialFilterRect(0.9, 1.9, 1.1, 2.1)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 1

    lyr.SetSpatialFilterRect(0.9, 1.9, 0.95, 2.1)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(1.05, 1.9, 1.1, 2.1)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(0.9, 1.9, 1.1, 1.95)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(0.9, 2.05, 1.1, 2.1)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(-100, -100, -100, -100)
    lyr.ResetReading()
    assert lyr.GetNextFeature() is None

    gdal.RmdirRecursive("/vsimem/somedir")


###############################################################################
# Test that we don't write an id in members of datum ensemble
# Cf https://github.com/opengeospatial/geoparquet/discussions/110


@pytest.mark.require_proj(7, 2)
def test_ogr_parquet_write_crs_without_id_in_datum_ensemble_members():

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ds.CreateLayer("out", geom_type=ogr.wkbPoint, srs=srs)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    crs = j["columns"]["geometry"]["crs"]
    assert "id" not in crs["base_crs"]["datum_ensemble"]["members"][0]

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert int(srs.GetAuthorityCode(None)) == 32631
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################


def test_ogr_parquet_arrow_stream_numpy():
    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(
        options=["MAX_FEATURES_IN_BATCH=5", "USE_MASKED_ARRAYS=NO"]
    )
    with gdal.quiet_errors():
        batches = [batch for batch in stream]
    assert len(batches) == 2
    batch = batches[0]
    assert batch.keys() == {
        "boolean",
        "uint8",
        "int8",
        "uint16",
        "int16",
        "uint32",
        "int32",
        "uint64",
        "int64",
        "float32",
        "float64",
        "string",
        "large_string",
        "timestamp_ms_gmt",
        "timestamp_ms_gmt_plus_2",
        "timestamp_ms_gmt_minus_0215",
        "timestamp_s_no_tz",
        "timestamp_us_no_tz",
        "timestamp_ns_no_tz",
        "time32_s",
        "time32_ms",
        "time64_us",
        "date32",
        "date64",
        "binary",
        "large_binary",
        "fixed_size_binary",
        "list_boolean",
        "list_uint8",
        "list_int8",
        "list_uint16",
        "list_int16",
        "list_uint32",
        "list_int32",
        "list_uint64",
        "list_int64",
        "list_float32",
        "list_float64",
        "list_string",
        "fixed_size_list_boolean",
        "fixed_size_list_uint8",
        "fixed_size_list_int8",
        "fixed_size_list_uint16",
        "fixed_size_list_int16",
        "fixed_size_list_uint32",
        "fixed_size_list_int32",
        "fixed_size_list_uint64",
        "fixed_size_list_int64",
        "fixed_size_list_float32",
        "fixed_size_list_float64",
        "fixed_size_list_string",
        "struct_field.a",
        "struct_field.b",
        "struct_field.c.d",
        "struct_field.c.f",
        "struct_field.h",
        "struct_field.i",
        "dict",
        "geometry",
    }
    assert batch["boolean"][0] == True
    assert batch["boolean"][1] == False
    assert batch["boolean"][2] == False
    assert batches[1]["boolean"][0] == False
    assert batches[1]["boolean"][1] == True
    assert batch["uint8"][0] == 1
    assert batches[1]["uint8"][0] == 4
    assert batch["int8"][0] == -2
    assert batch["string"][0] == b"abcd"
    assert batch["string"][1] == b""
    assert batch["string"][2] == b""
    assert batches[1]["string"][0] == b"c"
    assert batches[1]["string"][1] == b"d"
    assert numpy.array_equal(batch["list_boolean"][0], numpy.array([]))
    assert numpy.array_equal(batch["list_boolean"][1], numpy.array([False]))

    assert numpy.array_equal(
        batches[0]["fixed_size_list_boolean"][0], numpy.array([True, False])
    )
    assert numpy.array_equal(
        batches[0]["fixed_size_list_boolean"][1], numpy.array([False, True])
    )
    assert numpy.array_equal(
        batches[0]["fixed_size_list_boolean"][2], numpy.array([True, False])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_boolean"][0], numpy.array([False, True])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_boolean"][1], numpy.array([True, False])
    )

    assert numpy.array_equal(batch["fixed_size_list_uint8"][0], numpy.array([0, 1]))
    assert numpy.array_equal(batch["list_uint64"][1], numpy.array([0])), batch[
        "list_uint64"
    ][1]
    assert numpy.array_equal(
        batch["fixed_size_list_string"][0], numpy.array([b"a", b"b"], dtype="|S1")
    )
    assert batch["geometry"][0] is not None
    assert batch["geometry"][1] is None
    assert batch["geometry"][0] is not None
    assert batches[1]["geometry"][0] is not None
    assert batches[1]["geometry"][1] is not None
    assert numpy.array_equal(
        batches[1]["list_string"][0], numpy.array([b"A", b"BC", b"CDE"])
    )
    assert numpy.array_equal(batches[1]["list_string"][1], numpy.array([b""]))

    assert numpy.array_equal(batches[0]["list_uint8"][0], numpy.array([]))
    assert numpy.array_equal(batches[0]["list_uint8"][1], numpy.array([0]))
    assert numpy.array_equal(batches[0]["list_uint8"][2], numpy.array([]))
    assert numpy.array_equal(batches[1]["list_uint8"][0], numpy.array([0, 4, 5]))
    assert numpy.array_equal(batches[1]["list_uint8"][1], numpy.array([0, 7, 8, 9]))

    assert batches[0]["fixed_size_binary"][0] == b"\x00\x01"
    assert batches[0]["fixed_size_binary"][1] == b"\x00\x00"
    assert batches[0]["fixed_size_binary"][2] == b"\x01\x01"
    assert batches[1]["fixed_size_binary"][0] == b"\x01\x00"
    assert batches[1]["fixed_size_binary"][1] == b"\x00\x01"

    assert numpy.array_equal(
        batches[0]["fixed_size_list_uint8"][0], numpy.array([0, 1])
    )
    assert numpy.array_equal(
        batches[0]["fixed_size_list_uint8"][1], numpy.array([2, 3])
    )
    assert numpy.array_equal(
        batches[0]["fixed_size_list_uint8"][2], numpy.array([4, 5])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_uint8"][0], numpy.array([6, 7])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_uint8"][1], numpy.array([8, 9])
    )

    assert batches[0]["struct_field.a"][0] == 1
    assert batches[0]["struct_field.a"][1] == 2
    assert batches[0]["struct_field.b"][0] == 2.5

    ignored_fields = ["geometry"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        if lyr_defn.GetFieldDefn(i).GetNameRef() != "string":
            ignored_fields.append(lyr_defn.GetFieldDefn(i).GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    batch = batches[0]
    assert batch.keys() == {"string"}

    # Test ignoring only one subfield of a struct field
    ignored_fields = ["struct_field.a", "struct_field.b"]
    lyr.SetIgnoredFields(ignored_fields)
    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    batch = batches[0]
    # + 1: FID
    # + 1: geometry
    assert (
        len(batch.keys())
        == lyr.GetLayerDefn().GetFieldCount() - len(ignored_fields) + 1 + 1
    )


###############################################################################
# Test reading an empty file with GetArrowStream()


def test_ogr_parquet_arrow_stream_empty_file():

    ds = ogr.GetDriverByName("Parquet").CreateDataSource("/vsimem/test.parquet")
    ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = None
    ds = ogr.Open("/vsimem/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1
    stream = lyr.GetArrowStream()
    assert stream.GetNextRecordBatch() is None
    del stream
    ds = None

    ogr.GetDriverByName("Parquet").DeleteDataSource("/vsimem/test.parquet")


###############################################################################


def test_ogr_parquet_arrow_stream_numpy_with_fid_column():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    filename = "/vsimem/test_ogr_parquet_arrow_stream_numpy_with_fid_column.parquet"
    gdal.VectorTranslate(
        filename, "data/poly.shp", options="-unsetFieldWidth -lco FID=fid"
    )
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    batch = batches[0]
    assert batch.keys() == set(["fid", "AREA", "EAS_ID", "PRFEDEA", "geometry"])

    lyr.SetIgnoredFields(["geometry"])

    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    batch = batches[0]
    assert batch.keys() == set(["fid", "AREA", "EAS_ID", "PRFEDEA"])

    ds = None
    gdal.Unlink(filename)


###############################################################################


def test_ogr_parquet_arrow_stream_numpy_fast_spatial_filter():
    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")
    import datetime

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    ignored_fields = ["decimal128", "decimal256", "time64_ns"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        fld_defn = lyr_defn.GetFieldDefn(i)
        if fld_defn.GetName().startswith("map_"):
            ignored_fields.append(fld_defn.GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    lyr.SetSpatialFilterRect(-10, -10, 10, 10)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fc = 0
    for batch in stream:
        fc += len(batch["uint8"])
    assert fc == 4

    lyr.SetSpatialFilterRect(4, 2, 4, 2)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert len(batch["geometry"]) == 1
    assert batch["uint8"][0] == 5
    assert numpy.array_equal(
        batch["fixed_size_list_boolean"][0], numpy.array([True, False])
    )
    assert numpy.array_equal(batch["fixed_size_list_uint8"][0], numpy.array([8, 9]))
    assert numpy.array_equal(batch["fixed_size_list_uint16"][0], numpy.array([8, 9]))
    assert numpy.array_equal(
        batch["fixed_size_list_string"][0], numpy.array([b"i", b"j"])
    )
    assert numpy.array_equal(
        batch["list_boolean"][0], numpy.array([True, False, True, False])
    )
    assert numpy.array_equal(batch["list_uint8"][0], numpy.array([0, 7, 8, 9]))
    assert numpy.array_equal(batch["list_uint16"][0], numpy.array([0, 7, 8, 9]))
    assert numpy.array_equal(batch["list_string"][0], numpy.array([b""]))
    assert (
        ogr.CreateGeometryFromWkb(batch["geometry"][0]).ExportToWkt() == "POINT (4 2)"
    )
    assert batch["fixed_size_binary"][0] == b"\x00\x01"

    lyr.SetSpatialFilterRect(3, 2, 3, 2)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert len(batch["geometry"]) == 1
    assert batch["boolean"][0] == False
    assert batch["uint8"][0] == 4
    assert batch["int8"][0] == 1
    assert batch["uint16"][0] == 30001
    assert batch["int16"][0] == 10000
    assert batch["uint32"][0] == 3000000001
    assert batch["int32"][0] == 1000000000
    assert batch["uint64"][0] == 300000000001
    assert batch["int64"][0] == 100000000000
    assert batch["int64"][0] == 100000000000
    assert batch["float32"][0] == 4.5
    assert batch["float64"][0] == 4.5
    assert batch["string"][0] == b"c"
    assert batch["large_string"][0] == b"c"
    assert batch["fixed_size_binary"][0] == b"\x01\x00"
    assert batch["timestamp_ms_gmt"][0] == numpy.datetime64("2019-01-01T14:00:00.500")
    assert batch["time32_s"][0] == datetime.time(0, 0, 4)
    assert batch["time32_ms"][0] == datetime.time(0, 0, 0, 4000)
    assert batch["time64_us"][0] == datetime.time(0, 0, 0, 4)
    assert batch["date32"][0] == numpy.datetime64("1970-01-05")
    assert batch["date64"][0] == numpy.datetime64("1970-01-01")
    assert bytes(batch["binary"][0]) == b"\00\01"
    assert bytes(batch["large_binary"][0]) == b"\00\01"
    assert batch["struct_field.a"][0] == 4
    assert batch["struct_field.c.d"][0] == b"e345"
    assert numpy.array_equal(
        batch["fixed_size_list_boolean"][0], numpy.array([False, True])
    )
    assert numpy.array_equal(batch["fixed_size_list_uint8"][0], numpy.array([6, 7]))
    assert numpy.array_equal(batch["fixed_size_list_uint16"][0], numpy.array([6, 7]))
    assert numpy.array_equal(
        batch["fixed_size_list_string"][0], numpy.array([b"g", b"h"])
    )
    assert numpy.array_equal(
        batch["list_boolean"][0], numpy.array([False, False, True])
    )
    assert numpy.array_equal(batch["list_uint8"][0], numpy.array([0, 4, 5]))
    assert numpy.array_equal(batch["list_uint16"][0], numpy.array([0, 4, 5]))
    assert numpy.array_equal(
        batch["list_string"][0], numpy.array([b"A", b"BC", b"CDE"])
    )
    assert (
        ogr.CreateGeometryFromWkb(batch["geometry"][0]).ExportToWkt() == "POINT (3 2)"
    )

    lyr.SetSpatialFilterRect(1, 1, 1, 1)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 0


###############################################################################


def test_ogr_parquet_arrow_stream_numpy_detailed_spatial_filter(tmp_vsimem):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    filename = str(
        tmp_vsimem
        / "test_ogr_parquet_arrow_stream_numpy_detailed_spatial_filter.parquet"
    )
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(filename)
    lyr = ds.CreateLayer("test", options=["FID=fid"])
    for idx, wkt in enumerate(
        [
            "POINT(1 2)",
            "MULTIPOINT(0 0,1 2)",
            "LINESTRING(3 4,5 6)",
            "MULTILINESTRING((7 8,7.5 8.5),(3 4,5 6))",
            "POLYGON((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21))",
            "MULTIPOLYGON(((100 100,100 200,200 200,100 100)),((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21)))",
            "LINESTRING EMPTY",
            "MULTILINESTRING EMPTY",
            "POLYGON EMPTY",
            "MULTIPOLYGON EMPTY",
            "GEOMETRYCOLLECTION EMPTY",
        ]
    ):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(idx)
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

    eps = 1e-1

    # Select nothing
    with ogrtest.spatial_filter(lyr, 6, 0, 8, 1):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 0

    # Select POINT and MULTIPOINT
    with ogrtest.spatial_filter(lyr, 1 - eps, 2 - eps, 1 + eps, 2 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [0, 1]
        assert [f.GetFID() for f in lyr] == [0, 1]

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 3 - eps, 4 - eps, 3 + eps, 4 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 5 - eps, 6 - eps, 5 + eps, 6 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select LINESTRING and MULTILINESTRING due to more generic intersection
    with ogrtest.spatial_filter(lyr, 4 - eps, 5 - eps, 4 + eps, 5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select POLYGON and MULTIPOLYGON due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 10 - eps, 20 - eps, 10 + eps, 20 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [4, 5]
        assert [f.GetFID() for f in lyr] == [4, 5]

    # bbox with polygon hole
    with ogrtest.spatial_filter(lyr, 12 - eps, 20.5 - eps, 12 + eps, 20.5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        if ogrtest.have_geos():
            assert len(batches) == 0
        else:
            assert len(batches) == 1
            assert list(batches[0]["fid"]) == [4, 5]
            assert [f.GetFID() for f in lyr] == [4, 5]

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SetAttributeFilter() and arrow stream interface


@pytest.mark.parametrize(
    "filter",
    [
        "boolean = 0",
        "boolean = 1",
        "uint8 = 2",
        "uint8 = -1",
        "int8 = -1",
        "int8 = 0",
        "int8 IS NULL",
        "int8 IS NOT NULL",
        "uint16 = 10001",
        "uint32 = 1000000001",
        "int32 = -1000000000",
        "uint64 = 100000000001",
        "int64 = -100000000000",
        "float32 = 2.5",
        "float64 = 2.5",
        "float64 != 2.5",
        "string = 'd'",
        "string != 'd'",
        "large_string = 'd'",
        "large_string != 'd'",
        "binary = '0001'",
        "large_binary = '0001'",
        "fixed_size_binary = '0001'",
        "timestamp_ms_gmt = '2019-01-01T14:00:00.500'",
        "timestamp_ms_gmt != '2019-01-01T14:00:00.500'",
        "timestamp_ms_gmt_plus_2 = '2019-01-01T14:00:00.500+02'",
        "timestamp_ms_gmt_plus_2 != '2019-01-01T14:00:00.500+02'",
        "timestamp_s_no_tz = '2019-01-01T14:00:00'",
        "timestamp_s_no_tz != '2019-01-01T14:00:00'",
        "timestamp_us_no_tz = '2019-01-01T14:00:00.001'",
        "timestamp_us_no_tz != '2019-01-01T14:00:00.001'",
        "timestamp_ns_no_tz = '2019-01-01T14:00:00'",
        "timestamp_ns_no_tz != '2019-01-01T14:00:00'",
        "time32_s = '00:00:05'",
        "time32_ms = '00:00:00.002'",
        "time64_us = 3723000000",
        # not dealt by GetArrowStreamAsNumPy
        # "time64_ns = 3723000000456",
        "date32 = '1970-01-02'",
        "date64 = '1970-01-02'",
    ],
)
def test_ogr_parquet_arrow_stream_numpy_fast_attribute_filter(filter):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    ignored_fields = ["decimal128", "decimal256", "time64_ns"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        fld_defn = lyr_defn.GetFieldDefn(i)
        if fld_defn.GetName().startswith("map_"):
            ignored_fields.append(fld_defn.GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    lyr.SetAttributeFilter(filter)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fc = 0
    for batch in stream:
        fc += len(batch["uint8"])
    assert fc == lyr.GetFeatureCount()
    if filter not in ("uint8 = -1", "int8 = 0"):
        assert fc != 0


###############################################################################


def test_ogr_parquet_arrow_stream_numpy_attribute_filter_on_fid_without_fid_column():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    ignored_fields = ["decimal128", "decimal256", "time64_ns"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        fld_defn = lyr_defn.GetFieldDefn(i)
        if fld_defn.GetName().startswith("map_"):
            ignored_fields.append(fld_defn.GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    lyr.SetAttributeFilter("fid in (1, 3)")
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    f = lyr.GetNextFeature()
    assert f["uint8"] == 2
    f = lyr.GetNextFeature()
    assert f["uint8"] == 4
    f = lyr.GetNextFeature()
    assert f is None

    lyr.ResetReading()
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    vals = []
    for batch in stream:
        for v in batch["uint8"]:
            vals.append(v)
    assert vals == [2, 4]

    # Check that it works if the effect of the attribute filter is to
    # skip entire row groups.
    lyr.SetAttributeFilter("fid = 4")

    lyr.ResetReading()
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    vals = []
    for batch in stream:
        for v in batch["uint8"]:
            vals.append(v)
    assert vals == [5]


###############################################################################


def test_ogr_parquet_arrow_stream_numpy_attribute_filter_on_fid_with_fid_column():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    filename = "/vsimem/test_ogr_parquet_arrow_stream_numpy_attribute_filter_on_fid_with_fid_column.parquet"
    gdal.VectorTranslate(
        filename, "data/poly.shp", options="-unsetFieldWidth -lco FID=my_fid"
    )
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("fid in (1, 3)")
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    f = lyr.GetNextFeature()
    assert f["EAS_ID"] == 179
    f = lyr.GetNextFeature()
    assert f["EAS_ID"] == 173
    f = lyr.GetNextFeature()
    assert f is None

    lyr.ResetReading()
    stream = lyr.GetArrowStreamAsNumPy()
    vals_fid = []
    vals_EAS_ID = []
    for batch in stream:
        for v in batch["my_fid"]:
            vals_fid.append(v)
        for v in batch["EAS_ID"]:
            vals_EAS_ID.append(v)
    assert vals_fid == [1, 3]
    assert vals_EAS_ID == [179, 173]

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test attribute filter through ArrowStream API
# We use the pyarrow API, to be able to test we correctly deal with decimal
# data type


@pytest.mark.parametrize("OGR_ARROW_STREAM_BASE_IMPL", [None, "YES"])
@pytest.mark.parametrize(
    "test_file", ["data/parquet/test.parquet", "data/parquet/test_single_group.parquet"]
)
def test_ogr_parquet_arrow_stream_fast_attribute_filter_pyarrow(
    OGR_ARROW_STREAM_BASE_IMPL, test_file
):
    pa = pytest.importorskip("pyarrow")

    ds = ogr.Open(test_file)
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema
    batches = []
    new_schema = pa.schema([f for f in schema])
    for batch in stream:
        batches.append(
            pa.RecordBatch.from_arrays(
                [batch.field(f.name) for f in schema], schema=new_schema
            )
        )
    table = pa.Table.from_batches(batches)
    table.validate(full=True)
    full_table = []
    for col in table:
        new_col = []
        for row in col:
            new_col.append(row)
        full_table.append(new_col)

    lyr.SetAttributeFilter("boolean = 0")
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1
    with gdaltest.config_option(
        "OGR_ARROW_STREAM_BASE_IMPL", OGR_ARROW_STREAM_BASE_IMPL
    ):
        stream = lyr.GetArrowStreamAsPyArrow()
    uint8_vals = []
    decimal128_vals = []
    fixed_size_binary_vals = []
    for batch in stream:
        for x in batch.field("uint8"):
            uint8_vals.append(x.as_py())
        for x in batch.field("decimal128"):
            decimal128_vals.append(float(x.as_py()))
        for x in batch.field("fixed_size_binary"):
            fixed_size_binary_vals.append(x.as_py())
    assert uint8_vals == [2, 4]
    assert decimal128_vals == [-1234.567, 1234.567]
    assert fixed_size_binary_vals == [b"\x00\x00", b"\x01\x00"]

    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema
    batches = []
    new_schema = pa.schema([f for f in schema])
    for batch in stream:
        batches.append(
            pa.RecordBatch.from_arrays(
                [batch.field(f.name) for f in schema], schema=new_schema
            )
        )
    table = pa.Table.from_batches(batches)
    table.validate(full=True)
    for (col, full_col) in zip(table, full_table):
        assert col[0] == full_col[1]
        assert col[1] == full_col[3]

    lyr.SetAttributeFilter("boolean = 1")
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1
    with gdaltest.config_option(
        "OGR_ARROW_STREAM_BASE_IMPL", OGR_ARROW_STREAM_BASE_IMPL
    ):
        stream = lyr.GetArrowStreamAsPyArrow()
    uint8_vals = []
    decimal128_vals = []
    fixed_size_binary_vals = []
    map_boolean_vals = []
    for batch in stream:
        for x in batch.field("uint8"):
            uint8_vals.append(x.as_py())
        for x in batch.field("decimal128"):
            decimal128_vals.append(float(x.as_py()))
        for x in batch.field("fixed_size_binary"):
            fixed_size_binary_vals.append(x.as_py())
        for x in batch.field("map_boolean"):
            map_boolean_vals.append(x.as_py())
    assert uint8_vals == [1, 5]
    assert decimal128_vals == [1234.567, -1234.567]
    assert fixed_size_binary_vals == [b"\x00\x01", b"\x00\x01"]
    if OGR_ARROW_STREAM_BASE_IMPL is None:
        assert map_boolean_vals == [[("x", None), ("y", True)], []]
    else:
        assert map_boolean_vals == ['{"x":null,"y":true}', "{}"]

    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema
    batches = []
    new_schema = pa.schema([f for f in schema])
    for batch in stream:
        batches.append(
            pa.RecordBatch.from_arrays(
                [batch.field(f.name) for f in schema], schema=new_schema
            )
        )
    table = pa.Table.from_batches(batches)
    table.validate(full=True)


def test_ogr_parquet_arrow_stream_fast_attribute_filter_on_decimal128():
    pytest.importorskip("pyarrow")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("decimal128 = -1234.567")
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1
    stream = lyr.GetArrowStreamAsPyArrow()
    batches = [batch for batch in stream]
    assert len(batches) == 2
    assert len(batches[0].field("uint8")) == 1
    assert len(batches[1].field("uint8")) == 1
    assert batches[0].field("uint8")[0].as_py() == 2
    assert batches[1].field("uint8")[0].as_py() == 5
    assert float(batches[0].field("decimal128")[0].as_py()) == -1234.567
    assert float(batches[1].field("decimal128")[0].as_py()) == -1234.567


###############################################################################
# Combine both spatial and attribute filters through ArrowStream API


def test_ogr_parquet_arrow_stream_numpy_fast_spatial_and_attribute_filter():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    ignored_fields = ["decimal128", "decimal256", "time64_ns"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        fld_defn = lyr_defn.GetFieldDefn(i)
        if fld_defn.GetName().startswith("map_"):
            ignored_fields.append(fld_defn.GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    lyr.SetAttributeFilter("uint8 = 4 or uint8 = 5")
    lyr.SetSpatialFilterRect(0, 2, 3, 2)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fc = 0
    for batch in stream:
        fc += len(batch["uint8"])
    assert fc == lyr.GetFeatureCount()


###############################################################################
# Test bbox


@pytest.mark.parametrize(
    "input_geometries,expected_bbox",
    [
        (["POINT(1 2)"], [1, 2, 1, 2]),
        (["POINT(1 2 3)"], [1, 2, 3, 1, 2, 3]),
        (["POINT(3 4)", "POINT(1 2)"], [1, 2, 3, 4]),
        (["POINT(4 5 6)", "POINT(1 2 3)"], [1, 2, 3, 4, 5, 6]),
        (["POINT(4 5 6)", "POINT(1 2)"], [1, 2, 6, 4, 5, 6]),
    ],
)
def test_ogr_parquet_bbox(input_geometries, expected_bbox):

    outfilename = "/vsimem/out.parquet"
    ds = gdal.GetDriverByName("Parquet").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("out")
    for input_geom in input_geometries:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(input_geom))
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert "columns" in j
    assert "geometry" in j["columns"]
    assert "bbox" in j["columns"]["geometry"]
    assert j["columns"]["geometry"]["bbox"] == expected_bbox
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test storing OGR field alternative name and comment in gdal:schema extension


def test_ogr_parquet_field_alternative_name_comment():

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        fld_defn = ogr.FieldDefn("fld", ogr.OFTInteger)
        fld_defn.SetAlternativeName("long_field_name")
        fld_defn.SetComment("this is a field")
        lyr.CreateField(fld_defn)
        ds = None

        ds = ogr.Open(outfilename)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr is not None
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "fld"
        assert (
            lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "long_field_name"
        )
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == "this is a field"
        lyr = None
        ds = None
    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test reading a parquet file with WKT content as WKB through ArrowArray
# interface


@pytest.mark.parametrize("nullable_geom", [False, True])
@pytest.mark.parametrize("ignore_geom_field", [False, True])
@pytest.mark.parametrize("ignore_geom_before", [False, True])
def test_ogr_parquet_read_wkt_as_wkt_arrow_array(
    nullable_geom, ignore_geom_field, ignore_geom_before
):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbNone, options=["GEOMETRY_ENCODING=WKT"]
        )
        lyr.CreateGeomField(ogr.GeomFieldDefn("geom_before", ogr.wkbUnknown))
        geom_fld_defn = ogr.GeomFieldDefn("my_geom", ogr.wkbUnknown)
        geom_fld_defn.SetNullable(nullable_geom)
        lyr.CreateGeomField(geom_fld_defn)
        lyr.CreateGeomField(ogr.GeomFieldDefn("geom_after", ogr.wkbUnknown))
        fld_defn = ogr.FieldDefn("fld", ogr.OFTInteger)
        lyr.CreateField(fld_defn)

        input_wkts = [
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",  # benefits from optimization for single part single ring
            "MULTIPOLYGON (((0 0,0 2,2 2,0 0)))",
            "MULTIPOLYGON Z (((0 0 -1,0 2 -1,2 2 -1,0 0 -1)))",
            "MULTIPOLYGON M (((0 0 -1,0 2 -1,2 2 -1,0 0 -1)))",
            "MULTIPOLYGON ZM (((0 0 -1 -2,0 2 -1 -2,2 2 -1 -2,0 0 -1 -2)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 10,10 11,11 11,10 10)))",  # two parts
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2)))",  # two rings
            None,
            "POLYGON ((0 0,0 2,2 2,0 0))",
        ]
        expected_wkts = []

        val = 1
        for wkt in input_wkts:
            if wkt:
                f = ogr.Feature(lyr.GetLayerDefn())
                f["fld"] = val
                val += 1
                f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (1 2)"))
                f.SetGeomField(1, ogr.CreateGeometryFromWkt(wkt))
                f.SetGeomField(2, ogr.CreateGeometryFromWkt("POINT (3 4)"))
                lyr.CreateFeature(f)
                expected_wkts.append(wkt)
            elif nullable_geom:
                f = ogr.Feature(lyr.GetLayerDefn())
                f["fld"] = val
                val += 1
                f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (1 2)"))
                f.SetGeomField(2, ogr.CreateGeometryFromWkt("POINT (3 4)"))
                lyr.CreateFeature(f)
                expected_wkts.append(None)
        ds = None

        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)

        ignored_fields = []
        if ignore_geom_before:
            ignored_fields.append("geom_before")
        if ignore_geom_field:
            ignored_fields.append("my_geom")
        if ignored_fields:
            lyr.SetIgnoredFields(ignored_fields)

        # Get geometry column in its raw form (WKT)
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        batch = batches[0]
        assert [x for x in batch["fld"]] == [
            i for i in range(1, len(expected_wkts) + 1)
        ]
        if not ignore_geom_before:
            assert [x.decode("ASCII") for x in batch["geom_before"]] == [
                "POINT (1 2)"
            ] * len(expected_wkts)
        if ignore_geom_field:
            assert "my_geom" not in batch.keys()
        else:
            assert [
                (x.decode("ASCII") if x else None) for x in batch["my_geom"]
            ] == expected_wkts
        assert [x.decode("ASCII") for x in batch["geom_after"]] == [
            "POINT (3 4)"
        ] * len(expected_wkts)

        # Force geometry column as WKB
        stream = lyr.GetArrowStreamAsNumPy(
            options=["USE_MASKED_ARRAYS=NO", "GEOMETRY_ENCODING=WKB"]
        )
        batches = [batch for batch in stream]
        batch = batches[0]
        assert [x for x in batch["fld"]] == [
            i for i in range(1, len(expected_wkts) + 1)
        ]
        if not ignore_geom_before:
            assert [
                ogr.CreateGeometryFromWkb(x).ExportToIsoWkt()
                for x in batch["geom_before"]
            ] == ["POINT (1 2)"] * len(expected_wkts)
        if ignore_geom_field:
            assert "my_geom" not in batch.keys()
        else:
            assert [
                (ogr.CreateGeometryFromWkb(x).ExportToIsoWkt() if x else None)
                for x in batch["my_geom"]
            ] == expected_wkts
        assert [
            ogr.CreateGeometryFromWkb(x).ExportToIsoWkt() for x in batch["geom_after"]
        ] == ["POINT (3 4)"] * len(expected_wkts)

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test reading a parquet file with WKT content as WKB through ArrowArray
# interface


def test_ogr_parquet_read_wkt_with_dict_as_wkt_arrow_array():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/wkt_with_dict.parquet")
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "GEOMETRY_ENCODING=WKB"]
    )
    geoms = []
    for batch in stream:
        for wkb in batch["geometry"]:
            if wkb:
                geoms.append(ogr.CreateGeometryFromWkb(wkb).ExportToWkt())
            else:
                geoms.append(None)
    assert geoms == ["POINT (1 2)", "POINT (3 4)", None, "POINT (7 8)", "POINT (9 10)"]


###############################################################################
# Check that GetArrowStream interface always returns
# ARROW:extension:name = ogc.wkb as a field schema metadata.


def test_ogr_parquet_check_geom_column_schema_metadata():
    pytest.importorskip("pyarrow")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema
    field = schema.field("geometry")
    field_md = field.metadata
    arrow_extension_name = field_md.get(b"ARROW:extension:name", None)
    assert arrow_extension_name == b"ogc.wkb"


###############################################################################
# Check that we recognize the geometry field just from the presence of
# a ARROW:extension:name == ogc.wkb column on it


def test_ogr_parquet_recognize_geo_from_arrow_extension_name():

    outfilename = "/vsimem/out.parquet"
    try:
        with gdaltest.config_options(
            {
                "OGR_PARQUET_WRITE_GEO": "NO",
                "OGR_PARQUET_WRITE_ARROW_EXTENSION_NAME": "YES",
            }
        ):
            ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
            lyr = ds.CreateLayer("test", options=_get_do_not_use_parquet_geo_types())
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
            lyr.CreateFeature(f)
            f = None
            ds = None

        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)
        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        assert geo is None
        assert lyr.GetGeomType() == ogr.wkbPoint
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Check that we recognize the geometry field just from its name


@pytest.mark.parametrize(
    "geom_col_name,is_wkb", [("geometry", True), ("wkt_geometry", False)]
)
def test_ogr_parquet_recognize_geo_from_geom_possible_names(geom_col_name, is_wkb):

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
        lyr.CreateField(
            ogr.FieldDefn(geom_col_name, ogr.OFTBinary if is_wkb else ogr.OFTString)
        )
        lyr.CreateField(ogr.FieldDefn("bar", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        if is_wkb:
            wkb = ogr.CreateGeometryFromWkt("POINT (1 2)").ExportToIsoWkb()
            f.SetField(geom_col_name, wkb)
        else:
            f[geom_col_name] = "POINT (1 2)"
        lyr.CreateFeature(f)
        f = None
        ds = None

        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)
        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        assert geo is None
        assert lyr.GetGeometryColumn() == geom_col_name
        assert lyr.GetGeomType() == ogr.wkbUnknown
        assert lyr.GetSpatialRef() is None
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"
        ds = None

        ds = gdal.OpenEx(outfilename, open_options=["GEOM_POSSIBLE_NAMES=bar"])
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbNone
        ds = None

        ds = gdal.OpenEx(
            outfilename,
            open_options=[
                f"GEOM_POSSIBLE_NAMES=foo,{geom_col_name},bar",
                "CRS=EPSG:4326",
            ],
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetGeometryColumn() == geom_col_name
        assert lyr.GetGeomType() == ogr.wkbUnknown
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test reading a partitioned dataset with a part with an empty batch


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_read_dataset_with_empty_batch():
    ds = ogr.Open("data/parquet/part_with_empty_batch")
    lyr = ds.GetLayer(0)
    # Check that we don't iterate forever
    lyr.GetExtent()
    assert len([f for f in lyr]) == 1


###############################################################################
# Test MIN() / MAX() on FID column


def test_ogr_parquet_statistics_fid_column():

    outfilename = "/vsimem/out.parquet"
    try:
        ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=["FID=FID"])
        for fid in (2, 4, 9876543210):
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetFID(fid)
            lyr.CreateFeature(f)
        ds = None
        ds = ogr.Open(outfilename)
        with ds.ExecuteSQL("SELECT MIN(FID), MAX(FID) FROM out") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f["MIN_FID"] == 2
            assert f["MAX_FID"] == 9876543210

        with ds.ExecuteSQL("SELECT * FROM out WHERE FID = 4") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f
            assert f.GetFID() == 4
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Read nested types that we map to JSON


def test_ogr_parquet_nested_types():

    # File generated by autotest/generate_parquet_test_file.py::generate_nested_types()
    ds = ogr.Open("data/parquet/nested_types.parquet")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["map_list_bool"] == """{"x":[true],"y":[false,true]}"""
    assert f["map_list_uint8"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_int8"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_uint16"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_int16"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_uint32"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_int32"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_uint64"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_int64"] == """{"x":[2],"y":[3,4]}"""
    assert f["map_list_float32"] == """{"x":[2.0],"y":[3.0,4.0]}"""
    assert f["map_list_float64"] == """{"x":[2.0],"y":[3.0,4.0]}"""
    assert f["map_map_bool"] == """{"a":{"b":true,"c":null,"d":null},"e":null}"""
    assert f["map_map_uint8"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_int8"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_uint16"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_int16"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_uint32"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_int32"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_uint64"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_int64"] == """{"a":{"b":1,"c":null,"d":2},"e":null}"""
    assert f["map_map_float32"] == """{"a":{"b":1.0,"c":null,"d":2.0},"e":null}"""
    assert f["map_map_float64"] == """{"a":{"b":1.0,"c":null,"d":2.0},"e":null}"""
    assert f["map_map_string"] == """{"a":{"b":"c","d":null},"e":null}"""
    assert f["list_list_bool"] == """[[true],null,[false,null,true]]"""
    assert f["list_list_uint8"] == """[[1],null,[2,null,3]]"""
    assert f["list_list_int8"] == """[[1],null,[-2,null,3]]"""
    assert f["list_list_uint16"] == """[[1],null,[2,null,3]]"""
    assert f["list_list_int16"] == """[[1],null,[-2,null,3]]"""
    assert f["list_list_uint32"] == """[[1],null,[2,null,3]]"""
    assert f["list_list_int32"] == """[[1],null,[-2,null,3]]"""
    assert f["list_list_uint64"] == """[[1],null,[2,null,3]]"""
    assert f["list_list_int64"] == """[[1],null,[-2,null,3]]"""
    assert f["list_list_float32"] == """[[1.5],null,[-2.5,null,3.5]]"""
    assert f["list_list_float64"] == """[[1.5],null,[-2.5,null,3.5]]"""
    assert (
        f["list_list_decimal128"] == """[[1234.567],null,[-1234.567,null,1234.567]]"""
    )
    assert (
        f["list_list_decimal256"] == """[[1234.567],null,[-1234.567,null,1234.567]]"""
    )
    assert f["list_list_string"] == """[["a"],null,["b",null,"cd"]]"""
    assert f["list_list_large_string"] == """[["a"],null,["b",null,"cd"]]"""
    assert f["list_list_binary"] == """[["a"],null,["b",null,"cd"]]"""
    assert f["list_large_list_string"] == """[["a"],null,["b",null,"cd"]]"""
    assert f["list_fixed_size_list_string"] == """[["a","b"]]"""
    assert f["list_map_string"] == """[{"a":"b","c":"d"},{"e":"f"}]"""

    f = lyr.GetNextFeature()
    assert f["map_list_bool"] == """{"z":[]}"""
    assert f["map_list_uint8"] == """{"z":[]}"""
    assert f["map_list_int8"] == """{"z":[]}"""
    assert f["map_list_uint16"] == """{"z":[]}"""
    assert f["map_list_int16"] == """{"z":[]}"""
    assert f["map_list_uint32"] == """{"z":[]}"""
    assert f["map_list_int32"] == """{"z":[]}"""
    assert f["map_list_uint64"] == """{"z":[]}"""
    assert f["map_list_int64"] == """{"z":[]}"""
    assert f["map_list_float32"] == """{"z":[]}"""
    assert f["map_list_float64"] == """{"z":[]}"""
    assert f["map_map_bool"] is None
    assert f["map_map_uint8"] is None
    assert f["map_map_int8"] is None
    assert f["map_map_uint16"] is None
    assert f["map_map_int16"] is None
    assert f["map_map_uint32"] is None
    assert f["map_map_int32"] is None
    assert f["map_map_uint64"] is None
    assert f["map_map_int64"] is None
    assert f["map_map_float32"] is None
    assert f["map_map_float64"] is None
    assert f["map_map_string"] is None
    assert f["list_list_decimal128"] is None
    assert f["list_list_decimal256"] is None
    assert f["list_list_string"] is None
    assert f["list_list_large_string"] is None
    assert f["list_list_binary"] is None
    assert f["list_large_list_string"] is None
    assert f["list_fixed_size_list_string"] is None
    assert f["list_map_string"] is None

    f = lyr.GetNextFeature()
    assert f["map_list_bool"] is None
    assert f["map_list_uint8"] is None
    assert f["map_list_int8"] is None
    assert f["map_list_uint16"] is None
    assert f["map_list_int16"] is None
    assert f["map_list_uint32"] is None
    assert f["map_list_int32"] is None
    assert f["map_list_uint64"] is None
    assert f["map_list_int64"] is None
    assert f["map_list_float32"] is None
    assert f["map_list_float64"] is None
    assert f["map_map_bool"] == """{"f":{"g":false}}"""
    assert f["map_map_uint8"] == """{"f":{"g":3}}"""
    assert f["map_map_int8"] == """{"f":{"g":3}}"""
    assert f["map_map_uint16"] == """{"f":{"g":3}}"""
    assert f["map_map_int16"] == """{"f":{"g":3}}"""
    assert f["map_map_uint32"] == """{"f":{"g":3}}"""
    assert f["map_map_int32"] == """{"f":{"g":3}}"""
    assert f["map_map_uint64"] == """{"f":{"g":3}}"""
    assert f["map_map_int64"] == """{"f":{"g":3}}"""
    assert f["map_map_float32"] == """{"f":{"g":3.0}}"""
    assert f["map_map_float64"] == """{"f":{"g":3.0}}"""
    assert f["map_map_string"] == """{"f":{"g":"h"}}"""
    assert f["list_list_decimal128"] == """[[-1234.567]]"""
    assert f["list_list_decimal256"] == """[[-1234.567]]"""
    assert f["list_list_string"] == """[["efg"]]"""
    assert f["list_list_large_string"] == """[["efg"]]"""
    assert f["list_list_binary"] == """[["base64:AQI="]]"""
    assert f["list_large_list_string"] == """[["efg"]]"""
    assert f["list_fixed_size_list_string"] == """[["e","f"]]"""
    assert f["list_map_string"] == """[null]"""

    f = lyr.GetNextFeature()
    assert f["map_list_bool"] == """{"w":[true,false]}"""
    assert f["map_list_uint8"] == """{"w":[5,6]}"""
    assert f["map_list_int8"] == """{"w":[5,6]}"""
    assert f["map_list_uint16"] == """{"w":[5,6]}"""
    assert f["map_list_int16"] == """{"w":[5,6]}"""
    assert f["map_list_uint32"] == """{"w":[5,6]}"""
    assert f["map_list_int32"] == """{"w":[5,6]}"""
    assert f["map_list_uint64"] == """{"w":[5,6]}"""
    assert f["map_list_int64"] == """{"w":[5,6]}"""
    assert f["map_list_float32"] == """{"w":[5.0,6.0]}"""
    assert f["map_list_float64"] == """{"w":[5.0,6.0]}"""
    assert f["map_map_bool"] is None
    assert f["map_map_uint8"] is None
    assert f["map_map_int8"] is None
    assert f["map_map_uint16"] is None
    assert f["map_map_int16"] is None
    assert f["map_map_uint32"] is None
    assert f["map_map_int32"] is None
    assert f["map_map_uint64"] is None
    assert f["map_map_int64"] is None
    assert f["map_map_float32"] is None
    assert f["map_map_float64"] is None
    assert f["map_map_string"] is None
    assert f["list_list_string"] == """[]"""
    assert f["list_list_binary"] == """[]"""
    assert f["list_map_string"] == """[]"""

    f = lyr.GetNextFeature()
    assert f["map_list_bool"] == """{"null":null}"""
    assert f["map_list_uint8"] == """{}"""
    assert f["map_list_int8"] == """{}"""
    assert f["map_list_uint16"] == """{}"""
    assert f["map_list_int16"] == """{}"""
    assert f["map_list_uint32"] == """{}"""
    assert f["map_list_int32"] == """{}"""
    assert f["map_list_uint64"] == """{}"""
    assert f["map_list_int64"] == """{}"""
    assert f["map_list_float32"] == """{}"""
    assert f["map_list_float64"] == """{}"""
    assert f["map_map_bool"] is None
    assert f["map_map_uint8"] is None
    assert f["map_map_int8"] is None
    assert f["map_map_uint16"] is None
    assert f["map_map_int16"] is None
    assert f["map_map_uint32"] is None
    assert f["map_map_int32"] is None
    assert f["map_map_uint64"] is None
    assert f["map_map_int64"] is None
    assert f["map_map_float32"] is None
    assert f["map_map_float64"] is None
    assert f["map_map_string"] is None
    assert f["list_list_string"] == """[]"""
    assert f["list_map_string"] == """[]"""


###############################################################################
#


def test_ogr_parquet_list_binary():

    # File generated by autotest/generate_parquet_test_file.py::generate_parquet_list_binary()
    ds = ogr.Open("data/parquet/list_binary.parquet")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["list_binary"] is None
    f = lyr.GetNextFeature()
    assert f["list_binary"] == [""]
    f = lyr.GetNextFeature()
    assert f["list_binary"] == ["foo", "bar", "base64:AQ=="]


###############################################################################
# Test float32 bounding box column


@pytest.mark.require_geos
def test_ogr_parquet_bbox_float32(tmp_vsimem):

    outfilename = str(tmp_vsimem / "test_ogr_parquet_bbox_float32.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbPolygon, options=["FID=fid", "ROW_GROUP_SIZE=2"]
    )
    fid = 0
    for wkt in ["LINESTRING(1 2,3 4)", None, "LINESTRING(-1 0,1 10)"]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        fid += 1
        if wkt:
            g = ogr.CreateGeometryFromWkt(wkt)
            f.SetGeometryDirectly(g)
        lyr.CreateFeature(f)
    ds = None

    def check_file(filename):
        with gdaltest.config_option("OGR_PARQUET_USE_BBOX", "NO"):
            ds = ogr.Open(filename)
            lyr = ds.GetLayer(0)
            assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0
            minx, maxx, miny, maxy = lyr.GetExtent()
            assert (minx, miny, maxx, maxy) == (-1.0, 0.0, 3.0, 10.0)
            f = lyr.GetNextFeature()
            assert f["geometry_bbox.xmin"] == 1
            assert f["geometry_bbox.ymin"] == 2
            assert f["geometry_bbox.xmax"] == 3
            assert f["geometry_bbox.ymax"] == 4
            f = lyr.GetNextFeature()
            assert f["geometry_bbox.xmin"] is None
            assert f["geometry_bbox.ymin"] is None
            assert f["geometry_bbox.xmax"] is None
            assert f["geometry_bbox.ymax"] is None
            f = lyr.GetNextFeature()
            assert f["geometry_bbox.xmin"] == -1
            assert f["geometry_bbox.ymin"] == 0
            assert f["geometry_bbox.xmax"] == 1
            assert f["geometry_bbox.ymax"] == 10
            ds = None

    check_file(outfilename)

    # Check that re-creating the bounding box column works with the Arrow
    # interface
    outfilename2 = str(tmp_vsimem / "test_ogr_parquet_bbox_float32_copy.parquet")
    gdal.VectorTranslate(outfilename2, outfilename)
    check_file(outfilename2)

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "geometry"
    assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
    minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == (-1.0, 0.0, 3.0, 10.0)

    with ogrtest.spatial_filter(lyr, 1, 2, 1, 2):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        assert lyr.GetNextFeature() is None

    # Test dfGroupMaxY < m_sFilterEnvelope.MinY
    with ogrtest.spatial_filter(lyr, 1, 10, 1, 10):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        assert lyr.GetNextFeature() is None

    with ogrtest.spatial_filter(lyr, -0.5, 0.5, 2, 9):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        assert lyr.GetNextFeature() is None

    # Test dfGroupMinX > m_sFilterEnvelope.MaxX
    with ogrtest.spatial_filter(lyr, -2, 0.5, 0, 9):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        assert lyr.GetNextFeature() is None

    # Test dfGroupMinX > m_sFilterEnvelope.MaxX
    with ogrtest.spatial_filter(lyr, -2, -2, 1, 1):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        assert lyr.GetNextFeature() is None

    # Test dfGroupMaxX < m_sFilterEnvelope.MinX
    with ogrtest.spatial_filter(lyr, 2, -2, 4, 5):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        assert lyr.GetNextFeature() is None
    ds = None


###############################################################################
# Test GetExtent() using bbox.minx, bbox.miny, bbox.maxx, bbox.maxy fields
# as in Overture Maps datasets


def test_ogr_parquet_bbox_double():

    ds = ogr.Open("data/parquet/overture_map_extract.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "geometry"
    assert lyr.GetLayerDefn().GetFieldIndex("bbox.minx") < 0
    assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
    minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (-36.831345, -10.049401, -36.831238, -10.049268)
    )

    with ogrtest.spatial_filter(
        lyr,
        minx + (maxx - minx) / 2,
        miny + (maxy - miny) / 2,
        maxx - (maxx - minx) / 2,
        maxy - (maxy - miny) / 2,
    ):
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        assert lyr.GetNextFeature() is None

    ds = None

    with gdaltest.config_option("OGR_PARQUET_USE_BBOX", "NO"):
        ds = ogr.Open("data/parquet/overture_map_extract.parquet")
        lyr = ds.GetLayer(0)
        assert lyr.GetGeometryColumn() == "geometry"
        assert lyr.GetLayerDefn().GetFieldIndex("bbox.minx") >= 0
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0
        minx, maxx, miny, maxy = lyr.GetExtent()
        assert (minx, miny, maxx, maxy) == pytest.approx(
            (-36.831345, -10.049401, -36.831238, -10.049268)
        )
        ds = None


###############################################################################
# Test GetExtent() using bbox.minx, bbox.miny, bbox.maxx, bbox.maxy fields
# as in Overture Maps datasets 2024-04-16-beta.0


@pytest.mark.require_geos
@pytest.mark.parametrize("use_dataset", [True, False])
def test_ogr_parquet_bbox_float32_but_no_covering_in_metadata(use_dataset):

    if use_dataset and not _has_arrow_dataset():
        pytest.skip("Test requires build with ArrowDataset")

    prefix = "PARQUET:" if use_dataset else ""

    ds = ogr.Open(
        prefix + "data/parquet/bbox_similar_to_overturemaps_2024-04-16-beta.0.parquet"
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "geometry"
    assert lyr.GetLayerDefn().GetFieldIndex("bbox.xmin") < 0
    if not use_dataset:
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
    minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (478315.53125, 4762880.5, 481645.3125, 4765610.5)
    )

    with ogrtest.spatial_filter(
        lyr,
        minx + (maxx - minx) / 2,
        miny + (maxy - miny) / 2,
        maxx - (maxx - minx) / 2,
        maxy - (maxy - miny) / 2,
    ):
        f = lyr.GetNextFeature()
        if not use_dataset:
            assert f.GetFID() == 8
        assert lyr.GetNextFeature() is None

    ds = None

    with gdaltest.config_option("OGR_PARQUET_USE_BBOX", "NO"):
        ds = ogr.Open(
            prefix
            + "data/parquet/bbox_similar_to_overturemaps_2024-04-16-beta.0.parquet"
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetGeometryColumn() == "geometry"
        assert lyr.GetLayerDefn().GetFieldIndex("bbox.xmin") >= 0
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0
        minx, maxx, miny, maxy = lyr.GetExtent()
        assert (minx, miny, maxx, maxy) == pytest.approx(
            (478315.53125, 4762880.5, 481645.3125, 4765610.5)
        )
        ds = None


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_curl
def test_ogr_parquet_overture_from_azure():

    if not _has_arrow_dataset():
        pytest.skip("Test requires build with ArrowDataset")

    url = "https://overturemapswestus2.blob.core.windows.net/release?comp=list&delimiter=%2F&prefix=2025-09-24.0%2Ftheme%3Ddivisions%2Ftype%3Ddivision_area%2F&restype=container"
    resp = gdaltest.gdalurlopen(url, timeout=5)
    if resp is None:
        pytest.skip(reason=f"{url} is down")
    resp = resp.read()
    if b"<Blobs />" in resp:
        print(resp)
        pytest.skip(
            reason=f"{url} dataset seems to be empty. Try upgrading to a newer release"
        )

    with ogr.Open(
        "PARQUET:/vsicurl/https://overturemapswestus2.blob.core.windows.net/release/2025-09-24.0/theme=divisions/type=division_area"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() > 0


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_parquet_write_arrow(tmp_vsimem):

    src_ds = ogr.Open("data/parquet/test.parquet")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "test_ogr_parquet_write_arrow.parquet")
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test", srs=src_lyr.GetSpatialRef(), geom_type=ogr.wkbPoint, options=[]
        )

        stream = src_lyr.GetArrowStream(["MAX_FEATURES_IN_BATCH=3"])
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() != src_lyr.GetGeometryColumn():
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    _check_test_parquet(outfilename)


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("Arrow")
def test_ogr_parquet_IsArrowSchemaSupported_float16(tmp_vsimem):

    src_ds = ogr.Open("data/arrow/test.feather")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(
        tmp_vsimem / "test_ogr_parquet_IsArrowSchemaSupported_float16.parquet"
    )
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test", srs=src_lyr.GetSpatialRef(), geom_type=ogr.wkbPoint, options=[]
        )

        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert not success
        assert error_msg == "float16 not supported"


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_parquet_write_arrow_rewind_polygon(tmp_vsimem):

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    src_lyr.CreateFeature(f)

    outfilename = str(tmp_vsimem / "test_ogr_parquet_write_arrow.parquet")
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer("test", geom_type=ogr.wkbPolygon, options=[])

        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() not in ("OGC_FID", "wkb_geometry"):
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,1 1,0 1,0 0))"


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "filename",
    [
        "data/parquet/poly_wkb_large_binary.parquet",
        "data/parquet/poly_wkt_large_string.parquet",
    ],
)
def test_ogr_parquet_read_large_binary_or_string_for_geometry(filename):
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

    if test_cli_utilities.get_test_ogrsf_path() is None:
        ret = gdaltest.runexternal(
            test_cli_utilities.get_test_ogrsf_path() + " -ro " + filename
        )

        assert "INFO" in ret
        assert "ERROR" not in ret


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_parquet_write_from_wkb_large_binary(tmp_vsimem):

    src_ds = ogr.Open("data/parquet/poly_wkb_large_binary.parquet")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(
        tmp_vsimem / "test_ogr_parquet_write_from_wkb_large_binary.parquet"
    )
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer("test", geom_type=ogr.wkbPolygon, options=[])

        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() not in ("geometry"):
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    ogrtest.compare_layers(src_lyr, lyr)


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("where", [None, "boolean = 0"])
def test_ogr_parquet_write_to_mem(tmp_vsimem, where):

    src_ds = gdal.OpenEx("data/parquet/test.parquet")
    ds = gdal.VectorTranslate("", src_ds, format="MEM", where=where)
    lyr = ds.GetLayer(0)
    if where is None:
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 1
        assert f["struct_field.c.d"] == "e"
        assert f["list_struct"] == '[{"a":1,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 2
        assert f["struct_field.c.d"] == "e1"
        assert f["list_struct"] == '[{"a":2,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 3
        assert f["struct_field.c.d"] == "e23"
        assert f["list_struct"] == '[{"a":3,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 4
        assert f["struct_field.c.d"] == "e345"
        assert f["list_struct"] == '[{"a":4,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 5
        assert f["struct_field.c.d"] == "e4567"
        assert f["list_struct"] == '[{"a":5,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
    else:
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 2
        assert f["struct_field.c.d"] == "e1"
        assert f["list_struct"] == '[{"a":2,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'
        f = lyr.GetNextFeature()
        assert f["struct_field.a"] == 4
        assert f["struct_field.c.d"] == "e345"
        assert f["list_struct"] == '[{"a":4,"b":2.5,"c":null},{"a":3,"b":null,"c":4.5}]'

    src_lyr = src_ds.GetLayer(0)
    if where:
        src_lyr.SetAttributeFilter(where)
    lyr.ResetReading()
    assert src_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    for i in range(lyr.GetFeatureCount()):
        src_f = src_lyr.GetNextFeature()
        f = lyr.GetNextFeature()
        for j in range(src_lyr.GetLayerDefn().GetFieldCount()):
            field_name = src_lyr.GetLayerDefn().GetFieldDefn(j).GetName()
            if field_name not in (
                "time64_us",
                "time64_ns",
                "dict",
            ) and "nan" not in str(src_f.GetField(j)):
                assert src_f.GetField(j) == f.GetField(j), field_name


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_parquet_metadata(tmp_vsimem):

    outfilename = str(tmp_vsimem / "test_ogr_parquet_metadata.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.SetMetadataItem("foo", "bar")
    lyr.SetMetadata(['{"foo":["bar","baz"]}'], "json:test")
    lyr.SetMetadata(["<foo/>"], "xml:test")
    ds = None

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata_Dict() == {"foo": "bar"}
    assert lyr.GetMetadata_List("json:test")[0] == '{"foo":["bar","baz"]}'
    assert lyr.GetMetadata_List("xml:test")[0] == "<foo/>"


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_parquet_get_extent_3d(tmp_vsimem):

    outfilename = str(tmp_vsimem / "test_ogr_parquet_get_extent_3d.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbLineString25D)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING Z (1 2 3,4 5 6)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.GetExtent3D() == (1.0, 4.0, 2.0, 5.0, 3.0, 6.0)

    with gdaltest.config_option("OGR_PARQUET_USE_BBOX", "NO"):
        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCFastGetExtent3D) == 0
        assert lyr.GetExtent3D() == (1.0, 4.0, 2.0, 5.0, 3.0, 6.0)


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("GPKG")
def test_ogr_parquet_sort_by_bbox(tmp_vsimem):

    outfilename = str(tmp_vsimem / "test_ogr_parquet_sort_by_bbox.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)

    gpkg_drv = gdal.GetDriverByName("GPKG")
    gpkg_drv.Deregister()
    ROW_GROUP_SIZE = 100
    try:
        with pytest.raises(
            Exception,
            match="Driver GPKG required for SORT_BY_BBOX layer creation option",
        ):
            ds.CreateLayer(
                "test",
                geom_type=ogr.wkbPoint,
                options=["SORT_BY_BBOX=YES", f"ROW_GROUP_SIZE={ROW_GROUP_SIZE}"],
            )
    finally:
        gpkg_drv.Register()

    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["SORT_BY_BBOX=YES", f"ROW_GROUP_SIZE={ROW_GROUP_SIZE}", "FID=fid"],
    )
    assert lyr.TestCapability(ogr.OLCFastWriteArrowBatch) == 0
    lyr.CreateField(ogr.FieldDefn("i", ogr.OFTInteger))
    COUNT_NON_SPATIAL = 501
    COUNT_SPATIAL = 601
    for i in range(COUNT_NON_SPATIAL):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["i"] = i
        lyr.CreateFeature(f)
    for i in range(COUNT_SPATIAL):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["i"] = i + COUNT_NON_SPATIAL
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(f"POINT({i} {i})"))
        lyr.CreateFeature(f)
    ds = None

    with gdaltest.config_option("OGR_PARQUET_SHOW_ROW_GROUP_EXTENT", "YES"):
        ds = ogr.Open(outfilename)
        lyr = ds.GetLayer(0)
        theorical_number_of_groups = (
            COUNT_SPATIAL + ROW_GROUP_SIZE - 1
        ) // ROW_GROUP_SIZE
        assert lyr.GetFeatureCount() - theorical_number_of_groups <= max(
            1, 0.3 * theorical_number_of_groups
        )
        assert sum([f["feature_count"] for f in lyr]) == COUNT_SPATIAL

    def check_file(filename):
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)

        # First features should be non spatial ones
        for i in range(COUNT_NON_SPATIAL):
            f = lyr.GetNextFeature()
            assert f.GetFID() == i
            assert f["i"] == i
            assert f.GetGeometryRef() is None

        # Now spatial features
        count = 0
        foundNonSequential = False
        set_i = set()
        while True:
            f = lyr.GetNextFeature()
            if not f:
                break
            assert f["i"] >= COUNT_NON_SPATIAL
            if f["i"] != i + COUNT_NON_SPATIAL:
                foundNonSequential = True
            assert f["i"] not in set_i
            set_i.add(f["i"])
            assert f.GetFID() == f["i"]
            assert f.GetGeometryRef().GetX() == f["i"] - COUNT_NON_SPATIAL
            count += 1

        assert count == COUNT_SPATIAL
        assert foundNonSequential

    check_file(outfilename)

    # Check that this works also when using the Arrow interface for creation
    outfilename2 = str(tmp_vsimem / "test_ogr_parquet_sort_by_bbox2.parquet")
    gdal.VectorTranslate(
        outfilename2,
        outfilename,
        layerCreationOptions=["SORT_BY_BBOX=YES", "ROW_GROUP_SIZE=100"],
    )
    check_file(outfilename2)


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("GPKG")
def test_ogr_parquet_sort_by_bbox__empty_layer(tmp_vsimem):
    """Test fix for https://github.com/OSGeo/gdal/issues/13328"""

    outfilename = str(tmp_vsimem / "test_ogr_parquet_sort_by_bbox_empty_layer.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(outfilename)

    ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["SORT_BY_BBOX=YES", "FID=fid"],
    )
    ds = None

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


###############################################################################
# Check GeoArrow struct encoding


@pytest.mark.parametrize(
    "wkt",
    [
        "POINT (1 2)",
        "POINT Z (1 2 3)",
        "LINESTRING (1 2,3 4)",
        "LINESTRING Z (1 2 3,4 5 6)",
        "POLYGON ((0 1,2 3,10 20,0 1))",
        "POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))",
        "POLYGON Z ((0 1 10,2 3 20,10 20 30,0 1 10))",
        "MULTIPOINT ((1 2),(3 4))",
        "MULTIPOINT Z ((1 2 3),(4 5 6))",
        "MULTILINESTRING ((1 2,3 4),(5 6,7 8,9 10))",
        "MULTILINESTRING Z ((1 2 3,4 5 6),(7 8 9,10 11 12,13 14 15))",
        "MULTIPOLYGON (((0 1,2 3,10 20,0 1)),((100 110,100 120,120 120,100 110)))",
        "MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((100 110,100 120,120 120,100 110)))",
        "MULTIPOLYGON Z (((0 1 10,2 3 20,10 20 30,0 1 10)))",
    ],
)
@pytest.mark.parametrize("check_with_pyarrow", [True, False])
@pytest.mark.parametrize(
    "covering_bbox,covering_bbox_name", [(True, None), (True, "bbox"), (False, None)]
)
@gdaltest.enable_exceptions()
def test_ogr_parquet_geoarrow(
    tmp_vsimem,
    tmp_path,
    wkt,
    check_with_pyarrow,
    covering_bbox,
    covering_bbox_name,
    with_arrow_dataset_or_not,
):

    geom = ogr.CreateGeometryFromWkt(wkt)

    if check_with_pyarrow:
        pa_parquet = pytest.importorskip("pyarrow.parquet")
        filename = str(tmp_path / "test_ogr_parquet_geoarrow.parquet")
    else:
        filename = str(tmp_vsimem / "test_ogr_parquet_geoarrow.parquet")

    ds = ogr.GetDriverByName("Parquet").CreateDataSource(filename)

    options = {
        "GEOMETRY_ENCODING": "GEOARROW",
        "WRITE_COVERING_BBOX": "YES" if covering_bbox else "NO",
    }
    if covering_bbox_name:
        options["COVERING_BBOX_NAME"] = covering_bbox_name
    lyr = ds.CreateLayer(
        "test",
        geom_type=geom.GetGeometryType(),
        options=options,
    )
    lyr.CreateField(ogr.FieldDefn("foo"))

    # Nominal geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    # Null geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    # Empty geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.Geometry(geom.GetGeometryType()))
    lyr.CreateFeature(f)

    # Nominal geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    geom2 = None
    if geom.GetGeometryCount() > 1:
        geom2 = geom.Clone()
        geom2.RemoveGeometry(1)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom2)
        lyr.CreateFeature(f)

    ds = None

    # Check we actually use a GeoArrow encoding
    if check_with_pyarrow:
        table = pa_parquet.read_table(filename)
        import pyarrow as pa

        if geom.GetGeometryType() in [ogr.wkbPoint, ogr.wkbPoint25D]:
            assert pa.types.is_struct(table.schema.field("geometry").type)
        else:
            assert pa.types.is_list(table.schema.field("geometry").type)

    _validate(filename)

    def check(lyr):
        assert lyr.GetGeomType() == geom.GetGeometryType()

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, geom)

        f = lyr.GetNextFeature()
        assert f.GetGeometryRef() is None

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, ogr.Geometry(geom.GetGeometryType()))

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, geom)

        if geom2:
            f = lyr.GetNextFeature()
            ogrtest.check_feature_geometry(f, geom2)

    filename_to_open = ("PARQUET:" if with_arrow_dataset_or_not else "") + filename

    ds = ogr.Open(filename_to_open)
    lyr = ds.GetLayer(0)
    check(lyr)

    if covering_bbox and not with_arrow_dataset_or_not:
        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        assert geo is not None
        j = json.loads(geo)
        assert j["columns"]["geometry"]["covering"]["bbox"]["xmin"][0] == (
            covering_bbox_name if covering_bbox_name else "geometry_bbox"
        )

    if (
        covering_bbox
        or not with_arrow_dataset_or_not
        or lyr.GetGeomType() in (ogr.wkbPoint, ogr.wkbPoint25D)
    ):
        assert lyr.TestCapability(ogr.OLCFastSpatialFilter)
    else:
        assert not lyr.TestCapability(ogr.OLCFastSpatialFilter)

    # Check that ignoring attribute fields doesn't impact geometry reading
    ds = ogr.Open(filename_to_open)
    lyr = ds.GetLayer(0)
    lyr.SetIgnoredFields(["foo"])
    check(lyr)
    lyr.SetSpatialFilter(geom)
    assert lyr.GetFeatureCount() == (3 if geom.GetGeometryCount() > 1 else 2)

    ds = ogr.Open(filename_to_open)
    lyr = ds.GetLayer(0)
    minx, maxx, miny, maxy = geom.GetEnvelope()

    lyr.SetSpatialFilter(geom)
    assert lyr.GetFeatureCount() == (3 if geom.GetGeometryCount() > 1 else 2)

    lyr.SetSpatialFilterRect(maxx + 1, miny, maxx + 2, maxy)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx, maxy + 1, maxx, maxy + 2)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx - 2, miny, minx - 1, maxy)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx, miny - 2, maxx, miny - 1)
    assert lyr.GetFeatureCount() == 0
    if (
        minx != miny
        and maxx != maxy
        and ogr.GT_Flatten(geom.GetGeometryType()) != ogr.wkbMultiPoint
    ):
        lyr.SetSpatialFilterRect(minx + 0.1, miny + 0.1, maxx - 0.1, maxy - 0.1)
        assert lyr.GetFeatureCount() != 0


###############################################################################
# Check GeoArrow fixed size list / interleaved encoding


@pytest.mark.parametrize(
    "wkt",
    [
        "POINT (1 2)",
        "POINT Z (1 2 3)",
        "LINESTRING (1 2,3 4)",
        "LINESTRING Z (1 2 3,4 5 6)",
        "POLYGON ((0 1,2 3,10 20,0 1))",
        "POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))",
        "POLYGON Z ((0 1 10,2 3 20,10 20 30,0 1 10))",
        "MULTIPOINT ((1 2),(3 4))",
        "MULTIPOINT Z ((1 2 3),(4 5 6))",
        "MULTILINESTRING ((1 2,3 4),(5 6,7 8,9 10))",
        "MULTILINESTRING Z ((1 2 3,4 5 6),(7 8 9,10 11 12,13 14 15))",
        "MULTIPOLYGON (((0 1,2 3,10 20,0 1)),((100 110,100 120,120 120,100 110)))",
        "MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((100 110,100 120,120 120,100 110)))",
        "MULTIPOLYGON Z (((0 1 10,2 3 20,10 20 30,0 1 10)))",
    ],
)
@pytest.mark.parametrize("covering_bbox", [True, False])
@gdaltest.enable_exceptions()
def test_ogr_parquet_geoarrow_fixed_size_list(tmp_vsimem, wkt, covering_bbox):

    geom = ogr.CreateGeometryFromWkt(wkt)

    filename = str(tmp_vsimem / "test_ogr_parquet_geoarrow_fixed_size_list.parquet")

    ds = ogr.GetDriverByName("Parquet").CreateDataSource(filename)

    lyr = ds.CreateLayer(
        "test",
        geom_type=geom.GetGeometryType(),
        options=[
            "GEOMETRY_ENCODING=GEOARROW_INTERLEAVED",
            "WRITE_COVERING_BBOX=" + ("YES" if covering_bbox else "NO"),
        ],
    )
    lyr.CreateField(ogr.FieldDefn("foo"))

    # Nominal geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    # Null geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    # Empty geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.Geometry(geom.GetGeometryType()))
    lyr.CreateFeature(f)

    # Nominal geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    geom2 = None
    if geom.GetGeometryCount() > 1:
        geom2 = geom.Clone()
        geom2.RemoveGeometry(1)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom2)
        lyr.CreateFeature(f)

    ds = None

    def check(lyr):
        assert lyr.GetGeomType() == geom.GetGeometryType()

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, geom)

        f = lyr.GetNextFeature()
        if geom.GetGeometryType() in (ogr.wkbPoint, ogr.wkbPoint25D):
            assert f.GetGeometryRef().IsEmpty()
        else:
            assert f.GetGeometryRef() is None

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, ogr.Geometry(geom.GetGeometryType()))

        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, geom)

        if geom2:
            f = lyr.GetNextFeature()
            ogrtest.check_feature_geometry(f, geom2)

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    check(lyr)

    # Check that ignoring attribute fields doesn't impact geometry reading
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    lyr.SetIgnoredFields(["foo"])
    check(lyr)

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    minx, maxx, miny, maxy = geom.GetEnvelope()

    lyr.SetSpatialFilter(geom)
    assert lyr.GetFeatureCount() == (3 if geom.GetGeometryCount() > 1 else 2)

    lyr.SetSpatialFilterRect(maxx + 1, miny, maxx + 2, maxy)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx, maxy + 1, maxx, maxy + 2)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx - 2, miny, minx - 1, maxy)
    assert lyr.GetFeatureCount() == 0

    lyr.SetSpatialFilterRect(minx, miny - 2, maxx, miny - 1)
    assert lyr.GetFeatureCount() == 0
    if (
        minx != miny
        and maxx != maxy
        and ogr.GT_Flatten(geom.GetGeometryType()) != ogr.wkbMultiPoint
    ):
        lyr.SetSpatialFilterRect(minx + 0.1, miny + 0.1, maxx - 0.1, maxy - 0.1)
        assert lyr.GetFeatureCount() != 0


###############################################################################
# Test reading a file with an extension on a regular field not registered with
# PyArrow


def test_ogr_parquet_read_with_extension_not_registered_on_regular_field():

    ds = ogr.Open("data/parquet/extension_custom.parquet")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["extension_custom"] == '{"foo":"bar"}'


###############################################################################
# Test reading a file with the arrow.json extension


def test_ogr_parquet_read_arrow_json_extension():

    ds = ogr.Open("data/parquet/extension_json.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = lyr.GetNextFeature()
    assert f["extension_json"] == '{"foo":"bar"}'


###############################################################################
# Test writing a file with the arrow.json extension


def test_ogr_parquet_writing_arrow_json_extension(tmp_vsimem):

    outfilename = str(tmp_vsimem / "out.parquet")
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as ds:
        lyr = ds.CreateLayer("test")
        fld_defn = ogr.FieldDefn("extension_json")
        fld_defn.SetSubType(ogr.OFSTJSON)
        lyr.CreateField(fld_defn)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["extension_json"] = '{"foo":"bar"}'
        lyr.CreateFeature(f)

    with gdal.config_option("OGR_PARQUET_READ_GDAL_SCHEMA", "NO"):
        ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = lyr.GetNextFeature()
    assert f["extension_json"] == '{"foo":"bar"}'


###############################################################################


@pytest.mark.parametrize("check_with_geoarrow_pyarrow", [False, True])
@gdaltest.enable_exceptions()
def test_ogr_parquet_read_geoarrow_without_geoparquet(check_with_geoarrow_pyarrow):

    if check_with_geoarrow_pyarrow:
        pytest.importorskip("geoarrow.pyarrow")

    ds = ogr.Open("data/parquet/poly_geoarrow_polygon_not_geoparquet.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "geometry"
    assert lyr.GetGeomType() == ogr.wkbPolygon


###############################################################################
# Test ignored fields with arrow::dataset and bounding box column


@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_ignored_fields_bounding_box_column_arrow_dataset(tmp_path):

    filename = str(tmp_path / "test.parquet")
    ds = ogr.GetDriverByName("Parquet").CreateDataSource(filename)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["FID=fid"])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)
    f = None
    ds.Close()

    ds = ogr.Open("PARQUET:" + filename)
    lyr = ds.GetLayer(0)
    lyr.SetIgnoredFields([lyr.GetGeometryColumn()])
    lyr.SetSpatialFilterRect(0, 0, 10, 10)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert f.GetGeometryRef() is None

    lyr.SetSpatialFilterRect(0, 0, 0, 0)
    lyr.ResetReading()
    assert lyr.GetNextFeature() is None


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("ARROW")
@pytest.mark.skipif(not _has_arrow_dataset(), reason="GDAL not built with ArrowDataset")
def test_ogr_parquet_vsi_arrow_file_system():

    version = int(
        ogr.GetDriverByName("ARROW").GetMetadataItem("ARROW_VERSION").split(".")[0]
    )
    if version < 16:
        pytest.skip("requires Arrow >= 16.0.0")

    ds = ogr.Open("PARQUET:gdalvsi://data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() > 0


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("ARROW")
@pytest.mark.parametrize(
    "src_filename,expected_error_msg",
    [
        ("data/arrow/stringview.feather", "StringView not supported"),
        ("data/arrow/binaryview.feather", "BinaryView not supported"),
    ],
)
def test_ogr_parquet_IsArrowSchemaSupported_arrow_15_types(
    src_filename, expected_error_msg, tmp_vsimem
):

    version = int(
        ogr.GetDriverByName("ARROW").GetMetadataItem("ARROW_VERSION").split(".")[0]
    )
    if version < 15:
        pytest.skip("requires Arrow >= 15.0.0")

    src_ds = ogr.Open(src_filename)
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "test.parquet")
    with ogr.GetDriverByName("Parquet").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test", srs=src_lyr.GetSpatialRef(), geom_type=ogr.wkbPoint, options=[]
        )

        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert not success
        assert error_msg == expected_error_msg


###############################################################################


def test_ogr_parquet_ogr2ogr_reprojection(tmp_vsimem):

    outfilename = str(tmp_vsimem / "test.parquet")
    gdal.VectorTranslate(
        outfilename,
        "data/parquet/poly.parquet",
        srcSRS="EPSG:32632",
        dstSRS="EPSG:4326",
    )
    with ogr.Open(outfilename) as ds:
        assert ds.GetLayer(0).GetExtent() == pytest.approx(
            (8.73380363499761, 8.774681944824946, 43.01833481785084, 43.04292637071279)
        )


###############################################################################
# Test DATETIME_AS_STRING=YES GetArrowStream() option


def test_ogr_parquet_arrow_stream_numpy_datetime_as_string(tmp_vsimem):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    with gdal.OpenEx(
        "data/parquet/test.parquet", gdal.OF_VECTOR, allowed_drivers=["Parquet"]
    ) as ds:
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsNumPy(
            options=["USE_MASKED_ARRAYS=NO", "DATETIME_AS_STRING=YES"]
        )
        batches = [batch for batch in stream]
        batch = batches[0]
        assert (
            batch["timestamp_ms_gmt_minus_0215"][0] == b"2019-01-01T14:00:00.500-02:15"
        )


###############################################################################


def _parquet_has_geo_types():
    drv = gdal.GetDriverByName("Parquet")
    return drv is not None and "USE_PARQUET_GEO_TYPES" in drv.GetMetadataItem(
        "DS_LAYER_CREATIONOPTIONLIST"
    )


###############################################################################
# Write file using Parquet geo types


@pytest.mark.skipif(
    not _parquet_has_geo_types(),
    reason="requires libarrow >= 21",
)
def test_ogr_parquet_write_use_geo_type(tmp_vsimem):

    with gdal.GetDriverByName("Parquet").CreateVector(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.CreateLayer(
            "test",
            srs=osr.SpatialReference(epsg=4326),
            options=["USE_PARQUET_GEO_TYPES=ONLY"],
        )
        lyr.SetMetadataItem("EDGES", "SPHERICAL")

    with gdal.OpenEx(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
        assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"

    with gdal.GetDriverByName("Parquet").CreateVector(tmp_vsimem / "out.parquet") as ds:
        ds.CreateLayer(
            "test",
            srs=osr.SpatialReference(epsg=32631),
            options=["USE_PARQUET_GEO_TYPES=ONLY"],
        )

    with gdal.OpenEx(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
        assert lyr.GetMetadataItem("EDGES") is None


###############################################################################
# Test files using Parquet GEOMETRY logical type introduced in libarrow 21


@pytest.mark.skipif(
    not _parquet_has_geo_types(),
    reason="requires libarrow >= 21",
)
@pytest.mark.parametrize(
    "filename,crs_name",
    [
        ("parquet_geometry/example-crs_vermont-4326.parquet", "WGS 84"),
        ("parquet_geometry/example-crs_vermont-crs84.parquet", "WGS 84"),
        ("parquet_geometry/example-crs_vermont-crs84-wkt2.parquet", "WGS 84"),
        ("parquet_geometry/example-crs_vermont-custom.parquet", "unknown"),
        ("parquet_geometry/example-crs_vermont-utm.parquet", "WGS 84 / UTM zone 18N"),
        ("parquet_geometry/example-crs_vermont-utm.parquet", "WGS 84 / UTM zone 18N"),
        ("parquet_testing_geospatial/crs-srid.parquet", "NAD83 / Conus Albers"),
        ("parquet_testing_geospatial/crs-projjson.parquet", "NAD83 / Conus Albers"),
        ("parquet_testing_geospatial/crs-geography.parquet", "WGS 84"),
    ],
)
def test_ogr_parquet_read_parquet_geometry(filename, crs_name):

    ds = ogr.Open("data/parquet/" + filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetName() == crs_name

    if "crs-geography" in filename:
        assert lyr.GetGeomType() == ogr.wkbUnknown
        assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0
        assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0
    else:
        assert lyr.GetGeomType() == ogr.wkbPolygon
        assert lyr.GetMetadataItem("EDGES") is None
        assert lyr.TestCapability(ogr.OLCFastGetExtent)
        assert lyr.TestCapability(ogr.OLCFastSpatialFilter)

    xmin, xmax, ymin, ymax = lyr.GetExtent()
    lyr.SetSpatialFilterRect(xmin + 1e-5, ymin + 1e-5, xmax - 1e-5, ymax - 1e-5)
    assert lyr.GetFeatureCount() == 1
    lyr.SetSpatialFilterRect(xmax + 1, ymin, xmax + 2, ymax)
    assert lyr.GetFeatureCount() == 0

    if _has_arrow_dataset():
        ds = ogr.Open("PARQUET:data/parquet/" + filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbUnknown
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0
        assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0
        assert lyr.GetSpatialRef().GetName() == crs_name

        if "crs-geography" in filename:
            assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"
        else:
            assert lyr.GetMetadataItem("EDGES") is None


###############################################################################
# Run test_ogrsf on files using Parquet GEOMETRY logical type introduced in libarrow 21


@pytest.mark.skipif(
    not _parquet_has_geo_types(),
    reason="requires libarrow >= 21",
)
def test_ogr_parquet_test_ogrsf_parquet_geometry():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro data/parquet/parquet_geometry/example-crs_vermont-utm.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################


def test_ogr_parquet_update(tmp_path):

    with gdal.GetDriverByName("PARQUET").CreateVector(tmp_path / "test.parquet") as ds:
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbPoint, srs=osr.SpatialReference(epsg=32631)
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        f["int"] = 123
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

    with ogr.Open(tmp_path / "test.parquet", gdal.GA_Update) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {}
        assert lyr.GetFIDColumn() == ""
        assert lyr.GetGeometryColumn() == "geometry"
        assert lyr.GetGeomType() == ogr.wkbPoint
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
        assert lyr.TestCapability(ogr.OLCSequentialWrite)
        assert lyr.TestCapability(ogr.OLCRandomWrite)
        assert lyr.TestCapability(ogr.OLCCreateField)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "bar"
        f["int"] = 456
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr.CreateFeature(f)
        assert lyr.GetFeatureCount() == 2

    with ogr.Open(tmp_path / "test.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {}
        assert lyr.GetFIDColumn() == ""
        assert lyr.GetGeometryColumn() == "geometry"
        assert lyr.GetGeomType() == ogr.wkbPoint
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        assert f["str"] == "foo"
        assert f["int"] == 123
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
        f = lyr.GetNextFeature()
        assert f["str"] == "bar"
        assert f["int"] == 456
        assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"

    with ogr.Open(tmp_path / "test.parquet", gdal.GA_Update) as ds:
        lyr = ds.GetLayer(0)
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        f = lyr.GetNextFeature()
        f["real"] = 1.5
        lyr.SetFeature(f)

    with ogr.Open(tmp_path / "test.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        assert f["str"] == "foo"
        assert f["int"] == 123
        assert f["real"] == 1.5
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(
            test_cli_utilities.get_test_ogrsf_path() + f" {tmp_path}/test.parquet"
        )

        assert "INFO" in ret
        assert "ERROR" not in ret


###############################################################################


def test_ogr_parquet_update_with_creation_options_implicit(tmp_path):

    with gdal.GetDriverByName("PARQUET").CreateVector(tmp_path / "test.parquet") as ds:
        lyr = ds.CreateLayer(
            "test",
            geom_type=ogr.wkbPoint,
            srs=osr.SpatialReference(epsg=32631),
            options=["FID=my_fid", "GEOMETRY_NAME=my_geom", "EDGES=SPHERICAL"],
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        f["int"] = 123
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

    with ogr.Open(tmp_path / "test.parquet", gdal.GA_Update) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {}
        assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"
        assert lyr.GetFIDColumn() == "my_fid"
        assert lyr.GetGeometryColumn() == "my_geom"
        assert lyr.GetGeomType() == ogr.wkbPoint
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "bar"
        f["int"] = 456
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr.CreateFeature(f)
        assert lyr.GetFeatureCount() == 2

    with ogr.Open(tmp_path / "test.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {}
        assert lyr.GetMetadataItem("EDGES") == "SPHERICAL"
        assert lyr.GetFIDColumn() == "my_fid"
        assert lyr.GetGeometryColumn() == "my_geom"
        assert lyr.GetGeomType() == ogr.wkbPoint
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        assert f["str"] == "foo"
        assert f["int"] == 123
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
        f = lyr.GetNextFeature()
        assert f["str"] == "bar"
        assert f["int"] == 456
        assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"


###############################################################################


def test_ogr_parquet_update_with_creation_options_explicit(tmp_path):

    with gdal.GetDriverByName("PARQUET").CreateVector(tmp_path / "test.parquet") as ds:
        lyr = ds.CreateLayer(
            "test",
            geom_type=ogr.wkbPoint,
            srs=osr.SpatialReference(epsg=32631),
            options=["ROW_GROUP_SIZE=1"],
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        f["int"] = 123
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

    with ogr.Open(tmp_path / "test.parquet", gdal.GA_Update) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {"ROW_GROUP_SIZE": "1"}
        assert lyr.GetMetadataItem("NUM_ROW_GROUPS", "_PARQUET_") == "1"
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "bar"
        f["int"] = 456
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr.CreateFeature(f)
        assert lyr.GetFeatureCount() == 2

    with ogr.Open(tmp_path / "test.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadata("_GDAL_CREATION_OPTIONS_") == {"ROW_GROUP_SIZE": "1"}
        assert lyr.GetMetadataItem("NUM_ROW_GROUPS", "_PARQUET_") == "2"
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        assert f["str"] == "foo"
        assert f["int"] == 123
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
        f = lyr.GetNextFeature()
        assert f["str"] == "bar"
        assert f["int"] == 456
        assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"


###############################################################################


def test_ogr_parquet_arrow_stream_list_of_struct_ignored_fields():
    pytest.importorskip("pyarrow")

    ds = ogr.Open("data/parquet/test_list_of_struct.parquet")
    lyr = ds.GetLayer(0)
    lyr.SetIgnoredFields(["OGR_GEOMETRY"])
    stream = lyr.GetArrowStreamAsPyArrow()
    batches = [batch for batch in stream]
    assert len(batches) == 1
    assert len(batches[0].field("col_flat")) == 3
    assert len(batches[0].field("col_nested")) == 3
    assert batches[0].field("col_flat")[0].as_py() == 0
    assert batches[0].field("col_flat")[1].as_py() == 1
    assert batches[0].field("col_flat")[2].as_py() == 2
    assert batches[0].field("col_nested")[0].as_py() == [
        {"a": 1, "b": 2},
        {"a": 1, "b": 2},
    ]
    assert batches[0].field("col_nested")[1].as_py() == [
        {"a": 1, "b": 2},
        {"a": 1, "b": 2},
    ]
    assert batches[0].field("col_nested")[2].as_py() == [
        {"a": 1, "b": 2},
        {"a": 1, "b": 2},
    ]


###############################################################################


def test_ogr_parquet_lists_as_string_json():

    ds = gdal.OpenEx(
        "data/parquet/test.parquet", open_options=["LISTS_AS_STRING_JSON=YES"]
    )
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("list_boolean")).GetType()
        == ogr.OFTString
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("list_boolean")).GetSubType()
        == ogr.OFSTJSON
    )
    assert (
        lyr_defn.GetFieldDefn(
            lyr_defn.GetFieldIndex("fixed_size_list_float64")
        ).GetType()
        == ogr.OFTString
    )
    assert (
        lyr_defn.GetFieldDefn(
            lyr_defn.GetFieldIndex("fixed_size_list_float64")
        ).GetSubType()
        == ogr.OFSTJSON
    )
    f = lyr.GetFeature(4)
    assert f["list_boolean"] == "[null,false,true,false]"
    assert f["list_uint8"] == "[null,7,8,9]"
    assert f["list_int64"] == "[null,7,8,9]"
    assert f["list_float64"] == "[null,7.5,8.5,9.5]"
    assert f["list_string"] == "[null]"
    assert f["fixed_size_list_float64"] == "[8.0,9.0]"


###############################################################################
# Test support for https://github.com/apache/arrow/blob/main/docs/source/format/CanonicalExtensions.rst#timestamp-with-offset


@pytest.mark.parametrize("OGR2OGR_USE_ARROW_API", ["YES", "NO"])
@pytest.mark.parametrize("TIMESTAMP_WITH_OFFSET", ["AUTO", "YES", "NO"])
def test_ogr_parquet_timestamp_with_offset(
    tmp_vsimem, OGR2OGR_USE_ARROW_API, TIMESTAMP_WITH_OFFSET
):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_MIXED_TZ)
    src_lyr.CreateField(fld_defn)
    f = ogr.Feature(src_lyr.GetLayerDefn())

    f["id"] = 1
    f["dt"] = "2025-12-20T12:34:56+0345"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 2
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 3
    f["dt"] = "2025-12-20T12:34:56-0745"
    src_lyr.CreateFeature(f)

    # Test without timezone
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 4
    f["dt"] = "2025-12-20T22:30:56"
    src_lyr.CreateFeature(f)

    with gdal.config_option(
        "OGR2OGR_USE_ARROW_API", OGR2OGR_USE_ARROW_API
    ), gdal.quiet_errors():
        gdal.VectorTranslate(
            tmp_vsimem / "out.parquet",
            src_ds,
            layerCreationOptions=["TIMESTAMP_WITH_OFFSET=" + TIMESTAMP_WITH_OFFSET],
        )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(1)
        assert fld_defn.GetType() == ogr.OFTDateTime
        if TIMESTAMP_WITH_OFFSET == "NO" and OGR2OGR_USE_ARROW_API == "NO":
            assert fld_defn.GetTZFlag() == ogr.TZFLAG_UNKNOWN
        else:
            assert fld_defn.GetTZFlag() == ogr.TZFLAG_MIXED_TZ

        f = lyr.GetNextFeature()
        assert f["id"] == 1
        if TIMESTAMP_WITH_OFFSET == "NO" and OGR2OGR_USE_ARROW_API == "NO":
            assert f["dt"] == "2025/12/20 08:49:56"
        else:
            assert f["dt"] == "2025/12/20 12:34:56+0345"

        f = lyr.GetNextFeature()
        assert f["id"] == 2
        assert f["dt"] is None

        f = lyr.GetNextFeature()
        assert f["id"] == 3
        if TIMESTAMP_WITH_OFFSET == "NO" and OGR2OGR_USE_ARROW_API == "NO":
            assert f["dt"] == "2025/12/20 20:19:56"
        else:
            assert f["dt"] == "2025/12/20 12:34:56-0745"

        f = lyr.GetNextFeature()
        assert f["id"] == 4
        if TIMESTAMP_WITH_OFFSET == "NO" and OGR2OGR_USE_ARROW_API == "NO":
            assert f["dt"] == "2025/12/20 22:30:56"
        else:
            assert f["dt"] == "2025/12/20 22:30:56+00"


###############################################################################


def test_ogr_parquet_create_metadata_file_alg(tmp_vsimem):

    assert gdal.alg.vsi.copy(
        source="data/parquet/partitioned_hive", destination=tmp_vsimem, recursive=True
    )
    assert gdal.VSIStatL(tmp_vsimem / "partitioned_hive/_metadata") is not None
    gdal.Unlink(tmp_vsimem / "partitioned_hive/_metadata")
    assert gdal.VSIStatL(tmp_vsimem / "partitioned_hive/_metadata") is None

    with pytest.raises(
        Exception,
        match=r"OpenStatic\(\) failed: cannot create i_do/not_exist/_metadata",
    ):
        gdal.alg.driver.parquet.create_metadata_file(
            input=[
                tmp_vsimem / "partitioned_hive/foo=bar/part.0.parquet",
                tmp_vsimem / "partitioned_hive/foo=baz/part.1.parquet",
            ],
            output="i_do/not_exist/_metadata",
        )

    with pytest.raises(Exception, match="Cannot infer relative path"):
        gdal.alg.driver.parquet.create_metadata_file(
            input=[
                tmp_vsimem / "partitioned_hive/foo=bar/part.0.parquet",
                tmp_vsimem / "partitioned_hive/foo=baz/part.1.parquet",
            ],
            output=tmp_vsimem / "unrelated/_metadata",
        )

    with pytest.raises(Exception, match=r"OpenInputFile\(\) failed"):
        gdal.alg.driver.parquet.create_metadata_file(
            input=[tmp_vsimem / "partitioned_hive/foo=bar/i_do_not_exist.parquet"],
            output=tmp_vsimem / "partitioned_hive/_metadata",
        )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "partitioned_hive/not_a_parquet_file.bin", b"foo"
    )

    with pytest.raises(Exception, match=r"Invalid: Parquet file"):
        gdal.alg.driver.parquet.create_metadata_file(
            input=[tmp_vsimem / "partitioned_hive/not_a_parquet_file.bin"],
            output=tmp_vsimem / "partitioned_hive/_metadata",
            overwrite=True,
        )

    assert gdal.alg.driver.parquet.create_metadata_file(
        input=[
            tmp_vsimem / "partitioned_hive/foo=bar/part.0.parquet",
            tmp_vsimem / "partitioned_hive/foo=baz/part.1.parquet",
        ],
        output=tmp_vsimem / "partitioned_hive/_metadata",
        overwrite=True,
    )

    assert ogr.Open(tmp_vsimem / "partitioned_hive/_metadata")

    gdal.Unlink(tmp_vsimem / "partitioned_hive/_metadata")
    assert gdal.VSIStatL(tmp_vsimem / "partitioned_hive/_metadata") is None

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert gdal.alg.driver.parquet.create_metadata_file(
        input=[
            tmp_vsimem / "partitioned_hive/foo=bar/part.0.parquet",
            tmp_vsimem / "partitioned_hive/foo=baz/part.1.parquet",
        ],
        output=tmp_vsimem / "partitioned_hive/_metadata",
        progress=my_progress,
    )

    assert tab_pct[0] == 1.0

    assert ogr.Open(tmp_vsimem / "partitioned_hive/_metadata")

    if _has_arrow_dataset():

        ds = ogr.Open(tmp_vsimem / "partitioned_hive")

        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == [str(tmp_vsimem / "partitioned_hive/_metadata")]

        lyr = ds.GetLayer(0)
        lyr.SetAttributeFilter("foo = 'baz'")
        assert lyr.GetFeatureCount() == 3

        with ds.ExecuteSQL(
            "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
        ) as sql_lyr:
            set_files = [f.GetField(0) for f in sql_lyr]
            assert set_files == [
                str(tmp_vsimem / "partitioned_hive/foo=baz/part.1.parquet")
            ]


###############################################################################


def test_ogr_parquet_create_metadata_file_alg_incompatible_schemas(tmp_vsimem):

    with gdal.GetDriverByName("PARQUET").CreateVector(tmp_vsimem / "one.parquet") as ds:
        lyr = ds.CreateLayer("test")
        lyr.CreateField(ogr.FieldDefn("foo"))

    with gdal.GetDriverByName("PARQUET").CreateVector(tmp_vsimem / "two.parquet") as ds:
        lyr = ds.CreateLayer("test")
        lyr.CreateField(ogr.FieldDefn("bar"))

    with pytest.raises(
        Exception, match="Parquet exception: AppendRowGroups requires equal schemas"
    ):
        gdal.alg.driver.parquet.create_metadata_file(
            input=[tmp_vsimem / "one.parquet", tmp_vsimem / "two.parquet"],
            output=tmp_vsimem / "_metadata",
        )
