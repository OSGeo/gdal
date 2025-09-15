#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector change-field-type' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal, ogr


def get_change_field_type_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["change-field-type"]


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
def test_gdalalg_vector_change_field_type(
    src_type, src_subtype, dest_type_str, src_value, expected_value
):

    src_ds = gdal.GetDriverByName("MEM").CreateDataSource("")
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("value", src_type)
    fld_defn.SetSubType(src_subtype)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["value"] = src_value
    lyr.CreateFeature(f)

    alg = get_change_field_type_alg()
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


def test_gdalalg_vector_change_field_type_errors():
    src_ds = gdal.GetDriverByName("MEM").CreateDataSource("")
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("value", ogr.OFTString)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["value"] = "foo"
    lyr.CreateFeature(f)

    # Wrong field name
    alg = get_change_field_type_alg()
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
    alg = get_change_field_type_alg()
    alg["input"] = src_ds
    with pytest.raises(
        Exception,
        match="change-field-type: Invalid value for argument 'field-type': 'NonExistingType'",
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
