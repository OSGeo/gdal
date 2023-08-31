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

import pathlib

import gdaltest
import pytest

from osgeo import gdal

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

    ret = gdal.VectorInfo(ds, format="json", dumpFeatures=True)
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
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 247328.172,
                            "EAS_ID": 179,
                            "PRFEDEA": "35043423",
                        },
                        "fid": 1,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [480035.34375, 4765558.5],
                                    [480039.03125, 4765539.5],
                                    [479730.375, 4765400.5],
                                    [479647.0, 4765369.5],
                                    [479690.1875, 4765259.5],
                                    [479819.84375, 4765180.5],
                                    [479779.84375, 4765109.5],
                                    [479681.78125, 4764940.0],
                                    [479468.0, 4764942.5],
                                    [479411.4375, 4764940.5],
                                    [479353.0, 4764939.5],
                                    [479208.65625, 4764882.5],
                                    [479196.8125, 4764879.0],
                                    [479123.28125, 4765015.0],
                                    [479046.53125, 4765117.0],
                                    [479029.71875, 4765110.5],
                                    [479014.9375, 4765147.5],
                                    [479149.9375, 4765200.5],
                                    [479639.625, 4765399.5],
                                    [480035.34375, 4765558.5],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 261752.781,
                            "EAS_ID": 171,
                            "PRFEDEA": "35043414",
                        },
                        "fid": 2,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479819.84375, 4765180.5],
                                    [479859.875, 4765270.0],
                                    [479909.875, 4765370.0],
                                    [479980.21875, 4765409.5],
                                    [480019.71875, 4765319.5],
                                    [480059.90625, 4765239.5],
                                    [480088.8125, 4765139.5],
                                    [480082.96875, 4765049.5],
                                    [480000.28125, 4765043.0],
                                    [479934.96875, 4765020.0],
                                    [479895.125, 4765000.0],
                                    [479734.375, 4764865.0],
                                    [479680.28125, 4764852.0],
                                    [479644.78125, 4764827.5],
                                    [479637.875, 4764803.0],
                                    [479617.21875, 4764760.0],
                                    [479587.28125, 4764718.0],
                                    [479548.03125, 4764693.5],
                                    [479504.90625, 4764609.5],
                                    [479239.8125, 4764505.0],
                                    [479117.8125, 4764847.0],
                                    [479196.8125, 4764879.0],
                                    [479208.65625, 4764882.5],
                                    [479353.0, 4764939.5],
                                    [479411.4375, 4764940.5],
                                    [479468.0, 4764942.5],
                                    [479681.78125, 4764940.0],
                                    [479779.84375, 4765109.5],
                                    [479819.84375, 4765180.5],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 547597.188,
                            "EAS_ID": 173,
                            "PRFEDEA": "35043416",
                        },
                        "fid": 3,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479014.9375, 4765147.5],
                                    [479029.71875, 4765110.5],
                                    [479117.8125, 4764847.0],
                                    [479239.8125, 4764505.0],
                                    [479305.875, 4764361.0],
                                    [479256.03125, 4764314.5],
                                    [479220.90625, 4764212.5],
                                    [479114.5, 4764174.0],
                                    [479018.28125, 4764418.5],
                                    [478896.9375, 4764371.0],
                                    [478748.8125, 4764308.5],
                                    [478503.03125, 4764218.0],
                                    [478461.75, 4764337.5],
                                    [478443.9375, 4764400.5],
                                    [478447.8125, 4764454.0],
                                    [478448.6875, 4764531.5],
                                    [478502.1875, 4764541.5],
                                    [478683.0, 4764730.5],
                                    [478621.03125, 4764788.5],
                                    [478597.34375, 4764766.5],
                                    [478532.5, 4764695.5],
                                    [478460.125, 4764615.0],
                                    [478408.0625, 4764654.0],
                                    [478315.53125, 4764876.0],
                                    [478889.25, 4765100.0],
                                    [479014.9375, 4765147.5],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 15775.758,
                            "EAS_ID": 172,
                            "PRFEDEA": "35043415",
                        },
                        "fid": 4,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479029.71875, 4765110.5],
                                    [479046.53125, 4765117.0],
                                    [479123.28125, 4765015.0],
                                    [479196.8125, 4764879.0],
                                    [479117.8125, 4764847.0],
                                    [479029.71875, 4765110.5],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 101429.977,
                            "EAS_ID": 169,
                            "PRFEDEA": "35043412",
                        },
                        "fid": 5,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [480082.96875, 4765049.5],
                                    [480080.28125, 4764979.5],
                                    [480133.96875, 4764856.5],
                                    [479968.46875, 4764788.0],
                                    [479750.6875, 4764702.0],
                                    [479735.90625, 4764752.0],
                                    [479640.09375, 4764721.0],
                                    [479658.59375, 4764670.0],
                                    [479504.90625, 4764609.5],
                                    [479548.03125, 4764693.5],
                                    [479587.28125, 4764718.0],
                                    [479617.21875, 4764760.0],
                                    [479637.875, 4764803.0],
                                    [479644.78125, 4764827.5],
                                    [479680.28125, 4764852.0],
                                    [479734.375, 4764865.0],
                                    [479895.125, 4765000.0],
                                    [479934.96875, 4765020.0],
                                    [480000.28125, 4765043.0],
                                    [480082.96875, 4765049.5],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 268597.625,
                            "EAS_ID": 166,
                            "PRFEDEA": "35043409",
                        },
                        "fid": 6,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [480389.6875, 4764950.0],
                                    [480537.15625, 4765014.0],
                                    [480567.96875, 4764918.0],
                                    [480605.0, 4764835.0],
                                    [480701.0625, 4764738.0],
                                    [480710.25, 4764690.5],
                                    [480588.59375, 4764740.5],
                                    [480540.71875, 4764741.0],
                                    [480515.125, 4764695.0],
                                    [480731.65625, 4764561.5],
                                    [480692.1875, 4764453.5],
                                    [480677.84375, 4764439.0],
                                    [480655.34375, 4764397.5],
                                    [480584.375, 4764353.0],
                                    [480500.40625, 4764326.5],
                                    [480358.53125, 4764277.0],
                                    [480192.3125, 4764183.0],
                                    [480157.125, 4764266.5],
                                    [480234.3125, 4764304.0],
                                    [480289.125, 4764348.5],
                                    [480316.0, 4764395.0],
                                    [480343.5625, 4764477.0],
                                    [480343.71875, 4764532.5],
                                    [480258.03125, 4764767.0],
                                    [480177.15625, 4764742.0],
                                    [480093.75, 4764703.0],
                                    [480011.0, 4764674.5],
                                    [479985.0625, 4764732.0],
                                    [479968.46875, 4764788.0],
                                    [480133.96875, 4764856.5],
                                    [480389.6875, 4764950.0],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 1634833.375,
                            "EAS_ID": 158,
                            "PRFEDEA": "35043369",
                        },
                        "fid": 7,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [480701.0625, 4764738.0],
                                    [480761.46875, 4764778.0],
                                    [480824.96875, 4764820.0],
                                    [480922.03125, 4764850.5],
                                    [480930.71875, 4764852.0],
                                    [480984.25, 4764875.0],
                                    [481088.1875, 4764936.0],
                                    [481136.84375, 4764994.5],
                                    [481281.3125, 4764876.5],
                                    [481291.09375, 4764810.0],
                                    [481465.90625, 4764872.5],
                                    [481457.375, 4764937.0],
                                    [481509.65625, 4764967.0],
                                    [481538.90625, 4764982.5],
                                    [481575.0, 4764999.5],
                                    [481602.125, 4764915.5],
                                    [481629.84375, 4764829.5],
                                    [481645.3125, 4764797.5],
                                    [481635.96875, 4764795.5],
                                    [481235.3125, 4764650.0],
                                    [481209.8125, 4764633.5],
                                    [481199.21875, 4764623.5],
                                    [481185.5, 4764607.0],
                                    [481159.9375, 4764580.0],
                                    [481140.46875, 4764510.5],
                                    [481141.625, 4764480.5],
                                    [481199.84375, 4764180.0],
                                    [481143.4375, 4764010.5],
                                    [481130.3125, 4763979.5],
                                    [481039.9375, 4763889.5],
                                    [480882.6875, 4763670.0],
                                    [480826.0625, 4763650.5],
                                    [480745.1875, 4763628.5],
                                    [480654.4375, 4763627.5],
                                    [480599.8125, 4763660.0],
                                    [480281.9375, 4763576.5],
                                    [480221.5, 4763533.5],
                                    [480199.6875, 4763509.0],
                                    [480195.09375, 4763430.0],
                                    [480273.6875, 4763305.5],
                                    [480309.6875, 4763063.5],
                                    [480201.84375, 4762962.5],
                                    [479855.3125, 4762880.5],
                                    [479848.53125, 4762897.0],
                                    [479728.875, 4763217.5],
                                    [479492.6875, 4763850.0],
                                    [479550.0625, 4763919.5],
                                    [480120.21875, 4764188.5],
                                    [480192.3125, 4764183.0],
                                    [480358.53125, 4764277.0],
                                    [480500.40625, 4764326.5],
                                    [480584.375, 4764353.0],
                                    [480655.34375, 4764397.5],
                                    [480677.84375, 4764439.0],
                                    [480692.1875, 4764453.5],
                                    [480731.65625, 4764561.5],
                                    [480515.125, 4764695.0],
                                    [480540.71875, 4764741.0],
                                    [480588.59375, 4764740.5],
                                    [480710.25, 4764690.5],
                                    [480701.0625, 4764738.0],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": -596610.313,
                            "EAS_ID": 165,
                            "PRFEDEA": "35043408",
                        },
                        "fid": 8,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479750.6875, 4764702.0],
                                    [479968.46875, 4764788.0],
                                    [479985.0625, 4764732.0],
                                    [480011.0, 4764674.5],
                                    [480093.75, 4764703.0],
                                    [480177.15625, 4764742.0],
                                    [480258.03125, 4764767.0],
                                    [480343.71875, 4764532.5],
                                    [480343.5625, 4764477.0],
                                    [480316.0, 4764395.0],
                                    [480289.125, 4764348.5],
                                    [480234.3125, 4764304.0],
                                    [480157.125, 4764266.5],
                                    [480192.3125, 4764183.0],
                                    [480120.21875, 4764188.5],
                                    [479550.0625, 4763919.5],
                                    [479492.6875, 4763850.0],
                                    [479487.75, 4763864.5],
                                    [479442.75, 4763990.0],
                                    [479436.0, 4764023.0],
                                    [479398.9375, 4764100.0],
                                    [479349.625, 4764230.0],
                                    [479305.875, 4764361.0],
                                    [479239.8125, 4764505.0],
                                    [479504.90625, 4764609.5],
                                    [479658.59375, 4764670.0],
                                    [479750.6875, 4764702.0],
                                ]
                            ],
                        },
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "AREA": 5268.813,
                            "EAS_ID": 170,
                            "PRFEDEA": "35043413",
                        },
                        "fid": 9,
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [479750.6875, 4764702.0],
                                    [479658.59375, 4764670.0],
                                    [479640.09375, 4764721.0],
                                    [479735.90625, 4764752.0],
                                    [479750.6875, 4764702.0],
                                ]
                            ],
                        },
                    },
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
  Related table type: feature
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
        "related_table_type": "feature",
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


def test_ogrinfo_lib_json_OFSTJSON():

    ds = gdal.OpenEx(
        """{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": true }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": null }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": "astring" }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "nested": 75 } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "a": "b" } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}"""
    )

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


def test_ogrinfo_lib_json_fields_NO():

    ds = gdal.OpenEx(
        """{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}"""
    )

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


def test_ogrinfo_lib_json_geom_NO():

    ds = gdal.OpenEx(
        """{"type":"FeatureCollection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}"""
    )

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
