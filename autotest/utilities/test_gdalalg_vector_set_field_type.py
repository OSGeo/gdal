#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector set-field-type' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr


def get_set_field_type_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["set-field-type"]


@pytest.mark.parametrize(
    "src_type,src_subtype,dest_type_str,src_value,expected_value",
    [
        (ogr.OFTString, ogr.OFSTNone, "Integer", "123", 123),
        (ogr.OFTBinary, ogr.OFSTNone, "String", b"foo", "666F6F"),
        (ogr.OFTString, ogr.OFSTNone, "Real", "123.456", 123.456),
        (ogr.OFTString, ogr.OFSTNone, "String", "foo", "foo"),
        (ogr.OFTInteger, ogr.OFSTNone, "String", 123, "123"),
        (ogr.OFTInteger, ogr.OFSTNone, "Real", 123, 123.0),
        (ogr.OFTInteger, ogr.OFSTNone, "Integer", 123, 123),
        (ogr.OFTReal, ogr.OFSTNone, "String", 123.456, "123.456"),
        (ogr.OFTReal, ogr.OFSTNone, "Integer", 123.456, 123),
        (ogr.OFTReal, ogr.OFSTNone, "Real", 123.456, 123.456),
        (ogr.OFTString, ogr.OFSTNone, "Date", "2024/06/01", "2024/06/01"),
        (
            ogr.OFTString,
            ogr.OFSTNone,
            "DateTime",
            "2024/06/01 12:34:56",
            "2024/06/01 12:34:56",
        ),
        (ogr.OFTString, ogr.OFSTNone, "Time", "12:34:56", "12:34:56"),
        (ogr.OFTString, ogr.OFSTNone, "Integer64", "1234567890123", 1234567890123),
        # Test JSON
        (
            ogr.OFTString,
            ogr.OFSTJSON,
            "StringList",
            '["foo","bar","baz"]',
            ["foo", "bar", "baz"],
        ),
        (ogr.OFTString, ogr.OFSTJSON, "IntegerList", "[1,2,3]", [1, 2, 3]),
        (ogr.OFTString, ogr.OFSTJSON, "RealList", "[1.1,2.2,3.3]", [1.1, 2.2, 3.3]),
        (
            ogr.OFTString,
            ogr.OFSTJSON,
            "Integer64List",
            "[1234567890123,2345678901234]",
            [1234567890123, 2345678901234],
        ),
        # Test list types
        (
            ogr.OFTStringList,
            ogr.OFSTNone,
            "String",
            '["foo","bar","baz"]',
            "(3:foo,bar,baz)",
        ),
        (ogr.OFTIntegerList, ogr.OFSTNone, "String", "[1,2,3]", "(3:1,2,3)"),
        (ogr.OFTRealList, ogr.OFSTNone, "String", "[1.1,2.2,3.3]", "(3:1.1,2.2,3.3)"),
        # Test destination subtype
        (
            ogr.OFTStringList,
            ogr.OFSTNone,
            "JSON",
            '["foo","bar","baz"]',
            '[ "foo", "bar", "baz" ]',
        ),
        (
            ogr.OFTString,
            ogr.OFSTNone,
            "UUID",
            "550e8400-e29b-41d4-a716-446655440000",
            "550e8400-e29b-41d4-a716-446655440000",
        ),
        (ogr.OFTString, ogr.OFSTNone, "boolean", "true", True),
        (ogr.OFTString, ogr.OFSTNone, "float32", "1.5", 1.5),
        (ogr.OFTString, ogr.OFSTNone, "int16", "123", 123),
        # Test source subtype
        (ogr.OFTInteger, ogr.OFSTInt16, "integer", -32768, -32768),
        (ogr.OFTInteger, ogr.OFSTBoolean, "integer", True, 1),
        (ogr.OFTInteger, ogr.OFSTBoolean, "integer", False, 0),
    ],
)
def test_gdalalg_vector_set_field_type(
    src_type, src_subtype, dest_type_str, src_value, expected_value
):

    src_ds = gdal.GetDriverByName("MEM").CreateDataSource("")
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbPoint)
    fld_defn = ogr.FieldDefn("value", src_type)
    fld_defn.SetSubType(src_subtype)
    lyr.CreateField(fld_defn)

    # Add another field
    fld_defn2 = ogr.FieldDefn("new_type", ogr.OFTString)
    lyr.CreateField(fld_defn2)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    f["value"] = src_value
    f["new_type"] = dest_type_str
    lyr.CreateFeature(f)

    alg = get_set_field_type_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        [
            "--field-name",
            "value",
            "--field-type",
            dest_type_str,
            "--of",
            "MEM",
            "--output",
            "memory_ds",
        ]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    # Check field type and subtype
    out_field_defn = out_lyr.GetLayerDefn().GetFieldDefn(0)
    expected_type = ogr.GetFieldTypeByName(dest_type_str)

    if expected_type == ogr.OFTString and dest_type_str.lower() != "string":
        # It must be a subtype
        expected_subtype = ogr.GetFieldSubtypeByName(dest_type_str)
        assert out_field_defn.GetSubType() == expected_subtype
    else:
        # It must be a type
        assert out_field_defn.GetType() == expected_type
        assert out_field_defn.GetSubType() == ogr.OFSTNone

    assert out_f["value"] == expected_value
    assert out_f["new_type"] == dest_type_str

    # Check geometry
    geom = out_f.GetGeometryRef()
    assert geom is not None and geom.ExportToWkt() == "POINT (1 2)"


def test_gdalalg_vector_set_field_type_errors():
    src_ds = gdal.GetDriverByName("MEM").CreateDataSource("")
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("value", ogr.OFTString)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["value"] = "foo"
    lyr.CreateFeature(f)

    # Wrong field name
    alg = get_set_field_type_alg()
    alg["input"] = src_ds

    with pytest.raises(
        Exception, match="Cannot find field 'non_existing_field' in layer 'layer'"
    ):
        alg.ParseCommandLineArguments(
            [
                "--field-name",
                "non_existing_field",
                "--field-type",
                "Integer",
                "--of",
                "MEM",
                "--output",
                "memory_ds",
            ]
        )

    # Wrong field type
    alg = get_set_field_type_alg()
    alg["input"] = src_ds
    with pytest.raises(
        Exception,
        match="set-field-type: Invalid value for argument 'field-type': 'NonExistingType'",
    ):
        alg.ParseCommandLineArguments(
            [
                "--field-name",
                "value",
                "--field-type",
                "NonExistingType",
                "--of",
                "MEM",
                "--output",
                "memory_ds",
            ]
        )

    # No input
    alg = get_set_field_type_alg()
    with pytest.raises(
        Exception,
        match="Positional arguments starting at 'INPUT' have not been specified.",
    ):
        alg.ParseCommandLineArguments(
            [
                "--field-name",
                "value",
                "--field-type",
                "Integer",
                "--of",
                "MEM",
                "--output",
                "memory_ds",
            ]
        )

    # No output
    alg = get_set_field_type_alg()
    alg["input"] = src_ds
    with pytest.raises(
        Exception,
        match="Positional arguments starting at 'OUTPUT' have not been specified.",
    ):
        alg.ParseCommandLineArguments(
            [
                "--field-name",
                "value",
                "--field-type",
                "Integer",
                "--of",
                "MEM",
            ]
        )


def test_gdalalg_set_field_type_in_pipeline():

    with gdal.Run(
        "pipeline",
        pipeline="read ../ogr/data/poly.shp ! set-field-type --field-name EAS_ID --field-type Integer ! write --output-format stream stream",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["EAS_ID"] == 168

    with pytest.raises(
        Exception, match="Cannot find field 'invalid_field' in layer 'poly'"
    ):
        gdal.Run(
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! set-field-type --field-name invalid_field --field-type Integer ! write --output-format stream stream",
        )


def test_gdalalg_set_field_type_completion(tmp_path):

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector set-field-type --field-type"
    )
    assert "Binary" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector set-field-type --input ../ogr/data/poly.shp --field-name"
    )
    assert "EAS_ID" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline read ../ogr/data/poly.shp ! set-field-type --field-name "
    )
    assert "EAS_ID" in out

    # No completion when there is a tee operator as this can be a slow operation
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline read ../ogr/data/poly.shp ! tee [ write /vsimem/out.gpkg ] ! set-field-type --field-name "
    )
    assert "EAS_ID" not in out

    # Or with contour
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline read ../gcore/data/byte.tif ! contour --interval 10 ! set-field-type --field-name "
    )
    assert "EAS_ID" not in out


@pytest.mark.require_driver("GPKG")
def test_gdalalg_set_field_type_multiple_layers(tmp_vsimem, tmp_path):

    # Create a GPKG with multiple layers
    in_filename = str(
        tmp_vsimem / "test_gdalalg_set_field_type_multiple_layers_in.gpkg"
    )
    out_filename = str(
        tmp_path / "test_gdalalg_set_field_type_multiple_layers_out.gpkg"
    )
    src_ds = gdal.GetDriverByName("GPKG").CreateDataSource(in_filename)

    # layer 1
    lyr = src_ds.CreateLayer("layer1", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("test_field", ogr.OFTString)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["test_field"] = "123"
    lyr.CreateFeature(f)
    lyr = None

    # layer 2
    lyr = src_ds.CreateLayer("layer2", geom_type=ogr.wkbNone)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["test_field"] = "456"
    lyr.CreateFeature(f)
    lyr = None

    alg = get_set_field_type_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        [
            "--field-name",
            "test_field",
            "--field-type",
            "Integer",
            "--output",
            out_filename,
        ]
    )
    assert alg.Run()

    # Verify
    with gdal.OpenEx(out_filename, gdal.OF_VECTOR) as out_ds:
        assert out_ds.GetLayerCount() == 2
        assert out_ds.GetLayer(0).GetFeatureCount() == 1
        assert out_ds.GetLayer(1).GetFeatureCount() == 1
        assert (
            out_ds.GetLayer(0).GetLayerDefn().GetFieldDefn(0).GetType()
            == ogr.OFTInteger
        )
        assert (
            out_ds.GetLayer(1).GetLayerDefn().GetFieldDefn(0).GetType()
            == ogr.OFTInteger
        )
        assert (
            out_ds.GetLayer(0).GetLayerDefn().GetFieldDefn(0).GetSubType()
            == ogr.OFSTNone
        )
        assert (
            out_ds.GetLayer(1).GetLayerDefn().GetFieldDefn(0).GetSubType()
            == ogr.OFSTNone
        )
        f = out_ds.GetLayer(0).GetNextFeature()
        assert f["test_field"] == 123
        f = out_ds.GetLayer(1).GetNextFeature()
        assert f["test_field"] == 456

    # Test with --layer
    alg = get_set_field_type_alg()
    alg["input"] = src_ds
    assert alg.ParseCommandLineArguments(
        [
            "--overwrite",
            "--field-name",
            "test_field",
            "--field-type",
            "Integer",
            "--layer",
            "layer2",
            "--output",
            out_filename,
        ]
    )
    assert alg.Run()
    src_ds = None

    # Verify
    with gdal.OpenEx(out_filename, gdal.OF_VECTOR) as out_ds:
        assert out_ds.GetLayerCount() == 1
        assert out_ds.GetLayer(0).GetFeatureCount() == 1
        assert (
            out_ds.GetLayer(0).GetLayerDefn().GetFieldDefn(0).GetType()
            == ogr.OFTInteger
        )
        assert (
            out_ds.GetLayer(0).GetLayerDefn().GetFieldDefn(0).GetSubType()
            == ogr.OFSTNone
        )
        f = out_ds.GetLayer(0).GetNextFeature()
        assert f["test_field"] == 456


def test_gdalalg_set_field_type_src_field_type():

    src_ds = gdal.GetDriverByName("MEM").CreateDataSource("")
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("string1", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("string2", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["string1"] = "123"
    f["string2"] = "456"
    f["real"] = 1.25
    lyr.CreateFeature(f)

    alg = gdal.Run(
        "vector",
        "set-field-type",
        input=src_ds,
        src_field_type="String",
        dst_field_type="Integer",
        output_format="MEM",
    )
    ds = alg.Output()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["string1"] == 123
    assert f["string2"] == 456
    assert f["real"] == 1.25


@pytest.mark.require_driver("CSV")
def test_gdalalg_set_field_type_pipeline_fuse_in_open(tmp_vsimem):

    with gdal.VSIFile(tmp_vsimem / "test.csv", "wb") as f:
        f.write(b"str,int,int2\n")
        f.write(b"2025-09-30,1,2\n")
    with gdal.VSIFile(tmp_vsimem / "test.csvt", "wb") as f:
        f.write(b"String,Integer,Integer\n")

    with gdal.Run(
        "pipeline",
        pipeline=f"read {tmp_vsimem}/test.csv ! set-field-type --src-field-type String --field-type Date ! set-field-type --field-name int --field-type Integer64 ! write --format MEM --output whatever",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldDefn(0).GetType() == ogr.OFTDate
        assert lyr_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger64
        assert lyr_defn.GetFieldDefn(2).GetType() == ogr.OFTInteger


def test_gdalalg_set_field_type_pipeline_fuse_in_open_not_supported(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.dbf"
    ) as ds:
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int2", ogr.OFTInteger))

    with gdal.Run(
        "pipeline",
        pipeline=f"read {tmp_vsimem}/test.dbf ! set-field-type --src-field-type String --field-type Date ! set-field-type --field-name int --field-type Integer64 ! write --format MEM --output whatever",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldDefn(0).GetType() == ogr.OFTDate
        assert lyr_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger64
        assert lyr_defn.GetFieldDefn(2).GetType() == ogr.OFTInteger
