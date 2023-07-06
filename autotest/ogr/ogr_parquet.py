#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for OGR Parquet driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import json
import math

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("Parquet")

PARQUET_JSON_SCHEMA = "data/parquet/schema.json"


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
        filename, check_data=check_data, local_schema=PARQUET_JSON_SCHEMA
    )
    assert not ret


###############################################################################
# Read invalid file


def test_ogr_parquet_invalid():

    with pytest.raises(Exception):
        ogr.Open("data/parquet/invalid.parquet")


###############################################################################
# Basic tests


def _check_test_parquet(
    filename,
    expect_fast_feature_count=True,
    expect_fast_get_extent=True,
    expect_ignore_fields=True,
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
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert lyr_defn.GetFieldCount() == 71
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
        ("list_string", "StringList", "None", 0, 0),
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
        ("map_string", "String", "JSON", 0, 0),
        ("dict", "Integer", "None", 0, 0),
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
    with pytest.raises(Exception):
        lyr.GetExtent(geom_field=-1)
    with pytest.raises(Exception):
        lyr.GetExtent(geom_field=1)

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
    assert f["timestamp_ms_gmt"] == "2019/01/01 14:00:00+00"
    assert f["timestamp_ms_gmt_plus_2"] == "2019/01/01 14:00:00+02"
    assert f["timestamp_ms_gmt_minus_0215"] == "2019/01/01 14:00:00-0215"
    assert f["timestamp_s_no_tz"] == "2019/01/01 14:00:00"
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
    assert f["list_string"] is None
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
    assert f["map_boolean"] == '{"x":null,"y":true}'
    assert f["map_uint8"] == '{"x":1,"y":null}'
    assert f["map_int8"] == '{"x":1,"y":null}'
    assert f["map_uint16"] == '{"x":1,"y":null}'
    assert f["map_int16"] == '{"x":1,"y":null}'
    assert f["map_uint32"] == '{"x":4000000000,"y":null}'
    assert f["map_int32"] == '{"x":2000000000,"y":null}'
    assert f["map_uint64"] == '{"x":4000000000000.0,"y":null}'
    assert f["map_int64"] == '{"x":-2000000000000,"y":null}'
    assert f["map_float32"] == '{"x":1.5,"y":null}'
    assert f["map_float64"] == '{"x":1.5,"y":null}'
    assert f["map_string"] == '{"x":"x_val","y":null}'
    assert f["dict"] == 0
    assert f.GetGeometryRef().ExportToWkt() == "POINT (0 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert not f["boolean"]
    assert f["uint8"] == 2
    assert f.GetGeometryRef() is None

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    assert f["uint8"] is None
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 3
    assert f["uint8"] == 4
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
    assert f.GetFID() == 4

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
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_test():
    import test_cli_utilities

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
    import test_cli_utilities

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
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/parquet/all_geoms.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test write support


@pytest.mark.parametrize(
    "use_vsi,row_group_size,fid", [(False, None, None), (True, 2, "fid")]
)
def test_ogr_parquet_write_from_another_dataset(use_vsi, row_group_size, fid):

    outfilename = "/vsimem/out.parquet" if use_vsi else "tmp/out.parquet"
    try:
        layerCreationOptions = []
        if row_group_size:
            layerCreationOptions.append("ROW_GROUP_SIZE=" + str(row_group_size))
        if fid:
            layerCreationOptions.append("FID=" + fid)
        gdal.VectorTranslate(
            outfilename,
            "data/parquet/test.parquet",
            layerCreationOptions=layerCreationOptions,
        )

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)

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
        assert geo is not None
        j = json.loads(geo)
        assert j is not None
        assert "version" in j
        assert j["version"] == "1.0.0-beta.1"
        assert "primary_column" in j
        assert j["primary_column"] == "geometry"
        assert "columns" in j
        assert "geometry" in j["columns"]
        assert "encoding" in j["columns"]["geometry"]
        assert j["columns"]["geometry"]["encoding"] == "WKB"

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
    assert ds.AddFieldDomain(domain) == False
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
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    f["foo"] = "bar"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    assert lyr.TestCapability(ogr.OLCCreateField) == 0
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 0
    with pytest.raises(Exception):
        assert lyr.CreateField(ogr.FieldDefn("bar")) != ogr.OGRERR_NONE
    with pytest.raises(Exception):
        assert (
            lyr.CreateGeomField(ogr.GeomFieldDefn("baz", ogr.wkbPoint))
            != ogr.OGRERR_NONE
        )
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
            "2019/01/01 14:00:00+00",
            "2019/01/01 14:00:00+00",
        ),
    ]

    sql = ""
    for field in expected_fields:
        name = field[0]
        if sql:
            sql += ", "
        else:
            sql = "SELECT "
        sql += "MIN(" + name + "), MAX(" + name + ")"
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
            assert fld_defn.GetName() == "MAX_" + name
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
        assert ds.ExecuteSQL("SELECT MIN(int32) FROM i_dont_exist") is None
    with pytest.raises(Exception):
        assert ds.ExecuteSQL("SELECT MIN(i_dont_exist) FROM test") is None

    # File without statistics
    outfilename = "/vsimem/out.parquet"
    try:
        gdal.VectorTranslate(
            outfilename, "data/parquet/test.parquet", options="-lco STATISTICS=NO"
        )
        ds = ogr.Open(outfilename)
        with pytest.raises(
            Exception,
            match=r".*Use of field function MIN\(\) on string field string illegal.*",
        ):
            # Generic OGR SQL doesn't support MIN() on string field
            ds.ExecuteSQL("SELECT MIN(string) FROM out")
        ds = None

    finally:
        gdal.Unlink(outfilename)


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
        # not optimized
        "boolean = 0 OR boolean = 1",
        "1 = 1",
        "boolean = boolean",
    ],
)
def test_ogr_parquet_attribute_filter(filter):

    with gdaltest.config_option("OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "NO"):
        ds = ogr.Open("data/parquet/test.parquet")
        lyr = ds.GetLayer(0)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        ds = None

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


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


def test_ogr_parquet_attribute_filter_and_spatial_filter():

    filter = "int8 != 0"

    with gdaltest.config_option("OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "NO"):
        ds = ogr.Open("data/parquet/test.parquet")
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(4, 2, 4, 2)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        assert ref_fc > 0
        ds = None

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(4, 2, 4, 2)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


###############################################################################


def _has_arrow_dataset():
    drv = gdal.GetDriverByName("Parquet")
    return drv is not None and drv.GetMetadataItem("ARROW_DATASET") is not None


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

    lyr.SetSpatialFilterRect(0, 0, 10, 10)
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 2

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
    pytest.importorskip("osgeo.gdal_array")
    numpy = pytest.importorskip("numpy")

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(
        options=["MAX_FEATURES_IN_BATCH=5", "USE_MASKED_ARRAYS=NO"]
    )
    with gdaltest.error_handler():
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
        batch["fixed_size_list_boolean"][0], numpy.array([True, False])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_boolean"][0], numpy.array([True, False])
    )
    assert numpy.array_equal(
        batches[1]["fixed_size_list_boolean"][1], numpy.array([False, True])
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
    assert numpy.array_equal(
        batches[1]["list_string"][1], numpy.array([b"A", b"BC", b"CDE", b"DEFG"])
    )

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


def test_ogr_parquet_arrow_stream_numpy_fast_spatial_filter():
    pytest.importorskip("osgeo.gdal_array")
    numpy = pytest.importorskip("numpy")
    import datetime

    ds = ogr.Open("data/parquet/test.parquet")
    lyr = ds.GetLayer(0)
    ignored_fields = ["decimal128", "decimal256", "time64_ns"]
    lyr_defn = lyr.GetLayerDefn()
    for i in range(lyr_defn.GetFieldCount()):
        fld_defn = lyr_defn.GetFieldDefn(i)
        if (
            fld_defn.GetName().startswith("map_")
            or fld_defn.GetName().startswith("struct_")
            or fld_defn.GetName().startswith("fixed_size_")
            or fld_defn.GetType()
            not in (
                ogr.OFTInteger,
                ogr.OFTInteger64,
                ogr.OFTReal,
                ogr.OFTString,
                ogr.OFTBinary,
                ogr.OFTTime,
                ogr.OFTDate,
                ogr.OFTDateTime,
            )
        ):
            ignored_fields.append(fld_defn.GetNameRef())
    lyr.SetIgnoredFields(ignored_fields)
    lyr.SetSpatialFilterRect(-10, -10, 10, 10)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fc = 0
    for batch in stream:
        fc += len(batch["uint8"])
    assert fc == 4

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
    assert batch["timestamp_ms_gmt"][0] == numpy.datetime64("2019-01-01T14:00:00.000")
    assert batch["time32_s"][0] == datetime.time(0, 0, 4)
    assert batch["time32_ms"][0] == datetime.time(0, 0, 0, 4000)
    assert batch["time64_us"][0] == datetime.time(0, 0, 0, 4)
    assert batch["date32"][0] == numpy.datetime64("1970-01-05")
    assert batch["date64"][0] == numpy.datetime64("1970-01-01")
    assert bytes(batch["binary"][0]) == b"\00\01"
    assert bytes(batch["large_binary"][0]) == b"\00\01"
    assert (
        ogr.CreateGeometryFromWkb(batch["geometry"][0]).ExportToWkt() == "POINT (3 2)"
    )

    lyr.SetSpatialFilterRect(1, 1, 1, 1)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 0


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
    pytest.importorskip("osgeo.gdal_array")
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
    pytest.importorskip("osgeo.gdal_array")
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
            lyr = ds.CreateLayer("test")
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
            f.SetFieldBinaryFromHexString(
                geom_col_name, "".join("%02X" % x for x in wkb)
            )
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
