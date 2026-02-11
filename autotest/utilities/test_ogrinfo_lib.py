#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal.VectorInfo() testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import pathlib

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Simple test


def test_ogrinfo_lib_1():

    ds = gdal.OpenEx("../ogr/data/poly.shp")

    ret = gdal.VectorInfo(ds)
    assert "ESRI Shapefile" in ret


def test_ogrinfo_lib_1_str():

    ret = gdal.VectorInfo("../ogr/data/poly.shp")
    assert "ESRI Shapefile" in ret


def test_ogrinfo_lib_1_path():

    ret = gdal.VectorInfo(pathlib.Path("../ogr/data/poly.shp"))
    assert "ESRI Shapefile" in ret


###############################################################################
# Test json output


def test_ogrinfo_lib_json():

    ds = gdal.OpenEx("../ogr/data/poly.shp")

    ret = gdal.VectorInfo(ds, format="json")
    del ret["description"]
    del ret["layers"][0]["geometryFields"][0]["coordinateSystem"]["wkt"]
    if "projjson" in ret["layers"][0]["geometryFields"][0]["coordinateSystem"]:
        del ret["layers"][0]["geometryFields"][0]["coordinateSystem"]["projjson"]
    # print(ret)
    expected_ret = {
        "driverShortName": "ESRI Shapefile",
        "driverLongName": "ESRI Shapefile",
        "layers": [
            {
                "name": "poly",
                "metadata": {
                    "": {"DBF_DATE_LAST_UPDATE": "2018-08-02"},
                    "SHAPEFILE": {"SOURCE_ENCODING": ""},
                },
                "geometryFields": [
                    {
                        "name": "",
                        "type": "Polygon",
                        "nullable": True,
                        "extent": [478315.53125, 4762880.5, 481645.3125, 4765610.5],
                        "coordinateSystem": {"dataAxisToSRSAxisMapping": [1, 2]},
                    }
                ],
                "featureCount": 10,
                "fields": [
                    {
                        "name": "AREA",
                        "type": "Real",
                        "width": 12,
                        "precision": 3,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                    {
                        "name": "EAS_ID",
                        "type": "Integer64",
                        "width": 11,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                    {
                        "name": "PRFEDEA",
                        "type": "String",
                        "width": 16,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                ],
            }
        ],
        "metadata": {},
        "domains": {},
        "relationships": {},
    }
    assert ret == expected_ret


###############################################################################
# Test json output with features


def test_ogrinfo_lib_json_features():

    ds = gdal.OpenEx("../ogr/data/poly.shp")

    ret = gdal.VectorInfo(ds, format="json", dumpFeatures=True, limit=1)
    del ret["description"]
    del ret["layers"][0]["geometryFields"][0]["coordinateSystem"]["wkt"]
    if "projjson" in ret["layers"][0]["geometryFields"][0]["coordinateSystem"]:
        del ret["layers"][0]["geometryFields"][0]["coordinateSystem"]["projjson"]
    # print(ret)
    expected_ret = {
        "driverShortName": "ESRI Shapefile",
        "driverLongName": "ESRI Shapefile",
        "layers": [
            {
                "name": "poly",
                "metadata": {
                    "": {"DBF_DATE_LAST_UPDATE": "2018-08-02"},
                    "SHAPEFILE": {"SOURCE_ENCODING": ""},
                },
                "geometryFields": [
                    {
                        "name": "",
                        "type": "Polygon",
                        "nullable": True,
                        "extent": [478315.53125, 4762880.5, 481645.3125, 4765610.5],
                        "coordinateSystem": {"dataAxisToSRSAxisMapping": [1, 2]},
                    }
                ],
                "featureCount": 10,
                "fields": [
                    {
                        "name": "AREA",
                        "type": "Real",
                        "width": 12,
                        "precision": 3,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                    {
                        "name": "EAS_ID",
                        "type": "Integer64",
                        "width": 11,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                    {
                        "name": "PRFEDEA",
                        "type": "String",
                        "width": 16,
                        "nullable": True,
                        "uniqueConstraint": False,
                    },
                ],
                "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 215229.266,
                            "EAS_ID": 168,
                            "PRFEDEA": "35043411",
                        },
                        "fid": 0,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479819.84375, 4765180.5],
                                    [479690.1875, 4765259.5],
                                    [479647.0, 4765369.5],
                                    [479730.375, 4765400.5],
                                    [480039.03125, 4765539.5],
                                    [480035.34375, 4765558.5],
                                    [480159.78125, 4765610.5],
                                    [480202.28125, 4765482.0],
                                    [480365.0, 4765015.5],
                                    [480389.6875, 4764950.0],
                                    [480133.96875, 4764856.5],
                                    [480080.28125, 4764979.5],
                                    [480082.96875, 4765049.5],
                                    [480088.8125, 4765139.5],
                                    [480059.90625, 4765239.5],
                                    [480019.71875, 4765319.5],
                                    [479980.21875, 4765409.5],
                                    [479909.875, 4765370.0],
                                    [479859.875, 4765270.0],
                                    [479819.84375, 4765180.5],
                                ]
                            ],
                        },
                    }
                ],
            }
        ],
        "metadata": {},
        "domains": {},
        "relationships": {},
    }
    assert ret == expected_ret

    # Test bugfix of https://github.com/OSGeo/gdal/pull/7345
    ret_json_features = gdal.VectorInfo(ds, options="-json -features")
    ret_features_json = gdal.VectorInfo(ds, options="-features -json")
    assert ret_json_features == ret_features_json


def test_ogrinfo_lib_json_validate():

    ds = gdal.OpenEx("../ogr/data/poly.shp")

    ret = gdal.VectorInfo(ds, format="json", dumpFeatures=True)

    gdaltest.validate_json(ret, "ogrinfo_output.schema.json")


###############################################################################
# Test json output with ZM geometry type


def test_ogrinfo_lib_json_zm():

    ds = gdal.OpenEx("../ogr/data/shp/testpointzm.shp")

    ret = gdal.VectorInfo(ds, format="json")
    assert ret["layers"][0]["geometryFields"][0]["type"] == "PointZM"


###############################################################################
# Test text output of relationships


@pytest.mark.require_driver("OpenFileGDB")
def test_ogrinfo_lib_relationships():

    ds = gdal.OpenEx("../ogr/data/filegdb/relationships.gdb")

    ret = gdal.VectorInfo(ds)
    expected = """Relationship: composite_many_to_many
  Type: Composite
  Related table type: features
  Cardinality: ManyToMany
  Left table name: table6
  Right table name: table7
  Left table fields: pk
  Right table fields: parent_pk
  Mapping table name: composite_many_to_many
  Left mapping table fields: origin_foreign_key
  Right mapping table fields: dest_foreign_key
  Forward path label: table7
  Backward path label: table6
"""
    assert expected.replace("\r\n", "\n") in ret.replace("\r\n", "\n")


###############################################################################
# Test json output of relationships


@pytest.mark.require_driver("OpenFileGDB")
def test_ogrinfo_lib_json_relationships():

    ds = gdal.OpenEx("../ogr/data/filegdb/relationships.gdb")

    ret = gdal.VectorInfo(ds, format="json")
    gdaltest.validate_json(ret, "ogrinfo_output.schema.json")

    # print(ret["relationships"]["composite_many_to_many"])
    assert ret["relationships"]["composite_many_to_many"] == {
        "type": "Composite",
        "related_table_type": "features",
        "cardinality": "ManyToMany",
        "left_table_name": "table6",
        "right_table_name": "table7",
        "left_table_fields": ["pk"],
        "right_table_fields": ["parent_pk"],
        "mapping_table_name": "composite_many_to_many",
        "left_mapping_table_fields": ["origin_foreign_key"],
        "right_mapping_table_fields": ["dest_foreign_key"],
        "forward_path_label": "table7",
        "backward_path_label": "table6",
    }


###############################################################################
# Test json output with OFSTJSON field


@pytest.mark.require_driver("GeoJSON")
def test_ogrinfo_lib_json_OFSTJSON():

    ds = gdal.OpenEx("""{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": true }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": null }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": "astring" }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "nested": 75 } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "a": "b" } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}""")

    ret = gdal.VectorInfo(ds, format="json", dumpFeatures=True)
    assert ret["layers"][0]["features"] == [
        {
            "type": "Feature",
            "fid": 0,
            "properties": {"prop0": 42},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
        {
            "type": "Feature",
            "fid": 1,
            "properties": {"prop0": True},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
        {
            "type": "Feature",
            "fid": 2,
            "properties": {"prop0": None},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
        {
            "type": "Feature",
            "fid": 3,
            "properties": {"prop0": "astring"},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
        {
            "type": "Feature",
            "fid": 4,
            "properties": {"prop0": {"nested": 75}},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
        {
            "type": "Feature",
            "fid": 5,
            "properties": {"prop0": {"a": "b"}},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        },
    ]


###############################################################################
# Test json output with -fields=NO


@pytest.mark.require_driver("GeoJSON")
def test_ogrinfo_lib_json_fields_NO():

    ds = gdal.OpenEx("""{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}""")

    ret = gdal.VectorInfo(ds, options="-json -features -fields=NO")
    assert ret["layers"][0]["features"] == [
        {
            "type": "Feature",
            "fid": 0,
            "properties": {},
            "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        }
    ]


###############################################################################
# Test json output with -geom=NO


@pytest.mark.require_driver("GeoJSON")
def test_ogrinfo_lib_json_geom_NO():

    ds = gdal.OpenEx("""{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}""")

    ret = gdal.VectorInfo(ds, options="-json -features -geom=NO")
    assert ret["layers"][0]["features"] == [
        {"type": "Feature", "fid": 0, "properties": {"prop0": 42}, "geometry": None}
    ]


###############################################################################
# Test field domains


@pytest.mark.require_driver("GPKG")
def test_ogrinfo_lib_fielddomains():

    ret = gdal.VectorInfo("../ogr/data/gpkg/domains.gpkg", format="json")
    assert ret["domains"] == {
        "enum_domain": {
            "type": "coded",
            "fieldType": "Integer",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
            "codedValues": {"1": "one", "2": None},
        },
        "glob_domain": {
            "type": "glob",
            "fieldType": "String",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
            "glob": "*",
        },
        "range_domain_int": {
            "type": "range",
            "fieldType": "Integer",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
            "minValue": 1,
            "minValueIncluded": True,
            "maxValue": 2,
            "maxValueIncluded": False,
        },
        "range_domain_int64": {
            "type": "range",
            "fieldType": "Integer64",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
            "minValue": -1234567890123,
            "minValueIncluded": False,
            "maxValue": 1234567890123,
            "maxValueIncluded": True,
        },
        "range_domain_real": {
            "type": "range",
            "fieldType": "Real",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
            "minValue": 1.5,
            "minValueIncluded": True,
            "maxValue": 2.5,
            "maxValueIncluded": True,
        },
        "range_domain_real_inf": {
            "type": "range",
            "fieldType": "Real",
            "splitPolicy": "default value",
            "mergePolicy": "default value",
        },
    }


###############################################################################
# Test time zones


def test_ogrinfo_lib_time_zones():

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("unknown", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_UNKNOWN)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("localtime", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_LOCALTIME)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("mixed", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_MIXED_TZ)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("utc", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_UTC)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("plus_one_hour", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_UTC + 4)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("minus_one_hour", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_UTC - 4)
    lyr.CreateField(fld_defn)
    ret = gdal.VectorInfo(ds, format="json")

    gdaltest.validate_json(ret, "ogrinfo_output.schema.json")

    fields = ret["layers"][0]["fields"]
    assert "timezone" not in fields[0]
    assert fields[1]["timezone"] == "localtime"
    assert fields[2]["timezone"] == "mixed timezones"
    assert fields[3]["timezone"] == "UTC"
    assert fields[4]["timezone"] == "+01:00"
    assert fields[5]["timezone"] == "-01:00"


###############################################################################
# Test -extent3D


@pytest.mark.require_driver("ESRI Shapefile")
@pytest.mark.require_driver("GPKG")
def test_ogrinfo_lib_extent3D():

    ret = gdal.VectorInfo("../ogr/data/poly.shp", extent="3D")
    assert (
        "(478315.531250, 4762880.500000, none) - (481645.312500, 4765610.500000, none)"
        in ret
    )

    ret = gdal.VectorInfo("../ogr/data/poly.shp", extent="3D", format="json")
    assert ret["layers"][0]["geometryFields"][0]["extent"] == [
        478315.53125,
        4762880.5,
        481645.3125,
        4765610.5,
    ]
    assert ret["layers"][0]["geometryFields"][0]["extent3D"] == [
        478315.53125,
        4762880.5,
        None,
        481645.3125,
        4765610.5,
        None,
    ]

    gdaltest.validate_json(ret, "ogrinfo_output.schema.json")

    ret = gdal.VectorInfo("../ogr/data/gpkg/3d_envelope.gpkg", extent="3D")
    assert "(0.000000, 0.000000, 0.000000) - (3.000000, 3.000000, 3.000000)" in ret

    ret = gdal.VectorInfo(
        "../ogr/data/gpkg/3d_envelope.gpkg", extent="3D", format="json"
    )
    assert ret["layers"][0]["geometryFields"][0]["extent"] == [0.0, 0.0, 3.0, 3.0]
    assert ret["layers"][0]["geometryFields"][0]["extent3D"] == [
        0.0,
        0.0,
        0.0,
        3.0,
        3.0,
        3.0,
    ]

    gdaltest.validate_json(ret, "ogrinfo_output.schema.json")

    ret = gdal.VectorInfo(
        "../ogr/data/gpkg/3d_envelope.gpkg", extent="3D", where="fid = 1", format="json"
    )
    assert ret["layers"][0]["geometryFields"][0]["extent3D"] == [
        0.0,
        0.0,
        0.0,
        1.0,
        1.0,
        1.0,
    ]


###############################################################################
# Test geometry coordinate precision support


@pytest.mark.require_driver("GeoJSON")
def test_ogrinfo_lib_json_features_resolution():

    content = json.dumps(
        {
            "type": "FeatureCollection",
            "xy_coordinate_resolution": 1e-1,
            "z_coordinate_resolution": 1e-2,
            "features": [
                {
                    "type": "Feature",
                    "geometry": {
                        "type": "Point",
                        "coordinates": [1.23456789, 1.23456789, 1.23456789],
                    },
                    "properties": None,
                }
            ],
        }
    )

    j = gdal.VectorInfo(content, format="json", dumpFeatures=True)
    assert j["layers"][0]["features"][0]["geometry"] == {
        "type": "Point",
        "coordinates": [1.2, 1.2, 1.23],
    }

    s = gdal.VectorInfo(content, dumpFeatures=True)
    assert "POINT Z (1.2 1.2 1.23)" in s


###############################################################################
# Test layers option


def test_ogrinfo_lib_layers():

    ds = gdal.GetDriverByName("MEM").Create("dummy", 0, 0, 0, gdal.GDT_Unknown)
    ds.CreateLayer("foo")
    ds.CreateLayer("bar")

    j = gdal.VectorInfo(ds, format="json", layers=[])
    assert len(j["layers"]) == 2
    assert j["layers"][0]["name"] == "foo"
    assert j["layers"][1]["name"] == "bar"

    j = gdal.VectorInfo(ds, format="json", layers=["foo"])
    assert len(j["layers"]) == 1
    assert j["layers"][0]["name"] == "foo"

    j = gdal.VectorInfo(ds, format="json", layers=["foo", "bar"])
    assert len(j["layers"]) == 2
    assert j["layers"][0]["name"] == "foo"
    assert j["layers"][1]["name"] == "bar"

    with pytest.raises(Exception, match="Couldn't fetch requested layer"):
        gdal.VectorInfo(ds, format="json", layers=["invalid"])


###############################################################################


@pytest.mark.parametrize("epoch", ["2021.0", "2021.3"])
def test_ogrinfo_lib_coordinate_epoch(epoch):

    ds = gdal.GetDriverByName("MEM").Create("dummy", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(float(epoch))
    ds.CreateLayer("foo", srs=srs)

    ret = gdal.VectorInfo(ds)
    assert f"Coordinate epoch: {epoch}" in ret

    j = gdal.VectorInfo(ds, format="json")
    crs = j["layers"][0]["geometryFields"][0]["coordinateSystem"]
    assert "coordinateEpoch" in crs
    assert crs["coordinateEpoch"] == float(epoch)
