#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoJSON driver test suite.
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import json
import math
import os
import struct

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("GeoJSON")


###############################################################################
# Test utilities


def validate_layer(lyr, name, features, typ, fields, box):

    if name is not None and name != lyr.GetName():
        print("Wrong layer name")
        return False

    if features != lyr.GetFeatureCount():
        print("Wrong number of features")
        return False

    lyrDefn = lyr.GetLayerDefn()
    if lyrDefn is None:
        print("Layer definition is none")
        return False

    if typ != lyrDefn.GetGeomType():
        print("Wrong geometry type")
        print(lyrDefn.GetGeomType())
        return False

    if fields != lyrDefn.GetFieldCount():
        print("Wrong number of fields")
        return False

    extent = lyr.GetExtent()

    minx = abs(extent[0] - box[0])
    maxx = abs(extent[1] - box[1])
    miny = abs(extent[2] - box[2])
    maxy = abs(extent[3] - box[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print("Wrong spatial extent of layer")
        print(extent)
        return False

    return True


def verify_geojson_copy(fname, fids, names):

    assert gdaltest.gjpoint_feat is not None, "Missing features collection"

    ds = ogr.Open(fname)
    assert ds is not None, f"Can not open '{fname}'"

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Missing layer"

    ######################################################
    # Test attributes
    ogrtest.check_features_against_list(lyr, "FID", fids)

    lyr.ResetReading()
    ogrtest.check_features_against_list(lyr, "NAME", names)

    ######################################################
    # Test geometries
    lyr.ResetReading()
    for i in range(len(gdaltest.gjpoint_feat)):

        orig_feat = gdaltest.gjpoint_feat[i]
        feat = lyr.GetNextFeature()

        assert feat is not None, "Failed trying to read feature"

        ogrtest.check_feature_geometry(
            feat, orig_feat.GetGeometryRef(), max_error=0.001
        )

    lyr = None


def copy_shape_to_geojson(gjname, compress=None):

    if compress is not None:
        if compress[0:5] == "/vsig":
            dst_name = os.path.join("/vsigzip/", "tmp", gjname + ".geojson" + ".gz")
        elif compress[0:4] == "/vsiz":
            dst_name = os.path.join("/vsizip/", "tmp", gjname + ".geojson" + ".zip")
        elif compress == "/vsistdout/":
            dst_name = compress
        else:
            return False, None
    else:
        dst_name = os.path.join("tmp", gjname + ".geojson")

    ds = gdal.GetDriverByName("GeoJSON").Create(dst_name, 0, 0, 0, gdal.GDT_Unknown)
    if ds is None:
        return False, dst_name

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(gjname)
    if lyr is None:
        return False, dst_name

    assert lyr.GetDataset().GetDescription() == ds.GetDescription()

    ######################################################
    # Setup schema (all test shapefiles use common schema)
    ogrtest.quick_create_layer_def(lyr, [("FID", ogr.OFTReal), ("NAME", ogr.OFTString)])

    ######################################################
    # Copy in gjpoint.shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    src_name = os.path.join("data", "shp", gjname + ".shp")
    shp_ds = ogr.Open(src_name)
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.gjpoint_feat = []

    while feat is not None:

        gdaltest.gjpoint_feat.append(feat)

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    shp_lyr = None
    lyr = None

    assert ds.FlushCache() == gdal.CE_None
    ds = None

    return True, dst_name


###############################################################################
# Test file-based DS with standalone "Point" feature object.


def test_ogr_geojson_2():

    ds = ogr.Open("data/geojson/point.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("point")
    assert lyr is not None, "Missing layer called point"

    assert lyr.GetDataset().GetDescription() == ds.GetDescription()

    extent = (100.0, 100.0, 0.0, 0.0)

    rc = validate_layer(lyr, "point", 1, ogr.wkbPoint, 0, extent)
    assert rc

    lyr = None


###############################################################################
# Test file-based DS with standalone "LineString" feature object.


def test_ogr_geojson_3():

    ds = ogr.Open("data/geojson/linestring.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("linestring")
    assert lyr is not None, "Missing layer called linestring"

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, "linestring", 1, ogr.wkbLineString, 0, extent)
    assert rc

    lyr = None


##############################################################################
# Test file-based DS with standalone "Polygon" feature object.


def test_ogr_geojson_4():

    ds = ogr.Open("data/geojson/polygon.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("polygon")
    assert lyr is not None, "Missing layer called polygon"

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, "polygon", 1, ogr.wkbPolygon, 0, extent)
    assert rc

    lyr = None


##############################################################################
# Test file-based DS with standalone "GeometryCollection" feature object.


def test_ogr_geojson_5():

    ds = ogr.Open("data/geojson/geometrycollection.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("geometrycollection")
    assert lyr is not None, "Missing layer called geometrycollection"

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(
        lyr, "geometrycollection", 1, ogr.wkbGeometryCollection, 0, extent
    )
    assert rc

    lyr = None


##############################################################################
# Test file-based DS with standalone "MultiPoint" feature object.


def test_ogr_geojson_6():

    ds = ogr.Open("data/geojson/multipoint.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("multipoint")
    assert lyr is not None, "Missing layer called multipoint"

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, "multipoint", 1, ogr.wkbMultiPoint, 0, extent)
    assert rc

    lyr = None


##############################################################################
# Test file-based DS with standalone "MultiLineString" feature object.


def test_ogr_geojson_7():

    ds = ogr.Open("data/geojson/multilinestring.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("multilinestring")
    assert lyr is not None, "Missing layer called multilinestring"

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, "multilinestring", 1, ogr.wkbMultiLineString, 0, extent)
    assert rc

    lyr = None


##############################################################################
# Test file-based DS with standalone "MultiPolygon" feature object.


def test_ogr_geojson_8():

    ds = ogr.Open("data/geojson/multipolygon.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("multipolygon")
    assert lyr is not None, "Missing layer called multipolygon"

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, "multipolygon", 1, ogr.wkbMultiPolygon, 0, extent)
    assert rc

    lyr = None


##############################################################################
# Test translation of data/gjpoint.shp to GeoJSON file


def test_ogr_geojson_9():

    tests = [
        ["gjpoint", [1], ["Point 1"]],
        ["gjline", [1], ["Line 1"]],
        ["gjpoly", [1], ["Polygon 1"]],
        ["gjmultipoint", [1], ["MultiPoint 1"]],
        ["gjmultiline", [2], ["MultiLine 1"]],
        ["gjmultipoly", [2], ["MultiPoly 1"]],
    ]

    for test in tests:

        rc, dstname = copy_shape_to_geojson(test[0])
        try:
            assert rc, "Failed making copy of " + test[0] + ".shp"

            verify_geojson_copy(dstname, test[1], test[2])
        finally:
            if dstname:
                gdal.Unlink(dstname)


##############################################################################
# Test translation of data/gjpoint.shp to GZip compressed GeoJSON file


def test_ogr_geojson_10():

    tests = [
        ["gjpoint", [1], ["Point 1"]],
        ["gjline", [1], ["Line 1"]],
        ["gjpoly", [1], ["Polygon 1"]],
        ["gjmultipoint", [1], ["MultiPoint 1"]],
        ["gjmultiline", [2], ["MultiLine 1"]],
        ["gjmultipoly", [2], ["MultiPoly 1"]],
    ]

    for test in tests:

        rc, dstname = copy_shape_to_geojson(test[0], "/vsigzip/")
        try:
            assert rc, "Failed making copy of " + test[0] + ".shp"

            verify_geojson_copy(dstname, test[1], test[2])
        finally:
            if dstname:
                dstname = dstname[len("/vsigzip/") :]
                gdal.Unlink(dstname)
                if gdal.VSIStatL(dstname + ".properties") is not None:
                    gdal.Unlink(dstname + ".properties")


###############################################################################


def test_ogr_geojson_11():

    ds = ogr.Open("data/geojson/srs_name.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("srs_name")
    assert lyr is not None, "Missing layer called srs_name"

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, "srs_name", 1, ogr.wkbGeometryCollection, 0, extent)
    assert rc

    ref = lyr.GetSpatialRef()
    pcs = int(ref.GetAuthorityCode("PROJCS"))
    assert pcs == 26915, "Spatial reference was not valid"

    feature = lyr.GetNextFeature()
    geometry = feature.GetGeometryRef().GetGeometryRef(0)

    srs = geometry.GetSpatialReference()
    pcs = int(srs.GetAuthorityCode("PROJCS"))
    assert pcs == 26916, "Spatial reference for individual geometry was not valid"

    lyr = None


###############################################################################
# Test DS passed as name with standalone "Point" feature object (#3377)


def test_ogr_geojson_12():

    if os.name == "nt":
        pytest.skip()

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_ogrinfo_path()
        + ' -ro -al \'{"type": "Point","coordinates": [100.0, 0.0]}\''
    )
    assert ret.find(" POINT (100 0)") != -1


###############################################################################
# Test writing to stdout (#3381)


def test_ogr_geojson_13():

    test = ["gjpoint", [1], ["Point 1"]]

    rc, _ = copy_shape_to_geojson(test[0], "/vsistdout/")
    assert rc, "Failed making copy of " + test[0] + ".shp"


###############################################################################
# Test reading & writing various degenerated geometries


@gdaltest.disable_exceptions()
def test_ogr_geojson_14(tmp_path):

    with gdal.quiet_errors():
        ds = ogr.Open("data/geojson/ogr_geojson_14.geojson")
    lyr = ds.GetLayer(0)

    out_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_path / "out_ogr_geojson_14.geojson"
    )
    out_lyr = out_ds.CreateLayer("lyr")

    with gdal.quiet_errors():
        for feat in lyr:
            geom = feat.GetGeometryRef()
            if geom is not None:
                # print(geom)
                out_feat = ogr.Feature(feature_def=out_lyr.GetLayerDefn())
                out_feat.SetGeometry(geom)
                out_lyr.CreateFeature(out_feat)

    out_ds = None


###############################################################################
# Test Feature.ExportToJson (#3870)


def test_ogr_geojson_15():

    feature_defn = ogr.FeatureDefn()
    feature_defn.AddFieldDefn(ogr.FieldDefn("foo"))
    field_defn = ogr.FieldDefn("boolfield", ogr.OFTInteger)
    field_defn.SetSubType(ogr.OFSTBoolean)
    feature_defn.AddFieldDefn(field_defn)

    feature = ogr.Feature(feature_defn)
    feature.SetField("foo", "bar")
    feature.SetField("boolfield", True)
    feature.SetFID(0)

    geom = ogr.CreateGeometryFromWkt("POINT(1 2)")
    feature.SetGeometry(geom)

    try:
        out = feature.ExportToJson()
    except ImportError:
        pytest.skip()

    expected_out = """{"geometry": {"type": "Point", "coordinates": [1.0, 2.0]}, "type": "Feature", "properties": {"foo": "bar", "boolfield": true}, "id": 0}"""

    if out != expected_out:
        out_json = json.loads(out)
        expected_out_json = json.loads(expected_out)
        assert out_json == expected_out_json, out

    out = feature.ExportToJson(as_object=True)
    expected_out = {
        "geometry": {"type": "Point", "coordinates": [1.0, 2.0]},
        "type": "Feature",
        "properties": {"foo": "bar", "boolfield": True},
        "id": 0,
    }

    assert out == expected_out


###############################################################################
# Test reading files with no extension (#4314)


def test_ogr_geojson_20(tmp_vsimem):

    from glob import glob

    geojson_files = glob("data/*.json")
    geojson_files.extend(glob("data/*.geojson"))

    for gj in geojson_files:
        # create tmp file with no file extension
        data = open(gj, "rb").read()

        f = gdal.VSIFOpenL(tmp_vsimem / "testgj", "wb")
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        with gdal.quiet_errors():
            ds = ogr.Open(tmp_vsimem / "testgj")
        if ds is None:
            print(gj)
            print(data.decode("LATIN1"))
            pytest.fail("Failed to open datasource")
        ds = None


###############################################################################
# Test reading output of geocouch spatiallist


def test_ogr_geojson_21():

    ds = ogr.Open(
        """{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo", "properties": { "foo": "bar"}}}}]}"""
    )
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("OGRGeoJSON")

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt("POINT (1 2)")

    assert feature.GetFieldAsString("_id") == "aid"
    assert feature.GetFieldAsString("_rev") == "arev"
    assert feature.GetFieldAsInteger("intvalue") == 2
    ogrtest.check_feature_geometry(feature, ref_geom)

    lyr = None
    ds = None


###############################################################################
# Same as ogr_geojson_21 with several features


def test_ogr_geojson_22():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo"}}},
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": {"_id":"aid2", "_rev":"arev2", "type":"Feature",
                "properties":{"intvalue" : 3.5, "str2value" : "bar"}}}]}""")
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("OGRGeoJSON")

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt("POINT (1 2)")

    assert feature.GetFieldAsString("_id") == "aid"
    assert feature.GetFieldAsString("_rev") == "arev"
    assert feature.GetFieldAsDouble("intvalue") == 2
    ogrtest.check_feature_geometry(feature, ref_geom)

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt("POINT (3 4)")

    assert feature.GetFieldAsString("_id") == "aid2"
    assert feature.GetFieldAsString("_rev") == "arev2"
    assert feature.GetFieldAsDouble("intvalue") == 3.5
    assert feature.GetFieldAsString("str2value") == "bar"
    ogrtest.check_feature_geometry(feature, ref_geom)

    lyr = None
    ds = None


###############################################################################
# Write GeoJSON with bbox and test SRS writing&reading back


def test_ogr_geojson_23(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_23.json"
    )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4322)
    lyr = ds.CreateLayer("foo", srs=sr, options=["WRITE_BBOX=YES"])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 10)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 20)"))
    lyr.CreateFeature(feat)
    assert lyr.GetExtent() == (1.0, 2.0, 10.0, 20.0)
    assert lyr.GetExtent(geom_field=0) == (1.0, 2.0, 10.0, 20.0)
    with gdaltest.disable_exceptions(), gdal.quiet_errors():
        assert lyr.GetExtent(geom_field=1, can_return_null=True) is None
    lyr = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_geojson_23.json")
    lyr = ds.GetLayer(0)
    sr_got = lyr.GetSpatialRef()
    ds = None

    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert sr_got.IsSame(sr), "did not get expected SRS"

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_23.json", "rb") as f:
        data = f.read()

    assert b'"bbox": [ 1, 10, 2, 20 ]' in data, "did not find global bbox"
    assert b'"bbox":[1.0,10.0,1.0,10.0]' in data, "did not find first feature bbox"


###############################################################################
# Test alternate form of geojson


def test_ogr_geojson_24(tmp_vsimem):

    content = """loadGeoJSON({"layerFoo": { "type": "Feature",
  "geometry": {
    "type": "Point",
    "coordinates": [2, 49]
    },
  "name": "bar"
},
"layerBar": { "type": "FeatureCollection", "features" : [  { "type": "Feature",
  "geometry": {
    "type": "Point",
    "coordinates": [2, 49]
    },
  "other_name": "baz"
}]}})"""

    for i in range(2):
        if i == 0:
            ds = ogr.Open(content)
        else:
            gdal.FileFromMemBuffer(tmp_vsimem / "ogr_geojson_24.js", content)
            ds = ogr.Open(tmp_vsimem / "ogr_geojson_24.js")
            gdal.Unlink(tmp_vsimem / "ogr_geojson_24.js")

        assert ds is not None, "Failed to open datasource"

        lyr = ds.GetLayerByName("layerFoo")
        assert lyr is not None, "cannot find layer"

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt("POINT (2 49)")

        assert feature.GetFieldAsString("name") == "bar"
        ogrtest.check_feature_geometry(feature, ref_geom)

        lyr = ds.GetLayerByName("layerBar")
        assert lyr is not None, "cannot find layer"

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt("POINT (2 49)")

        assert feature.GetFieldAsString("other_name") == "baz"
        ogrtest.check_feature_geometry(feature, ref_geom)

        ds = None


###############################################################################
# Test 64bit support


def test_ogr_geojson_26(tmp_vsimem):

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "id": 1,
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": { "intvalue" : 1, "int64" : 1234567890123, "intlist" : [1] }},
{"type": "Feature", "id": 1234567890123,
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": { "intvalue" : 1234567890123, "intlist" : [1, 1234567890123] }},
 ]}""")
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("OGRGeoJSON")
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("int64") != 1234567890123:
        feature.DumpReadable()
        pytest.fail()

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1234567890123:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intvalue") != 1234567890123:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intlist") != [1, 1234567890123]:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_26.json"
    )
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("int64list", ogr.OFTInteger64List))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1234567890123)
    f.SetField(0, 1234567890123)
    f.SetFieldInteger64List(1, [1234567890123])
    lyr.CreateFeature(f)
    f = None
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_26.json", "rb") as f:
        data = f.read()

    assert (
        b'{"type":"Feature","id":1234567890123,"properties":{"int64":1234567890123,"int64list":[1234567890123]},"geometry":null}'
        in data
    )


###############################################################################
# Test workaround for 64bit values (returned as strings)


def test_ogr_geojson_27():

    with gdal.quiet_errors():
        # Warning 1: Integer values probably ranging out of 64bit integer range
        # have been found. Will be clamped to INT64_MIN/INT64_MAX
        ds = ogr.Open("""{"type": "FeatureCollection", "features":[
    {"type": "Feature",
     "geometry": {"type":"Point","coordinates":[1,2]},
     "properties": { "intvalue" : 1 }},
    {"type": "Feature",
     "geometry": {"type":"Point","coordinates":[3,4]},
     "properties": { "intvalue" : 12345678901231234567890123 }},
     ]}""")
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("OGRGeoJSON")

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        pytest.fail()

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 9223372036854775807:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None


###############################################################################
# Test handling of huge coordinates (#5377)


def test_ogr_geojson_35(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_35.json"
    )
    lyr = ds.CreateLayer("foo")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint_2D(-1.79769313486231571e308, -1.79769313486231571e308)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    with gdal.quiet_errors():
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(2)
        geom = ogr.Geometry(ogr.wkbPoint)
        geom.AddPoint(-1.7e308 * 2, 1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(3)
        geom = ogr.Geometry(ogr.wkbLineString)
        geom.AddPoint_2D(0, 0)
        geom.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(4)
        geom = ogr.Geometry(ogr.wkbPolygon)
        geom2 = ogr.Geometry(ogr.wkbLinearRing)
        geom2.AddPoint_2D(0, 0)
        geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        geom.AddGeometry(geom2)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(5)
        geom = ogr.Geometry(ogr.wkbMultiPoint)
        geom2 = ogr.Geometry(ogr.wkbPoint)
        geom2.AddPoint_2D(0, 0)
        geom2 = ogr.Geometry(ogr.wkbPoint)
        geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        geom.AddGeometry(geom2)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(6)
        geom = ogr.Geometry(ogr.wkbMultiLineString)
        geom2 = ogr.Geometry(ogr.wkbLineString)
        geom2.AddPoint_2D(0, 0)
        geom2 = ogr.Geometry(ogr.wkbLineString)
        geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        geom.AddGeometry(geom2)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(7)
        geom = ogr.Geometry(ogr.wkbMultiPolygon)
        geom2 = ogr.Geometry(ogr.wkbPolygon)
        geom3 = ogr.Geometry(ogr.wkbLinearRing)
        geom3.AddPoint_2D(0, 0)
        geom2.AddGeometry(geom3)
        geom2 = ogr.Geometry(ogr.wkbPolygon)
        geom3 = ogr.Geometry(ogr.wkbLinearRing)
        geom3.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
        geom2.AddGeometry(geom3)
        geom.AddGeometry(geom2)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_35.json", "rb") as f:
        data = f.read()

    assert b"-1.79" in data and b"e+308" in data
    for ident in range(2, 8):
        assert (
            b'{"type":"Feature","id":%d,"properties":{},"geometry":null}' % ident
            in data
        )


###############################################################################
# Test reading file with UTF-8 BOM (which is supposed to be illegal in JSON...) (#5630)


def test_ogr_geojson_36():

    ds = ogr.Open("data/geojson/point_with_utf8bom.json")
    assert ds is not None, "Failed to open datasource"
    ds = None


#########################################################################
# Test boolean type support


def test_ogr_geojson_37(tmp_vsimem):

    # Test read support
    ds = ogr.Open("""{"type": "FeatureCollection","features": [
{ "type": "Feature", "properties": { "bool" : false, "not_bool": false, "bool_list" : [false, true], "notbool_list" : [false, 3]}, "geometry": null  },
{ "type": "Feature", "properties": { "bool" : true, "not_bool": 2, "bool_list" : [true] }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("bool")).GetType()
        == ogr.OFTInteger
        and feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("bool")).GetSubType()
        == ogr.OFSTBoolean
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("not_bool")).GetSubType()
        == ogr.OFSTNone
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("bool_list")).GetType()
        == ogr.OFTIntegerList
        and feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("bool_list")).GetSubType()
        == ogr.OFSTBoolean
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("notbool_list")).GetType()
        == ogr.OFTString
        and feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("notbool_list")).GetSubType()
        == ogr.OFSTJSON
    )
    f = lyr.GetNextFeature()
    if f.GetField("bool") != 0 or f.GetField("bool_list") != [0, 1]:
        f.DumpReadable()
        pytest.fail()

    out_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_37.json"
    )
    out_lyr = out_ds.CreateLayer("test")
    for i in range(feat_defn.GetFieldCount()):
        out_lyr.CreateField(feat_defn.GetFieldDefn(i))
    out_f = ogr.Feature(out_lyr.GetLayerDefn())
    out_f.SetFrom(f)
    out_lyr.CreateFeature(out_f)
    out_ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_37.json", "rb") as f:
        data = f.read()

    assert (
        b'"bool":false,"not_bool":0,"bool_list":[false,true],"notbool_list":[false,3]'
        in data
    )


###############################################################################
# Test datetime/date/time type support


def test_ogr_geojson_38(tmp_vsimem):

    # Test read support
    ds = gdal.OpenEx("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "properties": { "dt": "2014-11-20 12:34:56+0100", "dt2": "2014\\/11\\/20", "date":"2014\\/11\\/20", "time":"12:34:56", "no_dt": "2014-11-20 12:34:56+0100", "no_dt2": "2014-11-20 12:34:56+0100", "no_date": "2022/05/12 blah" }, "geometry": null },
{ "type": "Feature", "properties": { "dt": "2014\\/11\\/20", "dt2": "2014\\/11\\/20T12:34:56Z", "date":"2014-11-20", "time":"12:34:56", "no_dt": "foo", "no_dt2": 1 }, "geometry": null }
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("dt")).GetType()
        == ogr.OFTDateTime
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("dt2")).GetType()
        == ogr.OFTDateTime
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("date")).GetType() == ogr.OFTDate
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("time")).GetType() == ogr.OFTTime
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("no_dt")).GetType()
        == ogr.OFTString
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("no_dt2")).GetType()
        == ogr.OFTString
    )
    assert (
        feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("no_date")).GetType()
        == ogr.OFTString
    )
    f = lyr.GetNextFeature()
    if (
        f.GetField("dt") != "2014/11/20 12:34:56+01"
        or f.GetField("dt2") != "2014/11/20 00:00:00"
        or f.GetField("date") != "2014/11/20"
        or f.GetField("time") != "12:34:56"
    ):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if (
        f.GetField("dt") != "2014/11/20 00:00:00"
        or f.GetField("dt2") != "2014/11/20 12:34:56+00"
        or f.GetField("date") != "2014/11/20"
        or f.GetField("time") != "12:34:56"
    ):
        f.DumpReadable()
        pytest.fail()

    tmpfilename = tmp_vsimem / "out.json"
    gdal.VectorTranslate(
        tmpfilename, ds, options="-lco NATIVE_DATA=dummy -of GeoJSON"
    )  # dummy NATIVE_DATA so that input values are not copied directly

    with gdal.VSIFile(tmpfilename, "rb") as f:
        data = f.read()

    assert (
        b'"dt":"2014-11-20T12:34:56+01:00","dt2":"2014-11-20T00:00:00","date":"2014-11-20","time":"12:34:56"'
        in data
    ), data

    ds = gdal.OpenEx(
        """{"type": "FeatureCollection", "features": [
{ "type": "Feature", "properties": { "dt": "2014-11-20 12:34:56+0100", "dt2": "2014\\/11\\/20", "date":"2014\\/11\\/20", "time":"12:34:56", "no_dt": "2014-11-20 12:34:56+0100", "no_dt2": "2014-11-20 12:34:56+0100" }, "geometry": null }
] }""",
        open_options=["DATE_AS_STRING=YES"],
    )
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    for i in range(feat_defn.GetFieldCount()):
        assert feat_defn.GetFieldDefn(i).GetType() == ogr.OFTString


###############################################################################
# Test id top-object level


@gdaltest.disable_exceptions()
def test_ogr_geojson_39():

    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "bar" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != "foo" or feat.GetField("bar") != "baz":
        feat.DumpReadable()
        pytest.fail()

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : 6 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != 6:
        feat.DumpReadable()
        pytest.fail()

    # Same with 2 features
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : 6 }, "geometry": null },
{ "type": "Feature", "id" : "bar", "properties": { "id" : 7 }, "geometry": null }
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != 6:
        feat.DumpReadable()
        pytest.fail()

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != "baz":
        feat.DumpReadable()
        pytest.fail()

    # id and properties.ID (#6538)
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "ID": 2 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "ID"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    )
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField("ID") != 2:
        feat.DumpReadable()
        pytest.fail()

    # Test handling of duplicated id
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : 2, "properties": { "foo": "baw" }, "geometry": null }
] }""")
    assert gdal.GetLastErrorMsg() != "", "expected warning"
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField("foo") != "bar":
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2 or feat.GetField("foo") != "baz":
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 3 or feat.GetField("foo") != "baw":
        feat.DumpReadable()
        pytest.fail()

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != -1:
        feat.DumpReadable()
        pytest.fail()

    # negative id 64bit
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger64
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != -1234567890123:
        feat.DumpReadable()
        pytest.fail()

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger64
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != -2:
        feat.DumpReadable()
        pytest.fail()

    # positive and then negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != 1:
        feat.DumpReadable()
        pytest.fail()

    # mix of int and string id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : "str", "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -3, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (
        feat_defn.GetFieldDefn(0).GetName() == "id"
        and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    )
    feat = lyr.GetNextFeature()
    if feat.GetField("id") != "-2":
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test nested attributes


def test_ogr_geojson_40():

    ds = gdal.OpenEx(
        """{
  "type": "FeatureCollection",
  "features" :
  [
    {
      "type": "Feature",
      "geometry": {
        "type": "Point",
        "coordinates": [ 2, 49 ]
      },
      "properties": {
        "a_property": 1,
        "some_object": {
          "a_property": 1,
          "another_property": 2
        }
      }
    },
    {
      "type": "Feature",
      "geometry": {
        "type": "Point",
        "coordinates": [ 2, 49 ]
      },
      "properties": {
        "a_property": "foo",
        "some_object": {
          "a_property": 1,
          "another_property": 2.34
        }
      }
    }
  ]
}""",
        gdal.OF_VECTOR,
        open_options=["FLATTEN_NESTED_ATTRIBUTES=YES", "NESTED_ATTRIBUTE_SEPARATOR=."],
    )
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if (
        feat.GetField("a_property") != "foo"
        or feat.GetField("some_object.a_property") != 1
        or feat.GetField("some_object.another_property") != 2.34
    ):
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test ogr.CreateGeometryFromJson()


def test_ogr_geojson_41():

    # Check that by default we return a WGS 84 SRS
    g = ogr.CreateGeometryFromJson("{ 'type': 'Point', 'coordinates' : [ 2, 49] }")
    assert g.ExportToWkt() == "POINT (2 49)"
    srs = g.GetSpatialReference()
    g = None

    assert srs.ExportToWkt().find("WGS 84") >= 0

    # But if a crs object is set (allowed originally, but not recommended!), we use it
    g = ogr.CreateGeometryFromJson(
        '{ "type": "Point", "coordinates" : [ 2, 49], "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::4322" } } }'
    )
    srs = g.GetSpatialReference()
    assert srs.ExportToWkt().find("4322") >= 0

    # But if a crs object is set to null, set no crs
    g = ogr.CreateGeometryFromJson(
        '{ "type": "Point", "coordinates" : [ 2, 49], "crs": null }'
    )
    srs = g.GetSpatialReference()
    assert not srs


###############################################################################
# Test Feature without geometry


def test_ogr_geojson_43():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "properties": {"foo": "bar"}}]}""")
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("OGRGeoJSON")

    feature = lyr.GetNextFeature()
    if feature.GetFieldAsString("foo") != "bar":
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None


###############################################################################
# Test null Feature (#6166)


def test_ogr_geojson_44():

    with pytest.raises(Exception):
        ogr.Open("""{"type": "FeatureCollection", "features":[ null ]}""")


###############################################################################
# Test native data support


def test_ogr_geojson_45(tmp_vsimem):

    # Test read support
    content = """{"type": "FeatureCollection", "foo": "bar", "bar": "baz",
    "features":[ { "type": "Feature", "foo": ["bar", "baz", 1.0, true, false,[],{}], "properties": { "myprop": "myvalue" }, "geometry": null } ]}"""
    for i in range(2):
        if i == 0:
            ds = gdal.OpenEx(content, gdal.OF_VECTOR, open_options=["NATIVE_DATA=YES"])
        else:
            gdal.FileFromMemBuffer(tmp_vsimem / "ogr_geojson_45.json", content)
            ds = gdal.OpenEx(
                tmp_vsimem / "ogr_geojson_45.json",
                gdal.OF_VECTOR,
                open_options=["NATIVE_DATA=YES"],
            )
        lyr = ds.GetLayer(0)
        native_data = lyr.GetMetadataItem("NATIVE_DATA", "NATIVE_DATA")
        assert native_data == '{ "foo": "bar", "bar": "baz" }'
        native_media_type = lyr.GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA")
        assert native_media_type == "application/vnd.geo+json"
        f = lyr.GetNextFeature()
        native_data = f.GetNativeData()
        if i == 0:
            expected = [
                '{ "type": "Feature", "foo": [ "bar", "baz", 1.000000, true, false, [ ], { } ], "properties": { "myprop": "myvalue" }, "geometry": null }',
                '{ "type": "Feature", "foo": [ "bar", "baz", 1.0, true, false, [ ], { } ], "properties": { "myprop": "myvalue" }, "geometry": null }',
            ]
        else:
            expected = [
                '{"type":"Feature","foo":["bar","baz",1.0,true,false,[],{}],"properties":{"myprop":"myvalue"},"geometry":null}'
            ]
        assert native_data in expected
        native_media_type = f.GetNativeMediaType()
        assert native_media_type == "application/vnd.geo+json"
        ds = None
        if i == 1:
            gdal.Unlink(tmp_vsimem / "ogr_geojson_45.json")

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_45.json"
    )
    lyr = ds.CreateLayer(
        "test",
        options=[
            'NATIVE_DATA={ "type": "ignored", "bbox": [ 0, 0, 0, 0 ], "foo": "bar", "bar": "baz", "features": "ignored" }',
            "NATIVE_MEDIA_TYPE=application/vnd.geo+json",
        ],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    json_geom = """{ "type": "GeometryCollection", "foo_gc": "bar_gc", "geometries" : [
                        { "type": "Point", "foo_point": "bar_point", "coordinates": [0,1,2, 3] },
                        { "type": "LineString", "foo_linestring": "bar_linestring", "coordinates": [[0,1,2, 4]] },
                        { "type": "MultiPoint", "foo_multipoint": "bar_multipoint", "coordinates": [[0,1,2, 5]] },
                        { "type": "MultiLineString", "foo_multilinestring": "bar_multilinestring", "coordinates": [[[0,1,2, 6]]] },
                        { "type": "Polygon", "foo_polygon": "bar_polygon", "coordinates": [[[0,1,2, 7]]] },
                        { "type": "MultiPolygon", "foo_multipolygon": "bar_multipolygon", "coordinates": [[[[0,1,2, 8]]]] }
                        ] }"""
    f.SetNativeData(
        '{ "type": "ignored", "bbox": "ignored", "properties" : "ignored", "foo_feature": "bar_feature", "geometry": %s }'
        % json_geom
    )
    f.SetNativeMediaType("application/vnd.geo+json")
    with gdal.quiet_errors():  # will warn about "coordinates": [0,1,2, 3]
        f.SetGeometry(ogr.CreateGeometryFromJson(json_geom))
    lyr.CreateFeature(f)
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_45.json", "rb") as f:
        data = f.read()

    gdal.Unlink(tmp_vsimem / "ogr_geojson_45.json")

    assert (
        b'"bbox": [ 0, 1, 2, 0, 1, 2 ],' in data
        and b'"foo": "bar"' in data
        and b'"bar": "baz"' in data
        and b'"foo_feature":"bar_feature"' in data
        and b'"foo_gc":"bar_gc"' in data
        and b'"foo_point":"bar_point"' in data
        and b"3" in data
        and b'"foo_linestring":"bar_linestring"' in data
        and b"4" in data
        and b'"foo_multipoint":"bar_multipoint"' in data
        and b"5" in data
        and b'"foo_multilinestring":"bar_multilinestring"' in data
        and b"6" in data
        and b'"foo_polygon":"bar_polygon"' in data
        and b"7" in data
        and b'"foo_multipolygon":"bar_multipolygon"' in data
        and b"8" in data
    )

    # Test native support with string id
    src_ds = gdal.OpenEx(
        """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature",
  "id": "foobarbaz",
  "properties": {},
  "geometry": null
}
]
}
""",
        open_options=["NATIVE_DATA=YES"],
    )
    gdal.VectorTranslate(tmp_vsimem / "out.json", src_ds, format="GeoJSON")

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": "foobarbaz", "properties": { }, "geometry": null }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test native support with numeric id
    src_ds = gdal.OpenEx(
        """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature",
  "id": 1234657890123,
  "properties": {},
  "geometry": null
}
]
}
""",
        open_options=["NATIVE_DATA=YES"],
    )
    gdal.VectorTranslate(tmp_vsimem / "out.json", src_ds, format="GeoJSON")

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"features": [
{"type":"Feature","id":1234657890123,"properties":{},"geometry":null}
]
}
"""
    assert json.loads(got) == json.loads(expected)


###############################################################################
# Test that writing JSon content as value of a string field is serialized as it


def test_ogr_geojson_46(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_46.json"
    )
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("myprop"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["myprop"] = '{ "a": "b" }'
    lyr.CreateFeature(f)
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_46.json", "rb") as f:
        data = f.read()

    assert b'{"myprop":{"a":"b"}}' in data


###############################################################################
# Test SetFeature() support


@gdaltest.disable_exceptions()
def test_ogr_geojson_47(tmp_vsimem):

    # ERROR 6: Update from inline definition not supported
    with gdal.quiet_errors():
        ds = ogr.Open('{"type": "FeatureCollection", "features":[]}', update=1)
    assert ds is None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_47.json",
        """{"type": "FeatureCollection", "foo": "bar",
    "features":[ { "type": "Feature", "bar": "baz", "properties": { "myprop": "myvalue" }, "geometry": null } ]}""",
    )

    # Test read support
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetField("myprop", "another_value")
    lyr.SetFeature(f)
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_47.json", "rb") as f:
        data = f.read()

    # we don't want crs if there's no in the source
    assert (
        b'"foo": "bar"' in data
        and b'"bar":"baz"' in data
        and b"crs" not in data
        and b'"myprop":"another_value"' in data
    )

    # Test append support
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)
    if f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    lyr.CreateFeature(f)
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test append support
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(4 5)"))
    lyr.CreateFeature(f)
    f.SetField("myprop", "value_of_point_4_5")
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_47.json", "rb") as f:
        data = f.read()

    # we don't want crs if there's no in the source
    assert (
        b'"foo": "bar"' in data
        and b'"bar":"baz"' in data
        and b"crs" not in data
        and b'"myprop":"another_value"' in data
        and b'"myprop":"value_of_point_4_5"' in data
        and b"id" not in data
    )

    gdal.Unlink(tmp_vsimem / "ogr_geojson_47.json")

    # Test appending to empty features array
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_47.json",
        """{ "type": "FeatureCollection", "features": []}""",
    )
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    # Test appending to array ending with non feature
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_47.json",
        """{ "type": "FeatureCollection", "features": [ null ]}""",
    )
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    # Test appending to feature collection not ending with "features"
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_47.json",
        """{ "type": "FeatureCollection", "features": [], "something": "else"}""",
    )
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_47.json", "rb") as f:
        data = f.read()

    assert b"something" in data

    with gdaltest.config_option("OGR_GEOJSON_REWRITE_IN_PLACE", "YES"):
        # Test appending to feature collection with "bbox"
        gdal.FileFromMemBuffer(
            tmp_vsimem / "ogr_geojson_47.json",
            """{ "type": "FeatureCollection", "bbox": [0,0,0,0], "features": [ { "type": "Feature", "geometry": { "type": "Point", "coordinates": [0,0]} } ]}""",
        )
        ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json", update=1)
        lyr = ds.GetLayer(0)
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
        ds = None
        ds = ogr.Open(tmp_vsimem / "ogr_geojson_47.json")
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_47.json", "rb") as f:
        data = f.read()

    assert b"bbox" in data


###############################################################################
# Test SetFeature() support with file that has a single feature not in a FeatureCollection


def test_ogr_geojson_48(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_48.json",
        """{ "type": "Feature", "bar": "baz", "bbox": [2,49,2,49], "properties": { "myprop": "myvalue" }, "geometry": {"type": "Point", "coordinates": [ 2, 49]} }""",
    )

    # Test read support
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_48.json", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetField("myprop", "another_value")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 50)"))
    lyr.SetFeature(f)
    ds = None

    fp = gdal.VSIFOpenL(tmp_vsimem / "ogr_geojson_48.json", "rb")
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
        gdal.VSIFCloseL(fp)
    else:
        data = None

    gdal.Unlink(tmp_vsimem / "ogr_geojson_48.json")

    # we don't want crs if there's no in the source
    assert (
        '"bar": "baz"' in data
        and '"bbox": [ 3.0, 50.0, 3.0, 50.0 ]' in data
        and "crs" not in data
        and "FeatureCollection" not in data
        and '"myprop": "another_value"' in data
    )


###############################################################################
# Test UpdateFeature() support


@pytest.mark.parametrize("check_after_update_before_reopen", [True, False])
@pytest.mark.parametrize("sync_to_disk_after_update", [True, False])
def test_ogr_geojson_update_feature(
    tmp_vsimem, check_after_update_before_reopen, sync_to_disk_after_update
):

    filename = str(tmp_vsimem / "test.json")

    with ogr.GetDriverByName("GeoJSON").CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer("test")
        lyr.CreateField(ogr.FieldDefn("int64list", ogr.OFTInteger64List))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["int64list"] = [123456790123, -123456790123]
        lyr.CreateFeature(f)

    with ogr.Open(filename, update=1) as ds:
        lyr = ds.GetLayer(0)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(0)
        f["int64list"] = [-123456790123, 123456790123]
        lyr.UpdateFeature(f, [0], [], False)

        if sync_to_disk_after_update:
            lyr.SyncToDisk()

        if check_after_update_before_reopen:
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f["int64list"] == [-123456790123, 123456790123]

    with ogr.Open(filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["int64list"] == [-123456790123, 123456790123]


###############################################################################
# Test ARRAY_AS_STRING


def test_ogr_geojson_49(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_49.json",
        """{ "type": "Feature", "properties": { "foo": ["bar"] }, "geometry": null }""",
    )

    # Test read support
    ds = gdal.OpenEx(
        tmp_vsimem / "ogr_geojson_49.json", open_options=["ARRAY_AS_STRING=YES"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    if f["foo"] != '[ "bar" ]':
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test that we serialize floating point values with enough significant figures


def test_ogr_geojson_50(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_50.json"
    )
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("val", ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["val"] = 1.23456789012456
    lyr.CreateFeature(f)
    # To test smart rounding
    f = ogr.Feature(lyr.GetLayerDefn())
    f["val"] = 5268.813
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL(tmp_vsimem / "ogr_geojson_50.json", "rb")
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    gdal.Unlink(tmp_vsimem / "ogr_geojson_50.json")

    assert "1.23456789012456" in data or "5268.813 " in data

    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION not,
    # then it also applies to coordinates
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_50.json"
    )
    lyr = ds.CreateLayer("test", options=["SIGNIFICANT_FIGURES=17"])
    lyr.CreateField(ogr.FieldDefn("val", ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0.0000123456789012456 0)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL(tmp_vsimem / "ogr_geojson_50.json", "rb")
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    gdal.Unlink(tmp_vsimem / "ogr_geojson_50.json")

    assert "1.23456789012456" in data or "-5" in data

    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION too,
    # then SIGNIFICANT_FIGURES only applies to non-coordinates floating point values.
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_50.json"
    )
    lyr = ds.CreateLayer(
        "test", options=["COORDINATE_PRECISION=15", "SIGNIFICANT_FIGURES=17"]
    )
    lyr.CreateField(ogr.FieldDefn("val", ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["val"] = 1.23456789012456
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0.0000123456789012456 0)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL(tmp_vsimem / "ogr_geojson_50.json", "rb")
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    gdal.Unlink(tmp_vsimem / "ogr_geojson_50.json")

    assert "0.00001234" in data and "1.23456789012456" in data


###############################################################################
# Test writing and reading empty geometries


def test_ogr_geojson_51(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_51.json"
    )
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 2
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 3
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 4
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 5
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTILINESTRING EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 6
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOLYGON EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 7
    f.SetGeometry(ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION EMPTY"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_51.json", "rb") as f:
        data = f.read()

    assert b'{"id":1},"geometry":null' in data

    assert b'{"id":2},"geometry":{"type":"LineString","coordinates":[]}}' in data

    assert b'{"id":3},"geometry":{"type":"Polygon","coordinates":[]}}' in data

    assert b'{"id":4},"geometry":{"type":"MultiPoint","coordinates":[]}}' in data

    assert b'{"id":5},"geometry":{"type":"MultiLineString","coordinates":[]}}' in data

    assert b'{"id":6},"geometry":{"type":"MultiPolygon","coordinates":[]}}' in data

    assert b'{"id":7},"geometry":{"type":"GeometryCollection","geometries":[]}}' in data

    ds = ogr.Open(tmp_vsimem / "ogr_geojson_51.json")
    lyr = ds.GetLayer(0)
    for f in lyr:
        if f.GetFID() >= 2:
            assert f.GetGeometryRef().IsEmpty()


###############################################################################
# Test NULL type detection


def test_ogr_geojson_52():

    ds = ogr.Open("data/geojson/nullvalues.geojson")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("nullvalues")
    assert lyr is not None, "Missing layer called nullvalues"

    fld = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld.GetNameRef() == "int"
    assert fld.GetType() == ogr.OFTInteger
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    assert fld.GetNameRef() == "string"
    assert fld.GetType() == ogr.OFTString
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    assert fld.GetNameRef() == "double"
    assert fld.GetType() == ogr.OFTReal


###############################################################################
# Test that M is ignored (this is a test of OGRLayer::CreateFeature() actually)


def test_ogr_geojson_53(tmp_vsimem):

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_53.json"
    )
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT ZM (1 2 3 4)"))
    lyr.CreateFeature(f)
    ds = None

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_53.json", "rb") as f:
        data = f.read()

    assert b'{"type":"Point","coordinates":[1.0,2.0,3.0]}' in data


###############################################################################
# Test NULL type detection when first value is null


def test_ogr_geojson_54():

    ds = ogr.Open("""{
   "type": "FeatureCollection",

  "features": [
      { "type": "Feature", "properties": { "int": null, "string": null, "double": null, "dt" : null, "boolean": null, "null": null }, "geometry": null },
      { "type": "Feature", "properties": { "int": 168, "string": "string", "double": 1.23, "dt" : "2016-05-18T12:34:56Z", "boolean": true }, "geometry": null }
  ]
}
""")
    lyr = ds.GetLayer(0)

    fld = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld.GetType() == ogr.OFTInteger
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    assert fld.GetType() == ogr.OFTString
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    assert fld.GetType() == ogr.OFTReal
    fld = lyr.GetLayerDefn().GetFieldDefn(3)
    assert fld.GetType() == ogr.OFTDateTime
    fld = lyr.GetLayerDefn().GetFieldDefn(4)
    assert fld.GetType() == ogr.OFTInteger
    assert fld.GetSubType() == ogr.OFSTBoolean
    assert fld.GetWidth() == 1
    fld = lyr.GetLayerDefn().GetFieldDefn(5)
    assert fld.GetType() == ogr.OFTString


###############################################################################
# Test RFC 7946


def read_file(filename):
    f = gdal.VSIFOpenL(filename, "rb")
    if f is None:
        return None
    content = gdal.VSIFReadL(1, 10000, f).decode("UTF-8")
    gdal.VSIFCloseL(f)
    return content


def test_ogr_geojson_55(tmp_vsimem):

    # Basic test for standard bbox and coordinate truncation
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
   "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "id": 123, "properties": {}, "geometry": { "type": "Point", "coordinates": [2.123456789, 49] } },
      { "type": "Feature", "id": 124, "properties": {}, "geometry": { "type": "Point", "coordinates": [3, 50] } }
  ]
}""",
        options="-f GeoJSON -lco RFC7946=YES -lco WRITE_BBOX=YES -preserve_fid",
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 2.1234568, 49.0000000, 3.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "id": 123, "properties": { }, "bbox": [ 2.1234568, 49.0, 2.1234568, 49.0 ], "geometry": { "type": "Point", "coordinates": [ 2.1234568, 49.0 ] } },
{ "type": "Feature", "id": 124, "properties": { }, "bbox": [ 3.0, 50.0, 3.0, 50.0 ], "geometry": { "type": "Point", "coordinates": [ 3.0, 50.0 ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test polygon winding order
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[3,49],[3,50],[2,50],[2,49]],[[2.1,49.1],[2.1,49.9],[2.9,49.9],[2.9,49.1],[2.1,49.1]]] } },
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[2,50],[3,50],[3,49],[2,49]],[[2.1,49.1],[2.9,49.1],[2.9,49.9],[2.1,49.9],[2.1,49.1]]] } },
]
}
""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 2.0000000, 49.0000000, 3.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 2.0, 49.0, 3.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 2.0, 49.0 ], [ 3.0, 49.0 ], [ 3.0, 50.0 ], [ 2.0, 50.0 ], [ 2.0, 49.0 ] ], [ [ 2.1, 49.1 ], [ 2.1, 49.9 ], [ 2.9, 49.9 ], [ 2.9, 49.1 ], [ 2.1, 49.1 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 2.0, 49.0, 3.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 2.0, 49.0 ], [ 3.0, 49.0 ], [ 3.0, 50.0 ], [ 2.0, 50.0 ], [ 2.0, 49.0 ] ], [ [ 2.1, 49.1 ], [ 2.1, 49.9 ], [ 2.9, 49.9 ], [ 2.9, 49.1 ], [ 2.1, 49.1 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test foreign member
    src_ds = gdal.OpenEx(
        """{
"type": "FeatureCollection",
"coordinates": "should not be found in output",
"geometries": "should not be found in output",
"geometry": "should not be found in output",
"properties": "should not be found in output",
"valid": "should be in output",
"crs": "should not be found in output",
"bbox": [0,0,0,0],
"features": [
{ "type": "Feature",
  "id": ["not expected as child of features"],
  "coordinates": "should not be found in output",
  "geometries": "should not be found in output",
  "features": "should not be found in output",
  "valid": "should be in output",
  "properties": { "foo": "bar" },
  "geometry": {
    "type": "Point",
    "bbox": [0,0,0,0],
    "geometry": "should not be found in output",
    "properties": "should not be found in output",
    "features": "should not be found in output",
    "valid": "should be in output",
    "coordinates": [2,49]
  }
}
]
}
""",
        open_options=["NATIVE_DATA=YES"],
    )
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"valid": "should be in output",
"bbox": [ 2.0000000, 49.0000000, 2.0000000, 49.0000000 ],
"features": [
{ "type": "Feature",
  "valid": "should be in output",
  "properties": { "id": [ "not expected as child of features" ], "foo": "bar" },
  "geometry": { "type": "Point", "coordinates": [ 2.0, 49.0 ], "valid": "should be in output" } }
]
}
"""
    assert json.loads(got) == json.loads(expected)


###############################################################################
# Test RFC 7946 (that require geos)


@pytest.mark.require_geos
def test_ogr_geojson_56(tmp_vsimem):

    # Test offsetting longitudes beyond antimeridian
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [182, 49] } },
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [-183, 50] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[-183, 51],[-182, 48]] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[182, 52],[183, 47]] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[-183, 51],[-183, 48],[-182, 48],[-183, 48],[-183, 51]]] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[183, 51],[183, 48],[182, 48],[183, 48],[183, 51]]] } },
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -178.0000000, 47.0000000, 178.0000000, 52.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 49.0, -178.0, 49.0 ], "geometry": { "type": "Point", "coordinates": [ -178.0, 49.0 ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 50.0, 177.0, 50.0 ], "geometry": { "type": "Point", "coordinates": [ 177.0, 50.0 ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, 178.0, 51.0 ], "geometry": { "type": "LineString", "coordinates": [ [ 177.0, 51.0 ], [ 178.0, 48.0 ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 47.0, -177.0, 52.0 ], "geometry": { "type": "LineString", "coordinates": [ [ -178.0, 52.0 ], [ -177.0, 47.0 ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, 178.0, 51.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 178.0, 48.0 ], [ 177.0, 48.0 ], [ 177.0, 51.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 48.0, -177.0, 51.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -177.0, 51.0 ], [ -177.0, 48.0 ], [ -178.0, 48.0 ], [ -177.0, 48.0 ], [ -177.0, 51.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test geometries across the antimeridian
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[179, 51],[-179, 48]] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[-179, 52],[179, 47]] } },
      { "type": "Feature", "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[177, 51],[-175, 51],[-175, 48],[177, 48],[177, 51]]] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ] ] } }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [172.0, 47.0, -162.0, 52.0],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 48.0, -179.0, 51.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 47.0, -179.0, 52.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ -179.0, 52.0 ], [ -180.0, 49.5 ] ], [ [ 180.0, 49.5 ], [ 179.0, 47.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 48.0, -179.0, 51.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, -175.0, 51.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, -175.0, 51.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } },
{ "type": "Feature", "bbox": [ 172, 51, -162, 52 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ] ] } }
]
}
"""

    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 6
    for i in range(len(j_expected["features"])):
        assert j_got["features"][i]["bbox"] == j_expected["features"][i]["bbox"], i
        try:
            ogrtest.check_feature_geometry(
                ogr.CreateGeometryFromJson(
                    json.dumps(j_got["features"][i]["geometry"])
                ),
                ogr.CreateGeometryFromJson(
                    json.dumps(j_expected["features"][i]["geometry"])
                ),
            )
        except AssertionError as e:
            pytest.fail("At geom %d: %s" % (i, str(e)))

    # Test geometries that defeats antimeridian heuristics
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ -80, 10 ], [-60, 10], [-60, 11], [-80, 10] ] ] ] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ 80, 10 ], [60, 10], [60, 11], [80, 10] ] ] ] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ 10, 10 ], [120, 10], [120, 11], [10, 10] ] ] ] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ -91, 10 ], [-80, 10], [-80, 11], [-91, 10] ] ] ] } }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [-163.0, 10.0, 173.0, 52.0],
"features": [
{ "type": "Feature", "bbox": [ -163.0, 10.0, 173.0, 52.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ -80, 10 ], [-60, 10], [-60, 11], [-80, 10] ] ] ] } },
{ "type": "Feature", "bbox": [ -163.0, 10.0, 173.0, 52.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ 80, 10 ], [60, 10], [60, 11], [80, 10] ] ] ] } },
{ "type": "Feature", "bbox": [ -163.0, 10.0, 173.0, 52.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ 10, 10 ], [120, 10], [120, 11], [10, 10] ] ] ] } },
{ "type": "Feature", "bbox": [ -163.0, 10.0, 173.0, 52.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 172, 52 ], [ 173, 52 ], [ 173, 51 ], [ 172, 51 ], [ 172, 52 ] ] ], [ [ [ -162, 52 ], [ -163, 52 ], [ -163, 51 ], [ -162, 51 ], [ -162, 52 ] ] ], [ [ [ -91, 10 ], [-80, 10], [-80, 11], [-91, 10] ] ] ] } }
]
}
"""

    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 4
    for i in range(len(j_expected["features"])):
        assert j_got["features"][i]["bbox"] == j_expected["features"][i]["bbox"], i
        try:
            ogrtest.check_feature_geometry(
                ogr.CreateGeometryFromJson(
                    json.dumps(j_got["features"][i]["geometry"])
                ),
                ogr.CreateGeometryFromJson(
                    json.dumps(j_expected["features"][i]["geometry"])
                ),
            )
        except AssertionError as e:
            pytest.fail("At geom %d: %s" % (i, str(e)))


@pytest.mark.require_geos
def test_ogr_geojson_56_world(tmp_vsimem):

    # Test polygon geometry that covers the whole world (#2833)
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[-180,-90.0],[180,-90.0],[180,90.0],[-180,90.0],[-180,-90.0]]]} }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, -90.0000000, 180.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, -90.0, 180.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -180.0, -90.0 ], [ 180.0, -90.0 ], [ 180.0, 90.0 ], [ -180.0, 90.0 ], [ -180.0, -90.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)


@pytest.mark.require_geos
def test_ogr_geojson_56_next(tmp_vsimem):

    # Test polygon geometry with one longitude at +/- 180deg (#6250)
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[-180,50],[179.5,50.0],[179.5,40],[-180,45],[-180,50]]]} }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 179.5000000, 40.0000000, 180.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.5, 40.0, 180.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 179.5, 40.0 ], [ 180.0, 45.0 ], [ 180.0, 50.0 ], [ 179.5, 50.0 ], [ 179.5, 40.0 ] ] ] } }
]
}
"""
    expected_older_geos = """{
"type": "FeatureCollection",
"bbox": [ 179.5000000, 40.0000000, 180.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.5, 40.0, 180.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 180.0, 45.0 ], [ 180.0, 50.0 ], [ 179.5, 50.0 ], [ 179.5, 40.0 ], [ 180.0, 45.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected) or json.loads(got) == json.loads(
        expected_older_geos
    )

    # Test WRAPDATELINE=NO (#6250)
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{"type":"LineString","coordinates":[[179,50],[-179,50]]}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES", "WRAPDATELINE=NO"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -179.0, 50.0, 179.0, 50.0 ], "geometry": { "type": "LineString", "coordinates": [ [ 179.0, 50.0 ], [ -179.0, 50.0 ] ] } }
],
"bbox": [ -179.0000000, 50.0000000, 179.0000000, 50.0000000 ]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test line geometry with one longitude at +/- 180deg (#8645)
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"LineString","coordinates":[[-179,0],[-180,0],[179,0]]} }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 179.0000000, 0.0000000, -179.0000000, 0.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 0.0, -179.0, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ -179.0, 0.0 ], [ -180.0, 0.0 ] ], [ [ 180.0, 0.0 ], [ 179.0, 0.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test line geometry with one longitude at +/- 180deg (#8645)
    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"LineString","coordinates":[[179,0],[180,0],[-179,0]]} }
  ]
}""",
        format="GeoJSON",
        layerCreationOptions=["RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 179.0000000, 0.0000000, -179.0000000, 0.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 0.0, -179.0, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 0.0 ], [ 180.0, 0.0 ] ], [ [ -180.0, 0.0 ], [ -179.0, 0.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)


###############################################################################
# Test RFC 7946 and reprojection


@pytest.mark.require_geos
def test_ogr_geojson_57(tmp_vsimem):

    # Standard case: EPSG:32662: WGS 84 / Plate Carre
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -17.9663057, -17.9663057, 17.9663057, 17.9663057 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -17.9663057, -17.9663057, 17.9663057, 17.9663057 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 17.9663057, 17.9663057 ], [ -17.9663057, 17.9663057 ], [ -17.9663057, -17.9663057 ], [ 17.9663057, -17.9663057 ], [ 17.9663057, 17.9663057 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Polar case: EPSG:3995: WGS 84 / Arctic Polar Stereographic
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((-2000000 -2000000,-1000000 -2000000,-1000000 2000000,-2000000 2000000,-2000000 -2000000))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, 64.3861643, 180.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, 64.3861643, 180.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 135.0, 64.3861643 ], [ 180.0, 71.7425119 ], [ 180.0, 90.0 ], [ -180.0, 90.0 ], [ -180.0, 71.7425119 ], [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ], [ 45.0, 64.3861643 ], [ 135.0, 64.3861643 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -153.4349488, 64.3861643, -26.5650512, 69.6286694 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -45.0, 64.3861643 ], [ -26.5650512, 69.6286694 ], [ -153.4349488, 69.6286694 ], [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ] ] ] } }
]
}
"""

    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 2
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"])),
    )

    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][1]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][1]["geometry"])),
    )

    # Polar case: slice of spherical cap (not intersecting antimeridian, west hemisphere)
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((-2000000 2000000,0 0,-2000000 -2000000,-2000000 2000000))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -135.0000000, 64.3861643, -45.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -135.0, 64.3861643, -45.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ], [ -45.0, 90.0 ], [ -135.0, 90.0 ], [ -135.0, 64.3861643 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Polar case: slice of spherical cap (not intersecting antimeridian, east hemisphere)
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((2000000 2000000,0 0,2000000 -2000000,2000000 2000000)))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = {
        "type": "FeatureCollection",
        "bbox": [45.0, 64.3861643, 135.0, 90.0],
        "features": [
            {
                "type": "Feature",
                "properties": {},
                "bbox": [45.0, 64.3861643, 135.0, 90.0],
                "geometry": {
                    "type": "MultiPolygon",
                    "coordinates": [
                        [
                            [
                                [135.0, 64.3861643],
                                [135.0, 90.0],
                                [45.0, 90.0],
                                [45.0, 64.3861643],
                                [135.0, 64.3861643],
                            ]
                        ]
                    ],
                },
            }
        ],
    }
    assert json.loads(got) == expected

    # Polar case: slice of spherical cap crossing the antimeridian
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((100000 100000,-100000 100000,0 0,100000 100000))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 135.0000000, 88.6984598, -135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 135.0, 88.6984598, -135.0, 90.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 180.0, 89.0796531 ], [ 180.0, 90.0 ], [ 135.0, 88.6984598 ], [ 180.0, 89.0796531 ] ] ], [ [ [ -180.0, 90.0 ], [ -180.0, 89.0796531 ], [ -135.0, 88.6984598 ] ] ] ] } }
]
}
"""
    expected_geos_overlay_ng = """{
"type": "FeatureCollection",
"bbox": [ 135.0000000, 88.6984598, -135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 135.0, 88.6984598, -135.0, 90.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ -135.0, 88.6984598 ], [ -180.0, 90.0 ], [ -180.0, 89.0796531 ], [ -135.0, 88.6984598 ] ] ], [ [ [ 180.0, 90.0 ], [ 135.0, 88.6984598 ], [ 180.0, 89.0796531 ], [ 180.0, 90.0 ] ] ] ] } }
]
}"""
    expected_geos_3_9_1 = """{
"type": "FeatureCollection",
"bbox": [ 135.0000000, 88.6984598, -135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 135.0, 88.6984598, -135.0, 90.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 135.0, 88.6984598 ], [ 180.0, 89.0796531 ], [ 180.0, 90.0 ], [ 135.0, 88.6984598 ] ] ], [ [ [ -135.0, 88.6984598 ], [ -180.0, 90.0 ], [ -180.0, 89.0796531 ], [ -135.0, 88.6984598 ] ] ] ] } }
]
}"""
    if (
        ogr.GetGEOSVersionMajor() * 10000
        + ogr.GetGEOSVersionMinor() * 100
        + ogr.GetGEOSVersionMicro()
        >= 30900
    ):
        assert (
            json.loads(got) == json.loads(expected)
            or json.loads(got) == json.loads(expected_geos_overlay_ng)
            or json.loads(got) == json.loads(expected_geos_3_9_1)
        ), got

    # Polar case: EPSG:3031: WGS 84 / Antarctic Polar Stereographic
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        "+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
    )
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000)))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, -90.0000000, 180.0000000, -64.3861643 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, -90.0, 180.0, -64.3861643 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 45.0, -64.3861643 ], [ -45.0, -64.3861643 ], [ -135.0, -64.3861643 ], [ -180.0, -71.7425119 ], [ -180.0, -90.0 ], [ 180.0, -90.0 ], [ 180.0, -71.7425119 ], [ 135.0, -64.3861643 ], [ 45.0, -64.3861643 ] ] ] } }
]
}
"""
    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 1
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"])),
    )

    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N with polygon and line crossing
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput("+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs")
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((670000 4000000,850000 4000000,850000 4100000,670000 4100000,670000 4000000))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("MULTILINESTRING((670000 4000000,850000 4100000))")
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(670000 0,850000 0)"))
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 178.5275649, 0.0000000, -179.0681936, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.0816324, -179.0681936, 37.0308258 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 180.0, 36.1071354 ], [ 180.0, 36.1071354 ], [ 180.0, 37.0082839 ], [ 180.0, 37.0082839 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ], [ 180.0, 36.1071354 ] ] ], [ [ [ -180.0, 37.0082839 ], [ -180.0, 36.1071354 ], [ -180.0, 36.1071354 ], [ -179.1135277, 36.0816324 ], [ -179.0681936, 36.9810434 ], [ -180.0, 37.0082839 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1298163, -179.0681936, 36.9810434 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 180.0, 36.5995612 ] ], [ [ -180.0, 36.5995612 ], [ -179.0681936, 36.9810434 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.5275649, 0.0, -179.8562277, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.5275649, 0.0 ], [ 180.0, 0.0 ] ], [ [ -180.0, 0.0 ], [ -179.8562277, 0.0 ] ] ] } }
]
}
"""
    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 3
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"])),
    )

    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][1]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][1]["geometry"])),
    )

    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(json.dumps(j_got["features"][2]["geometry"])),
        ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][2]["geometry"])),
    )

    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N with polygon on west of antimeridian
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput("+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs")
    lyr = src_ds.CreateLayer("test", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((670000 4000000,700000 4000000,700000 4100000,670000 4100000,670000 4000000))"
        )
    )
    lyr.CreateFeature(f)

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["WRITE_NAME=NO", "RFC7946=YES", "WRITE_BBOX=YES"],
    )

    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    expected = """{
"type": "FeatureCollection",
"bbox": [ 178.8892102, 36.1240958, 179.2483693, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1240958, 179.2483693, 37.0308258 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 179.2223914, 36.1240958 ], [ 179.2483693, 37.0249155 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)


###############################################################################
# Test using the name member of FeatureCollection


def test_ogr_geojson_58(tmp_vsimem):

    ds = ogr.Open(
        '{ "type": "FeatureCollection", "name": "layer_name", "features": []}'
    )
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayerByName("layer_name")
    assert lyr is not None, "Missing layer called layer_name"
    ds = None

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_58.json"
    )
    lyr = ds.CreateLayer("foo")
    ds = None
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_58.json")
    assert ds.GetLayerByName("foo") is not None, "Missing layer called foo"
    ds = None


###############################################################################
# Test using the name member of FeatureCollection


def test_ogr_geojson_empty_name():

    with ogr.Open('{ "type": "FeatureCollection", "name": "", "features": []}') as ds:
        assert ds.GetLayer(0).GetName() == "OGRGeoJSON"


###############################################################################
# Test using the description member of FeatureCollection


def test_ogr_geojson_59(tmp_vsimem):

    ds = ogr.Open(
        '{ "type": "FeatureCollection", "description": "my_description", "features": []}'
    )
    assert ds is not None, "Failed to open datasource"

    lyr = ds.GetLayer(0)
    assert (
        lyr.GetMetadataItem("DESCRIPTION") == "my_description"
    ), "Did not get DESCRIPTION"
    ds = None

    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "ogr_geojson_59.json"
    )
    lyr = ds.CreateLayer("foo", options=["DESCRIPTION=my desc"])
    ds = None
    ds = ogr.Open(tmp_vsimem / "ogr_geojson_59.json")
    lyr = ds.GetLayerByName("foo")
    assert lyr.GetMetadataItem("DESCRIPTION") == "my desc", "Did not get DESCRIPTION"
    ds = None


###############################################################################
# Test null vs unset field


def test_ogr_geojson_60(tmp_vsimem):

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "features": [
{ "type": "Feature", "properties" : { "foo" : "bar" } },
{ "type": "Feature", "properties" : { "foo": null } },
{ "type": "Feature", "properties" : {  } } ] }""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f["foo"] != "bar":
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if not f.IsFieldNull("foo"):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.IsFieldSet("foo"):
        f.DumpReadable()
        pytest.fail()

    # Test writing side
    gdal.VectorTranslate(tmp_vsimem / "ogr_geojson_60.json", ds, format="GeoJSON")

    with gdal.VSIFile(tmp_vsimem / "ogr_geojson_60.json", "rb") as f:
        data = f.read()

    gdal.Unlink(tmp_vsimem / "ogr_geojson_60.json")
    assert (
        b'"properties":{"foo":"bar"}' in data
        and b'"properties":{"foo":null}' in data
        and b'"properties":{}' in data
    )


###############################################################################
# Test corner cases


def test_ogr_geojson_61(tmp_vsimem):

    # Invalid JSon
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_61.json",
        """{ "type": "FeatureCollection", "features": [""",
    )
    with pytest.raises(Exception):
        ds = gdal.OpenEx(tmp_vsimem / "ogr_geojson_61.json")
    gdal.Unlink(tmp_vsimem / "ogr_geojson_61.json")

    # Invalid single geometry
    with pytest.raises(Exception):
        ds = gdal.OpenEx("""{ "type": "Point", "x" : { "coordinates" : null } } """)

    # Empty property name
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_geojson_61.json",
        """{ "type": "FeatureCollection", "features": [ { "type": "Feature", "properties": {"": 1}, "geometry": null }] }""",
    )
    ds = gdal.OpenEx(tmp_vsimem / "ogr_geojson_61.json")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField("") == 1
    ds = None
    gdal.Unlink(tmp_vsimem / "ogr_geojson_61.json")


###############################################################################
# Test crs object


def test_ogr_geojson_62():

    # crs type=name tests
    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name" }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":null }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":1 }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":null} }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":1} }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":"x"} }, "features":[] }"""
    )

    ds = gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name": "urn:ogc:def:crs:EPSG::32631"} }, "features":[] }"""
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "32631"
    assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]

    # See https://github.com/OSGeo/gdal/issues/2035
    ds = gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name": "urn:ogc:def:crs:OGC:1.3:CRS84"} }, "features":[] }"""
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]

    # crs type=EPSG (not even documented in GJ2008 spec!) tests. Just for coverage completeness
    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"EPSG" }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":null }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":1 }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":null} }, "features":[] }"""
    )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":1} }, "features":[] }"""
        )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":"x"} }, "features":[] }"""
        )

    ds = gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code": 32631} }, "features":[] }"""
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.ExportToWkt().find("32631") >= 0

    # crs type=link tests
    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"link" }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"link", "properties":null }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"link", "properties":1 }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":null} }, "features":[] }"""
    )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":1} }, "features":[] }"""
        )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href": "1"} }, "features":[] }"""
        )

    # crs type=OGC (not even documented in GJ2008 spec!) tests. Just for coverage completeness
    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"OGC" }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":null }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":1 }, "features":[] }"""
    )

    gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":null} }, "features":[] }"""
    )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":1} }, "features":[] }"""
        )

    with gdal.quiet_errors():
        gdal.OpenEx(
            """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":"x"} }, "features":[] }"""
        )

    ds = gdal.OpenEx(
        """{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn": "urn:ogc:def:crs:EPSG::32631"} }, "features":[] }"""
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.ExportToWkt().find("32631") >= 0


###############################################################################
# Extensive test of field type promotion


def test_ogr_geojson_63():

    ds_ref = ogr.Open("data/geojson/test_type_promotion_ref.json")
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open("data/geojson/test_type_promotion.json")
    lyr = ds.GetLayer(0)
    ogrtest.compare_layers(lyr, lyr_ref)


###############################################################################
# Test exporting XYM / XYZM (#6935)


def test_ogr_geojson_64():

    g = ogr.CreateGeometryFromWkt("POINT ZM(1 2 3 4)")
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(g.ExportToJson()),
        "POINT Z(1 2 3)",
    )

    g = ogr.CreateGeometryFromWkt("POINT M(1 2 3)")
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(g.ExportToJson()),
        "POINT (1 2)",
    )

    g = ogr.CreateGeometryFromWkt("LINESTRING ZM(1 2 3 4,5 6 7 8)")
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(g.ExportToJson()),
        ogr.CreateGeometryFromWkt("LINESTRING Z(1 2 3,5 6 7)"),
    )

    g = ogr.CreateGeometryFromWkt("LINESTRING M(1 2 3,4 5 6)")
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromJson(g.ExportToJson()),
        ogr.CreateGeometryFromWkt("LINESTRING (1 2,4 5)"),
    )


###############################################################################
# Test feature geometry CRS when CRS set on the FeatureCollection
# See https://github.com/r-spatial/sf/issues/449#issuecomment-319369945


def test_ogr_geojson_65():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::32631" } },
"features": [{
"type": "Feature",
"geometry": {
"type": "Point",
"coordinates": [500000,4500000]},
"properties": {
}}]}""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    srs = f.GetGeometryRef().GetSpatialReference()
    pcs = int(srs.GetAuthorityCode("PROJCS"))
    assert pcs == 32631, "Spatial reference for individual geometry was not valid"


###############################################################################
# Test features with properties not being a dictionary


def test_ogr_geojson_66():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"features": [
{
    "type": "Feature",
    "geometry": null,
    "properties": null
},
{
    "type": "Feature",
    "geometry": null,
    "properties": []
}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################
# Test reading GeoJSON files starting with {"features":[{"geometry":.... (#7198)


def test_ogr_geojson_67():

    ds = ogr.Open("data/geojson/grenada.geojson")
    assert ds is not None
    assert ds.GetDriver().GetName() == "GeoJSON"
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################


def test_ogr_geojson_id_field_and_id_type(tmp_vsimem):

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        options="-f GeoJSON -lco ID_TYPE=String -preserve_fid -limit 1 -fid 2",
    )
    got = read_file(tmp_vsimem / "out.json")
    assert (
        '"id":"2","properties":{"AREA":261752.781,"EAS_ID":171,"PRFEDEA":"35043414"}'
        in got
    )

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        options="-f GeoJSON -lco ID_TYPE=Integer -preserve_fid -limit 1 -fid 2",
    )
    got = read_file(tmp_vsimem / "out.json")
    assert (
        '"id":2,"properties":{"AREA":261752.781,"EAS_ID":171,"PRFEDEA":"35043414"}'
        in got
    )

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_FIELD=EAS_ID"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    assert '"id":168,"properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(tmp_vsimem / "out2.json", src_ds, format="GeoJSON")
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":168,"properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(
        tmp_vsimem / "out2.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["ID_TYPE=String"],
    )
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":"168","properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(
        tmp_vsimem / "out2.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["ID_TYPE=Integer"],
    )
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":168,"properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    gdal.Unlink(tmp_vsimem / "out.json")

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_FIELD=EAS_ID", "ID_TYPE=String"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    assert '"id":"168","properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(tmp_vsimem / "out2.json", src_ds, format="GeoJSON")
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":"168","properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(
        tmp_vsimem / "out2.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["ID_TYPE=String"],
    )
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":"168","properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    src_ds = gdal.OpenEx(tmp_vsimem / "out.json", open_options=["NATIVE_DATA=YES"])
    gdal.VectorTranslate(
        tmp_vsimem / "out2.json",
        src_ds,
        format="GeoJSON",
        layerCreationOptions=["ID_TYPE=Integer"],
    )
    src_ds = None
    got = read_file(tmp_vsimem / "out2.json")
    gdal.Unlink(tmp_vsimem / "out2.json")
    assert '"id":168,"properties":{"AREA":215229.266,"PRFEDEA":"35043411"}' in got

    gdal.Unlink(tmp_vsimem / "out.json")

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_FIELD=PRFEDEA"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    assert '"id":"35043411","properties":{"AREA":215229.266,"EAS_ID":168}' in got

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_FIELD=PRFEDEA", "ID_TYPE=Integer"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    gdal.Unlink(tmp_vsimem / "out.json")
    assert '"id":35043411,"properties":{"AREA":215229.266,"EAS_ID":168}' in got

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_GENERATE=YES"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    assert (
        '"id":0,"properties":{"AREA":215229.266,"EAS_ID":168,"PRFEDEA":"35043411"}'
        in got
    )

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_GENERATE=YES", "ID_TYPE=Integer"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    assert (
        '"id":0,"properties":{"AREA":215229.266,"EAS_ID":168,"PRFEDEA":"35043411"}'
        in got
    )

    gdal.VectorTranslate(
        tmp_vsimem / "out.json",
        "data/poly.shp",
        format="GeoJSON",
        layerCreationOptions=["ID_GENERATE=YES", "ID_TYPE=String"],
        limit=1,
    )
    got = read_file(tmp_vsimem / "out.json")
    assert (
        '"id":"0","properties":{"AREA":215229.266,"EAS_ID":168,"PRFEDEA":"35043411"}'
        in got
    )


###############################################################################


@gdaltest.disable_exceptions()
def test_ogr_geojson_geom_export_failure():

    g = ogr.CreateGeometryFromWkt("POINT EMPTY")
    geojson = g.ExportToJson()
    assert geojson is None

    g = ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION(TIN EMPTY)")
    with gdal.quiet_errors():
        geojson = json.loads(g.ExportToJson())
    assert geojson == {"type": "GeometryCollection", "geometries": None}

    g = ogr.Geometry(ogr.wkbLineString)
    g.AddPoint_2D(float("nan"), 0)
    with gdal.quiet_errors():
        geojson = g.ExportToJson()
    assert geojson is None

    g = ogr.Geometry(ogr.wkbPolygon)
    lr = ogr.Geometry(ogr.wkbLinearRing)
    lr.AddPoint_2D(0, 0)
    lr.AddPoint_2D(0, 1)
    lr.AddPoint_2D(1, 1)
    lr.AddPoint_2D(0, 0)
    g.AddGeometry(lr)
    lr = ogr.Geometry(ogr.wkbLinearRing)
    lr.AddPoint_2D(0, 0)
    lr.AddPoint_2D(float("nan"), 1)
    lr.AddPoint_2D(1, 1)
    lr.AddPoint_2D(0, 0)
    g.AddGeometry(lr)
    with gdal.quiet_errors():
        geojson = g.ExportToJson()
    assert geojson is None


###############################################################################


def test_ogr_geojson_starting_with_crs():

    ds = ogr.Open("""{
"crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::32631" } },
"type": "FeatureCollection",
"features": [{
"type": "Feature",
"geometry": {
"type": "Point",
"coordinates": [500000,4500000]},
"properties": {
}}]}""")
    assert ds is not None


###############################################################################
# Test we properly flush the file in SyncToDisk() in append situations


def test_ogr_geojson_append_flush(tmp_path):

    tmpfilename = tmp_path / "ogr_geojson_append_flush.json"
    f = gdal.VSIFOpenL(tmpfilename, "wb")
    content = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "properties": { "x": 1, "y": 2, "z": 3, "w": 4 }, "geometry": { "type": "Point", "coordinates": [ 0, 0 ] } } ] }"""
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmpfilename, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["x"] = 10
    lyr.CreateFeature(f)
    lyr.SyncToDisk()

    ds2 = ogr.Open(tmpfilename, update=1)
    lyr = ds2.GetLayer(0)
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f is not None and f["x"] == 10

    ds = None
    ds2 = None


###############################################################################


def test_ogr_geojson_empty_geometrycollection():

    g = ogr.CreateGeometryFromJson('{"type": "GeometryCollection", "geometries": []}')
    assert g.ExportToWkt() == "GEOMETRYCOLLECTION EMPTY"


###############################################################################


def test_ogr_geojson_read_fields_with_different_case():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": "my_id", "geometry": null, "properties":
                                { "ID": "MY_ID", "x": "foo", "X": "FOO"} }
]}""")

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if (
        f.GetField(0) != "my_id"
        or f.GetField(1) != "MY_ID"
        or f.GetField(2) != "foo"
        or f.GetField(3) != "FOO"
    ):
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1068


@pytest.mark.require_geos
def test_ogr_geojson_clip_geometries_rfc7946(tmp_vsimem):

    tmpfilename = tmp_vsimem / "out.json"
    gdal.VectorTranslate(
        tmpfilename,
        """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[-220,-20],[-220,30],[16,30],[16,-20],[-220,-20]]]} },
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[220,40],[220,70],[-16,70],[-16,40],[220,40]]]} },
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[170,-40],[170,-70],[-16,70],[-16,-40],[170,-40]]]} }
  ]
}""",
        options="-f GeoJSON -lco RFC7946=YES",
    )

    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON (((-180 30,-180 -20,16 -20,16 30,-180 30)),((140 -20,180 -20,180 30,140 30,140 -20)))"
    )
    ogrtest.check_feature_geometry(f, ref_geom)

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON (((180 40,180 70,-16 70,-16 40,180 40)),((-180 70,-180 40,-140 40,-140 70,-180 70)))"
    )
    ogrtest.check_feature_geometry(f, ref_geom)

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((170 -40,-16 -40,-16 70,170 -70,170 -40))"
    )
    ogrtest.check_feature_geometry(f, ref_geom)

    ds = None


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1109


def test_ogr_geojson_non_finite(tmp_vsimem):

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "properties": { "inf_prop": infinity, "minus_inf_prop": -infinity, "nan_prop": nan }, "geometry": null }
  ]
}"""
    with gdal.quiet_errors():
        ds = ogr.Open(json_content)
    if ds is None:
        # Might fail with older libjson-c versions
        pytest.skip()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(3):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTReal

    if f["inf_prop"] != float("inf"):
        f.DumpReadable()
        pytest.fail()
    if f["minus_inf_prop"] != float("-inf"):
        f.DumpReadable()
        pytest.fail()
    if not math.isnan(f["nan_prop"]):
        f.DumpReadable()
        pytest.fail(str(f["nan_prop"]))
    ds = None

    tmpfilename = tmp_vsimem / "out.json"

    with gdal.quiet_errors():
        gdal.VectorTranslate(tmpfilename, json_content, options="-f GeoJSON")
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0
    ds = None

    gdal.VectorTranslate(
        tmpfilename, json_content, options="-f GeoJSON -lco WRITE_NON_FINITE_VALUES=YES"
    )
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    if f["inf_prop"] != float("inf"):
        f.DumpReadable()
        pytest.fail()
    if f["minus_inf_prop"] != float("-inf"):
        f.DumpReadable()
        pytest.fail()
    if not math.isnan(f["nan_prop"]):
        f.DumpReadable()
        pytest.fail(str(f["nan_prop"]))
    ds = None


###############################################################################
# Test writing fields with and without automatic JSON interpretation


def test_ogr_geojson_json_string_autodetect(tmp_vsimem):

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "properties": { "jsonish": "[5]" }, "geometry": null }
  ]
}"""
    with gdal.quiet_errors():
        ds = ogr.Open(json_content)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(1):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTString
    assert f["jsonish"] == "[5]"
    ds = None

    tmpfilename = tmp_vsimem / "out.json"

    with gdal.quiet_errors():
        gdal.VectorTranslate(tmpfilename, json_content, options="-f GeoJSON")
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTIntegerList
    ds = None
    gdal.Unlink(tmpfilename)

    with gdal.quiet_errors():
        gdal.VectorTranslate(
            tmpfilename,
            json_content,
            options="-f GeoJSON -lco AUTODETECT_JSON_STRINGS=FALSE",
        )
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f["jsonish"] == "[5]"
    ds = None


###############################################################################


def test_ogr_geojson_random_reading_with_id(tmp_vsimem):

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "id": 1, "properties": { "a": "a" }, "geometry": null },
      { "type": "Feature", "id": 2, "properties": { "a": "bc" }, "geometry": null }
  ]
}"""
    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f2_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(1)
    f2 = lyr.GetFeature(2)
    assert f1.Equal(f1_ref)
    assert f2.Equal(f2_ref)
    assert not lyr.GetFeature(3)
    ds = None


###############################################################################


def test_ogr_geojson_random_reading_without_id(tmp_vsimem):

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "properties": { "a": "a" }, "geometry": null },
      { "type": "Feature", "properties": { "a": "bc" }, "geometry": null }
  ]
}"""
    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f2_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(0)
    f2 = lyr.GetFeature(1)
    assert f1.Equal(f1_ref)
    assert f2.Equal(f2_ref)
    assert not lyr.GetFeature(2)
    ds = None


###############################################################################


def test_ogr_geojson_single_feature_random_reading_with_id(tmp_vsimem):

    json_content = """
      { "type": "Feature", "id": 1, "properties": { "a": "a" }, "geometry": null }
}"""
    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(1)
    assert f1.Equal(f1_ref)
    ds = None


###############################################################################


def test_ogr_geojson_3D_geom_type():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,3]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,4]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,3]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,4]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D


###############################################################################


def test_ogr_geojson_update_in_loop(tmp_vsimem):

    tmpfilename = tmp_vsimem / "temp.json"

    # No explicit id
    gdal.FileFromMemBuffer(
        tmpfilename,
        '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "geometry": null }]}',
    )
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [0, 1]
    ds = None

    # Explicit id no holes
    gdal.FileFromMemBuffer(
        tmpfilename,
        '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "id": 0, "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "id": 1, "geometry": null }]}',
    )

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [0, 1]
    ds = None

    # Explicit id with holes
    gdal.FileFromMemBuffer(
        tmpfilename,
        '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "id": 1, "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "id": 3, "geometry": null }]}',
    )
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [1, 3]
    ds = None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2720


def test_ogr_geojson_starting_with_coordinates(tmp_vsimem):

    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(
        tmpfilename, '{ "coordinates": [' + (" " * 10000) + '2,49], "type": "Point"}'
    )
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2787


def test_ogr_geojson_starting_with_geometry_coordinates(tmp_vsimem):

    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(
        tmpfilename,
        '{ "geometry": {"coordinates": ['
        + (" " * 10000)
        + '2,49], "type": "Point"}, "type": "Feature", "properties": {} }',
    )
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None


###############################################################################
# Test fix for https://github.com/qgis/QGIS/issues/61266


@pytest.mark.parametrize(
    "start,end",
    [
        ('{"type":"Point","coordinates":[', "2,49]}"),
        ('{"type":"LineString","coordinates":[[', "2,49],[3,50]]}"),
        ('{"type":"Polygon","coordinates":[[[', "0,0],[0,1],[1,1],[0,0]]]}"),
        ('{"type":"MultiPoint","coordinates":[[', "2,49]]}"),
        ('{"type":"MultiLineString","coordinates":[[[', "2,49],[3,50]]]}"),
        ('{"type":"MultiPolygon","coordinates":[[[[', "0,0],[0,1],[1,1],[0,0]]]]}"),
        ('{"type":"GeometryCollection","geometries":[', "]}"),
    ],
)
def test_ogr_geojson_starting_with_geometry_type(tmp_vsimem, start, end):

    tmpfilename = tmp_vsimem / "temp.json"
    gdal.FileFromMemBuffer(
        tmpfilename,
        '{ "geometry":'
        + start
        + (" " * 10000)
        + end
        + ', "type":"Feature","properties":{}}',
    )
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None


###############################################################################
# Test serialization of Float32 values


def test_ogr_geojson_write_float32(tmp_vsimem):
    def cast_as_float(x):
        return struct.unpack("f", struct.pack("f", x))[0]

    filename = tmp_vsimem / "test_ogr_geojson_write_float32.json"
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")

    fldn_defn = ogr.FieldDefn("float32", ogr.OFTReal)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    fldn_defn = ogr.FieldDefn("float32list", ogr.OFTRealList)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["float32"] = cast_as_float(0.35)
    f["float32list"] = [
        cast_as_float(123.0),
        cast_as_float(0.35),
        cast_as_float(0.15),
        cast_as_float(0.12345678),
        cast_as_float(1.2345678e-15),
        cast_as_float(1.2345678e15),
        cast_as_float(0.123456789),  # more decimals than Float32 can hold
    ]
    lyr.CreateFeature(f)

    ds = None

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read().decode("utf-8")

    gdal.Unlink(filename)

    data = data.replace("e+0", "e+").replace("e-0", "e-")

    assert '"float32":0.35,' in data
    assert (
        '"float32list":[123.0,0.35,0.15,0.12345678,1.2345678e-15,1.2345678e+15,0.12345679]'
        in data
    )


###############################################################################
# Test bugfix for #3172


def test_ogr_geojson_write_float_exponential_without_dot(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_write_float_exponential_without_dot.json"
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")

    fldn_defn = ogr.FieldDefn("float32", ogr.OFTReal)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    fldn_defn = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(fldn_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["float32"] = 1e-7
    f["float64"] = 1e-8
    lyr.CreateFeature(f)

    ds = None

    fp = gdal.VSIFOpenL(filename, "rb")
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    # Check that the json can be parsed
    json.loads(data)


###############################################################################
# Test bugfix for #3280


def test_ogr_geojson_feature_starting_with_big_properties(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_feature_starting_with_big_properties.json"
    gdal.FileFromMemBuffer(
        filename,
        '{"properties":{"foo":"%s"},"type":"Feature","geometry":null}' % ("x" * 10000),
    )
    assert ogr.Open(filename) is not None


###############################################################################


def test_ogr_geojson_export_geometry_axis_order():

    # EPSG:4326 and lat,long data order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    sr.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    g = ogr.CreateGeometryFromWkt("POINT (49 2)")
    g.AssignSpatialReference(sr)
    before_wkt = g.ExportToWkt()
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}
    assert g.ExportToWkt() == before_wkt

    # EPSG:4326 and long,lat data order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    g = ogr.CreateGeometryFromWkt("POINT (2 49)")
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}

    # CRS84 with long,lat CRS and data order
    sr = osr.SpatialReference()
    sr.SetFromUserInput("OGC:CRS84")
    g = ogr.CreateGeometryFromWkt("POINT (2 49)")
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}

    # Projected CRS with easting, northing order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    g = ogr.CreateGeometryFromWkt("POINT (2 49)")
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}

    # Projected CRS with northing, easting order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2393)
    sr.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    g = ogr.CreateGeometryFromWkt("POINT (49 2)")
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}

    # No CRS
    g = ogr.CreateGeometryFromWkt("POINT (2 49)")
    assert json.loads(g.ExportToJson()) == {"type": "Point", "coordinates": [2.0, 49.0]}


###############################################################################


def test_ogr_geojson_sparse_fields():

    ds = ogr.Open("data/geojson/sparse_fields.geojson")
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    field_names = [
        lyr_defn.GetFieldDefn(i).GetName() for i in range(lyr_defn.GetFieldCount())
    ]
    assert field_names == ["C", "B", "A", "D", "E_prev", "E", "E_next", "F", "X"]


###############################################################################


@pytest.mark.parametrize("filename", ["point.geojson", "featurecollection_point.json"])
def test_ogr_geojson_crs_4326(filename):

    ds = ogr.Open("data/geojson/" + filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################


@pytest.mark.parametrize("filename", ["pointz.json", "featurecollection_pointz.json"])
def test_ogr_geojson_crs_4979(filename):

    ds = ogr.Open("data/geojson/" + filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4979"
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1, 3]


###############################################################################


def test_ogr_geojson_write_rfc7946_from_3D_crs(tmp_vsimem):

    srs_4979 = osr.SpatialReference()
    srs_4979.ImportFromEPSG(4979)
    srs_4979.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    srs_4326_5773 = osr.SpatialReference()
    srs_4326_5773.SetFromUserInput("EPSG:4326+5773")
    srs_4326_5773.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(srs_4979, srs_4326_5773)
    ellipsoidal_height = 100
    lon, lat, z = ct.TransformPoint(2, 49, ellipsoidal_height)
    # If we have the egm96 grid, then z should be different from 100

    filename = tmp_vsimem / "out.geojson"
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    lyr = ds.CreateLayer("out", srs=srs_4326_5773, options=["RFC7946=YES"])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(%.17g %.17g %.17g)" % (lon, lat, z)))
    lyr.CreateFeature(f)
    ds = None

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    # Check that we get back the ellipsoidal height
    assert b'"coordinates":[2.0,49.0,100.0' in data


###############################################################################
# Test effect of OGR_GEOJSON_MAX_OBJ_SIZE


@gdaltest.disable_exceptions()
def test_ogr_geojson_feature_large(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_feature_large.json"
    gdal.FileFromMemBuffer(
        filename,
        '{"type":"FeatureCollection","features":[{"type":"Feature","properties":{},"geometry":{"type":"LineString","coordinates":[%s]}}]}'
        % ",".join(["[0,0]" for _ in range(10 * 1024)]),
    )
    assert ogr.Open(filename) is not None
    with gdaltest.config_option("OGR_GEOJSON_MAX_OBJ_SIZE", "0"):
        assert ogr.Open(filename) is not None
    with gdaltest.config_option("OGR_GEOJSON_MAX_OBJ_SIZE", "0.1"):
        with gdal.quiet_errors():
            assert ogr.Open(filename) is None
    gdal.Unlink(filename)


###############################################################################
# Test reading http:// resource


@pytest.mark.require_curl()
def test_ogr_geojson_read_from_http():

    import webserver

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    response = """{"type": "FeatureCollection", "features":[
    {"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}]}"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo",
        200,
        {},
        response,
        expected_headers={"Accept": "text/plain, application/json"},
    )

    try:
        with webserver.install_http_handler(handler):
            ds = ogr.Open("http://localhost:%d/foo" % webserver_port)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
    finally:
        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
# Test reading http:// resource with GDAL_HTTP_HEADERS config option set


@pytest.mark.require_curl()
def test_ogr_geojson_read_from_http_with_GDAL_HTTP_HEADERS():

    import webserver

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    response = """{"type": "FeatureCollection", "features":[
    {"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}]}"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo",
        200,
        {},
        response,
        expected_headers={
            "foo": "bar",
            "bar": "baz",
            "Accept": "text/plain, application/json",
        },
    )

    try:
        with webserver.install_http_handler(handler):
            with gdaltest.config_option("GDAL_HTTP_HEADERS", "foo: bar\r\nbar: baz"):
                ds = ogr.Open("http://localhost:%d/foo" % webserver_port)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
    finally:
        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
# Test reading http:// resource with GDAL_HTTP_HEADERS config option set


@pytest.mark.require_curl()
def test_ogr_geojson_read_from_http_with_GDAL_HTTP_HEADERS_Accept():

    import webserver

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    response = """{"type": "FeatureCollection", "features":[
    {"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}]}"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo",
        200,
        {},
        response,
        expected_headers={"Accept": "application/json, foo/bar"},
    )

    try:
        with webserver.install_http_handler(handler):
            with gdaltest.config_option(
                "GDAL_HTTP_HEADERS", "Accept: application/json, foo/bar"
            ):
                ds = ogr.Open("http://localhost:%d/foo" % webserver_port)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
    finally:
        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
# Test reading http:// resource with GDAL_HTTP_HEADERS config option set


@pytest.mark.require_curl()
def test_ogr_geojson_read_from_http_with_GDAL_HTTP_HEADERS_overriding_Accept():

    import webserver

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    response = """{"type": "FeatureCollection", "features":[
    {"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}]}"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo",
        200,
        {},
        response,
        expected_headers={"foo": "bar", "bar": "baz", "Accept": "application/json"},
    )

    try:
        with webserver.install_http_handler(handler):
            with gdaltest.config_option(
                "GDAL_HTTP_HEADERS", "foo: bar,bar: baz,Accept: application/json"
            ):
                ds = ogr.Open("http://localhost:%d/foo" % webserver_port)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
    finally:
        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
# Test ogr2ogr -nln with a input dataset being a GeoJSON file with a name


def test_ogr_geojson_ogr2ogr_nln_with_input_dataset_having_name(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_feature_large.geojson"
    gdal.VectorTranslate(
        filename,
        '{"type":"FeatureCollection","name":"to_be_overriden","features":[]}',
        layerName="newName",
    )
    ds = ogr.Open(filename)
    assert ds.GetLayer(0).GetName() == "newName"
    ds = None


###############################################################################
# Test reading a file with a id property with a mix of features where it is set
# and others none


@pytest.mark.parametrize("read_from_file", [True, False])
def test_ogr_geojson_ids_0_1_null_unset(read_from_file):

    connection_name = "data/geojson/ids_0_1_null_unset.json"
    if not read_from_file:
        connection_name = open(connection_name, "rb").read().decode("ascii")
    ds = ogr.Open(connection_name)
    assert ds
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    assert f["id"] == 0
    assert f["seq"] == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert f["id"] == 1
    assert f["seq"] == 1
    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    assert f["id"] is None
    assert f["seq"] == 2
    f = lyr.GetNextFeature()
    assert f.GetFID() == 3
    assert not f.IsFieldSet("id")
    assert f["seq"] == 3
    f = lyr.GetNextFeature()
    assert f is None

    for i in range(4):
        f = lyr.GetFeature(i)
        assert f.GetFID() == i
        assert f["seq"] == i


###############################################################################
# Test reading a file with a id property with a mix of features where it is set
# and others none, and a conflicting id


@gdaltest.disable_exceptions()
@pytest.mark.parametrize("read_from_file", [True, False])
def test_ogr_geojson_ids_0_1_null_1_null(read_from_file):

    connection_name = "data/geojson/ids_0_1_null_1_null.json"
    if not read_from_file:
        connection_name = open(connection_name, "rb").read().decode("ascii")
    with gdal.quiet_errors():
        gdal.ErrorReset()
        ds = ogr.Open(connection_name)
        assert ds
        if not read_from_file:
            assert gdal.GetLastErrorType() == gdal.CE_Warning
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        assert f["id"] == 0
        assert f["seq"] == 0
        f = lyr.GetNextFeature()
        assert f.GetFID() == 1
        assert f["id"] == 1
        assert f["seq"] == 1
        f = lyr.GetNextFeature()
        if read_from_file:
            assert gdal.GetLastErrorType() == gdal.CE_Warning
        assert f.GetFID() == 2
        assert f["id"] is None
        assert f["seq"] == 2
        f = lyr.GetNextFeature()
        assert f.GetFID() == 3
        assert f["id"] == 1
        assert f["seq"] == 3
        f = lyr.GetNextFeature()
        assert f.GetFID() == 4
        assert f["id"] is None
        assert f["seq"] == 4
        f = lyr.GetNextFeature()
        assert f is None

    gdal.ErrorReset()
    for i in range(5):
        f = lyr.GetFeature(i)
        assert f.GetFID() == i
        assert f["seq"] == i
    assert gdal.GetLastErrorType() == gdal.CE_None


###############################################################################
# Run test_ogrsf


def test_ogr_geojson_test_ogrsf():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret, _ = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro data/geojson/ids_0_1_null_1_null.json"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf


def test_ogr_geojson_test_ogrsf_update(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    filename = str(tmp_path / "out.json")
    gdal.VectorTranslate(filename, "data/poly.shp", format="GeoJSON")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" {filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/7313


@pytest.mark.parametrize(
    "properties",
    [
        ["a_string", 42.0, {"a_field": "a_value"}],
        ["a_string", 42, {"a_field": "a_value"}],
        ["a_string", {"a_field": "a_value"}, 42],
        [42, "a_string", {"a_field": "a_value"}],
        [42, {"a_field": "a_value"}, "a_string"],
        [{"a_field": "a_value"}, 42, "a_string"],
        [{"a_field": "a_value"}, "a_string", 42],
    ],
)
def test_ogr_geojson_mixed_type_promotion(tmp_vsimem, properties):

    tmpfilename = tmp_vsimem / "temp.json"

    jdata = {"type": "FeatureCollection", "features": []}

    for prop_val in properties:
        jdata["features"].append({"type": "Feature", "properties": {"prop0": prop_val}})

    gdal.FileFromMemBuffer(
        tmpfilename,
        json.dumps(jdata),
    )

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None

    lyr = ds.GetLayer(0)
    lyr_def = lyr.GetLayerDefn()
    fld_def = lyr_def.GetFieldDefn(0)
    assert fld_def.GetTypeName() == "String"
    assert fld_def.GetSubType() == ogr.OFSTJSON


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/7319


def test_ogr_geojson_coordinate_precision(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_coordinate_precision.json"
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo", options=["COORDINATE_PRECISION=1", "WRITE_BBOX=YES"])

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1.23456789 2.3456789)"))
    lyr.CreateFeature(f)

    ds = None

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert b'"bbox":[1.2,2.3,1.2,2.3]' in data
    assert b'"coordinates":[1.2,2.3]' in data
    assert b"3456" not in data

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-1


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/7319


def test_ogr_geojson_field_types(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_geojson_field_types.json"

    test_data = """{"type":"FeatureCollection","name":"My Collection","features":[
            { "type": "Feature", "properties": { "prop0": 42 }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": "42" }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": "astring" }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "nested": 75 } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } },
            { "type": "Feature", "properties": { "prop0": { "a": "b" } }, "geometry": { "type": "Point", "coordinates": [ 102.0, 0.5 ] } }
        ]}
        """

    srcds = gdal.OpenEx(
        test_data,
        gdal.OF_VECTOR,
        open_options=["NATIVE_DATA=TRUE"],
    )

    gdal.VectorTranslate(filename, srcds, options="-f GeoJSON -lco NATIVE_DATA=TRUE")

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert b'{"prop0":"42"}' in data
    assert b'{"prop0":"astring"}' in data
    assert b'{"prop0":{"nested":75}}' in data
    assert b'{"prop0":42}' in data
    assert b'{"prop0":{"a":"b"}}' in data

    gdal.Unlink(filename)


###############################################################################
# Test opening with non C locale


def test_ogr_geojson_open_with_non_C_locale():

    import locale

    original_locale = locale.setlocale(locale.LC_ALL)
    try:
        locale.setlocale(locale.LC_ALL, "")
        if locale.localeconv()["decimal_point"] == ".":
            pytest.skip("cannot test, as decimal_point is dot")

        test_ogr_geojson_2()
    finally:
        locale.setlocale(locale.LC_ALL, original_locale)


###############################################################################
# Test geometry validity fixing due to limited coordinate precision


@pytest.mark.require_geos
def test_ogr_geojson_write_geometry_validity_fixing_rfc7946(tmp_vsimem):

    filename = str(
        tmp_vsimem / "test_ogr_geojson_write_geometry_validity_fixing.geojson"
    )
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    lyr = ds.CreateLayer("foo", srs=srs, options=["RFC7946=YES"])

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "CURVEPOLYGON (COMPOUNDCURVE ((318049.787 5688446.432,318056.628 5688447.908),CIRCULARSTRING (318056.628 5688447.908,318055.204 5688448.31,318054.25 5688449.44),(318054.25 5688449.44,318049.487 5688471.421,318049.381 5688471.91,318046.45 5688471.278,318051.318 5688448.807),CIRCULARSTRING (318051.318 5688448.807,318051.039 5688447.306,318049.787 5688446.432)))"
        )
    )
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().IsValid()
    assert "((6.3889058 51.3181847," in f.GetGeometryRef().ExportToWkt()


###############################################################################
# Test geometry validity fixing due to limited coordinate precision


@pytest.mark.require_geos
def test_ogr_geojson_write_geometry_validity_fixing(tmp_vsimem):

    filename = str(
        tmp_vsimem / "test_ogr_geojson_write_geometry_validity_fixing.geojson"
    )
    ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("foo", srs=srs, options=["COORDINATE_PRECISION=7"])

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "CURVEPOLYGON ((6.38889863954163 51.3181827925179,6.38899594962816 51.3181982385094,6.38899341741945 51.3181982023144,6.38899088736512 51.3181982771017,6.3889883718375 51.3181984625056,6.3889858831369 51.3181987576155,6.38898343343252 51.3181991609928,6.38898103470357 51.3181996706617,6.38897869867934 51.3182002841327,6.38897643678313 51.3182009984033,6.38897426007491 51.3182018099849,6.38897217919885 51.3182027149033,6.38897020433018 51.3182037087387,6.38896834512585 51.3182047866288,6.38896661067703 51.3182059433019,6.38896500946527 51.3182071731038,6.3889635493198 51.3182084700195,6.38896223738132 51.3182098277102,6.38896108006473 51.3182112395339,6.38888158690724 51.3184071643413,6.3888798178826 51.3184115229661,6.38883812537253 51.3184049086286,6.38891937389822 51.3182046159251,6.38891977479799 51.3182034182797,6.38892004565215 51.3182022064019,6.38892018522285 51.318200985823,6.38892019287242 51.3181997621242,6.38892006856587 51.3181985408984,6.38891981287105 51.3181973277249,6.38891942695666 51.3181961281515,6.38891891258675 51.3181949476556,6.38891827211159 51.3181937916355,6.38891750845846 51.3181926653737,6.38891662511696 51.3181915740195,6.38891562612488 51.3181905225562,6.3889145160472 51.3181895157905,6.38891329995705 51.3181885583257,6.38891198341239 51.318187654536,6.38891057242995 51.3181868085505,6.38890907345788 51.3181860242377,6.388907493347 51.3181853051815,6.38890583931839 51.3181846546672,6.38890411893121 51.3181840756684,6.38890234004799 51.3181835708295,6.38890051079796 51.3181831424602,6.38889863954163 51.3181827925179))"
        )
    )
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().IsValid()


###############################################################################


def test_ogr_geojson_arrow_stream_pyarrow_mixed_timezone(tmp_vsimem):
    pytest.importorskip("pyarrow")

    filename = str(
        tmp_vsimem / "test_ogr_geojson_arrow_stream_pyarrow_mixed_timezone.geojson"
    )
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789+01:00")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789-01:00")
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsPyArrow(["TIMEZONE=unknown"])
    assert stream.schema.field("datetime").type.tz is None
    values = []
    for batch in stream:
        for x in batch.field("datetime"):
            values.append(x.value)
    assert values == [None, 1654000496789, None, 1653996896789, 1654004096789]

    for tz in ["UTC", "+01:00", "-01:00", "Europe/Paris", "unknown"]:
        stream = lyr.GetArrowStreamAsPyArrow(["TIMEZONE=" + tz])
        if tz == "unknown":
            assert stream.schema.field("datetime").type.tz is None
        else:
            assert stream.schema.field("datetime").type.tz == tz
        values = []
        for batch in stream:
            for x in batch.field("datetime"):
                values.append(x.value)
        assert values == [None, 1654000496789, None, 1653996896789, 1654004096789]


###############################################################################


def test_ogr_geojson_arrow_stream_pyarrow_utc_plus_five(tmp_vsimem):
    # pytest.importorskip("pyarrow")

    filename = str(
        tmp_vsimem / "test_ogr_geojson_arrow_stream_pyarrow_utc_plus_five.geojson"
    )
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789+05:00")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T13:34:56.789+05:00")
    lyr.CreateFeature(f)
    ds = None

    try:
        import pyarrow  # NOQA

        has_pyarrow = True
    except ImportError:
        has_pyarrow = False
    if has_pyarrow:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsPyArrow()
        assert stream.schema.field("datetime").type.tz == "+05:00"
        values = []
        for batch in stream:
            for x in batch.field("datetime"):
                values.append(x.value)
        assert values == [1653982496789, 1653986096789]

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    mem_lyr = mem_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    mem_lyr.WriteArrow(lyr)

    f = mem_lyr.GetNextFeature()
    assert f["datetime"] == "2022/05/31 12:34:56.789+05"


###############################################################################


def test_ogr_geojson_arrow_stream_pyarrow_utc_minus_five(tmp_vsimem):

    filename = str(
        tmp_vsimem / "test_ogr_geojson_arrow_stream_pyarrow_utc_minus_five.geojson"
    )
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789-05:00")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T13:34:56.789-05:00")
    lyr.CreateFeature(f)
    ds = None

    try:
        import pyarrow  # NOQA

        has_pyarrow = True
    except ImportError:
        has_pyarrow = False
    if has_pyarrow:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsPyArrow()
        assert stream.schema.field("datetime").type.tz == "-05:00"
        values = []
        for batch in stream:
            for x in batch.field("datetime"):
                values.append(x.value)
        assert values == [1654018496789, 1654022096789]

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    mem_lyr = mem_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    mem_lyr.WriteArrow(lyr)

    f = mem_lyr.GetNextFeature()
    assert f["datetime"] == "2022/05/31 12:34:56.789-05"


###############################################################################


def test_ogr_geojson_arrow_stream_pyarrow_unknown_timezone(tmp_vsimem):

    filename = str(
        tmp_vsimem / "test_ogr_geojson_arrow_stream_pyarrow_unknown_timezone.geojson"
    )
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T13:34:56.789")
    lyr.CreateFeature(f)
    ds = None

    try:
        import pyarrow  # NOQA

        has_pyarrow = True
    except ImportError:
        has_pyarrow = False
    if has_pyarrow:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsPyArrow()
        assert stream.schema.field("datetime").type.tz is None
        values = []
        for batch in stream:
            for x in batch.field("datetime"):
                values.append(x.value)
        assert values == [1654000496789, 1654004096789]

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    mem_lyr = mem_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    mem_lyr.WriteArrow(lyr)

    f = mem_lyr.GetNextFeature()
    # We have lost the timezone info here, as there's no way in Arrow to
    # have a mixed of with and without timezone in a single column
    assert f["datetime"] == "2022/05/31 12:34:56.789"


###############################################################################


def test_ogr_geojson_foreign_members_collection(tmp_vsimem):

    filename = str(tmp_vsimem / "test_ogr_geojson_foreign_members_collection.geojson")
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    with pytest.raises(
        Exception,
        match="Value of FOREIGN_MEMBERS_COLLECTION should start with { and end with }",
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_COLLECTION=invalid"],
        )

    with pytest.raises(
        Exception,
        match="Value of FOREIGN_MEMBERS_COLLECTION should start with { and end with }",
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_COLLECTION={invalid"],
        )

    with pytest.raises(
        Exception, match="Value of FOREIGN_MEMBERS_COLLECTION is invalid JSON"
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_COLLECTION={invalid}"],
        )

    ds.CreateLayer(
        "test",
        geom_type=ogr.wkbNone,
        options=['FOREIGN_MEMBERS_COLLECTION={"foo":"bar"}'],
    )
    ds.Close()

    fp = gdal.VSIFOpenL(filename, "rb")
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    assert """{\n"type": "FeatureCollection",\n"foo":"bar",\n"name": "test",""" in data


###############################################################################


def test_ogr_geojson_foreign_members_feature(tmp_vsimem):

    filename = str(tmp_vsimem / "test_ogr_geojson_foreign_members_feature.geojson")
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    with pytest.raises(
        Exception,
        match="Value of FOREIGN_MEMBERS_FEATURE should start with { and end with }",
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_FEATURE=invalid"],
        )

    with pytest.raises(
        Exception,
        match="Value of FOREIGN_MEMBERS_FEATURE should start with { and end with }",
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_FEATURE={invalid"],
        )

    with pytest.raises(
        Exception, match="Value of FOREIGN_MEMBERS_FEATURE is invalid JSON"
    ):
        ds.CreateLayer(
            "test",
            geom_type=ogr.wkbNone,
            options=["FOREIGN_MEMBERS_FEATURE={invalid}"],
        )

    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbNone, options=['FOREIGN_MEMBERS_FEATURE={"foo":"bar"}']
    )
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert (
        b"""{\n"type": "FeatureCollection",\n"name": "test",\n"features": [\n{"type":"Feature","properties":{},"geometry":null,"foo":"bar"}\n]\n}"""
        in data
    )


###############################################################################
def test_ogr_json_getextent3d(tmp_vsimem):

    jdata = r"""{
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {"foo": "bar"},
                    "geometry": {
                        "type": "%s",
                        "coordinates": %s
                    }
                },
                {
                    "type": "Feature",
                    "properties": {"foo": "baz"},
                    "geometry": {
                        "type": "%s",
                        "coordinates": %s
                    }
                }
            ]
        }"""

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        jdata % ("Point", "[1, 1, 1]", "Point", "[2, 2, 2]"),
    )

    gdal.ErrorReset()
    ds = gdal.OpenEx(
        tmp_vsimem / "test.json",
        gdal.OF_VECTOR,
    )
    assert gdal.GetLastErrorMsg() == ""
    lyr = ds.GetLayer(0)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, 1.0, 2.0)

    # Test 2D
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        jdata % ("Point", "[1, 1]", "Point", "[2, 2]"),
    )

    ds = gdal.OpenEx(
        tmp_vsimem / "test.json",
        gdal.OF_VECTOR,
    )

    assert gdal.GetLastErrorMsg() == ""
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, float("inf"), float("-inf"))

    # Test capabilities and extent with filters and round trip
    lyr.SetAttributeFilter("foo = 'baz'")
    assert not lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert not lyr.TestCapability(ogr.OLCFastGetExtent)
    ext2d = lyr.GetExtent()
    assert ext2d == (2.0, 2.0, 2.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (2.0, 2.0, 2.0, 2.0, float("inf"), float("-inf"))

    lyr.SetAttributeFilter(None)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, float("inf"), float("-inf"))

    # Test capability with geometry filter
    lyr.SetSpatialFilterRect(1.5, 1.5, 2.5, 2.5)
    assert not lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert not lyr.TestCapability(ogr.OLCFastGetExtent)
    ext2d = lyr.GetExtent()
    assert ext2d == (2.0, 2.0, 2.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (2.0, 2.0, 2.0, 2.0, float("inf"), float("-inf"))

    # Test mixed 2D
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        jdata % ("Point", "[1, 1, 1]", "Point", "[2, 2]"),
    )

    ds = gdal.OpenEx(
        tmp_vsimem / "test.json",
        gdal.OF_VECTOR,
    )

    assert gdal.GetLastErrorMsg() == ""
    lyr = ds.GetLayer(0)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, 1.0, 1.0)

    # Text mixed geometry types
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        jdata % ("Point", "[1, 1, 1]", "LineString", "[[2, 2, 2], [3, 3, 3]]"),
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    # Check geometry type is unknown
    assert dfn.GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown

    # Test a polygon
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        """{
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[1, 1], [2, 1], [2, 2], [1, 2], [1, 1]]]
                    }
                }
            ]
        }""",
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, float("inf"), float("-inf"))

    # Test a polygon with a hole
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        """{
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[1, 1], [2, 1], [2, 2], [1, 2], [1, 1]], [[1.5, 1.5], [1.5, 1.6], [1.6, 1.6], [1.6, 1.5], [1.5, 1.5]]]
                    }
                }
            ]
        }""",
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)
    dfn = lyr.GetLayerDefn()
    assert dfn.GetGeomFieldCount() == 1
    ext2d = lyr.GetExtent()
    assert ext2d == (1.0, 2.0, 1.0, 2.0)
    ext3d = lyr.GetExtent3D()
    assert ext3d == (1.0, 2.0, 1.0, 2.0, float("inf"), float("-inf"))

    # Test a series of different 2D geometries including polygons with holes
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        """{
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Point",
                        "coordinates": [1, 1]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "LineString",
                        "coordinates": [[2, 2], [3, 3]]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[4, 4], [5, 4], [5, 5], [4, 5], [4, 4]]]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[6, 6], [7, 6], [7, 7], [6, 7], [6, 6]], [[6.5, 6.5], [6.5, 6.6], [6.6, 6.6], [6.6, 6.5], [6.5, 6.5]]]
                    }
                }
            ]
        }""",
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)

    assert lyr.GetExtent() == (1.0, 7.0, 1.0, 7.0)
    assert lyr.GetExtent3D() == (1.0, 7.0, 1.0, 7.0, float("inf"), float("-inf"))

    # Test a series of different 3D geometries including polygons with holes

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        """{
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Point",
                        "coordinates": [1, 1, 1]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "LineString",
                        "coordinates": [[2, 2, 2], [3, 3, 3]]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[4, 4, 4], [5, 4, 4], [5, 5, 5], [4, 5, 5], [4, 4, 4]]]
                    }
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [[[6, 6, 6], [7, 6, 6], [7, 7, 7], [6, 7, 7], [6, 6, 6]], [[6.5, 6.5, 6.5], [6.5, 6.6, 6.5], [6.6, 6.6, 6.5], [6.6, 6.5, 6.5], [6.5, 6.5, 6.5]]]
                    }
                }
            ]
        }""",
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)

    assert lyr.GetExtent() == (1.0, 7.0, 1.0, 7.0)
    assert lyr.GetExtent3D() == (1.0, 7.0, 1.0, 7.0, 1.0, 7.0)

    # Test geometrycollection
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.json",
        r"""
        {
            "type": "FeatureCollection",
            "features": [{
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "GeometryCollection",
                    "geometries": [{
                            "type": "Point",
                            "coordinates": [6, 7]
                        }, {
                            "type": "Polygon",
                            "coordinates": [[[3, 4, 2], [5, 4, 4], [5, 5, 5], [4, 5, 5], [3, 4, 2]]]
                        }]
                }
            }]
        }
        """,
    )

    ds = gdal.OpenEx(tmp_vsimem / "test.json", gdal.OF_VECTOR)

    assert gdal.GetLastErrorMsg() == ""

    lyr = ds.GetLayer(0)

    assert lyr.GetExtent() == (3.0, 6.0, 4.0, 7.0)
    assert lyr.GetExtent3D() == (3.0, 6.0, 4.0, 7.0, 2.0, 5.0)


###############################################################################
# Test geometry coordinate precision support


def test_ogr_geojson_geom_coord_precision(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojson")
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbUnknown)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-5, 1e-3, 0)
    geom_fld.SetCoordinatePrecision(prec)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1.23456789 2.34567891 9.87654321)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert b'"xy_coordinate_resolution"' in data
    assert b'"z_coordinate_resolution"' in data
    assert b'"coordinates":[1.23457,2.34568,9.877]' in data

    # Test appending feature
    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2.23456789 3.34567891 8.87654321)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert b'"coordinates":[1.23457,2.34568,9.877]' in data
    assert b'"coordinates":[2.23457,3.34568,8.877]' in data

    # Test modifying existing feature
    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    f = lyr.GetNextFeature()
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT(-2.23456789 -3.34567891 -8.87654321)")
    )
    lyr.SetFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        data = f.read()

    assert b'"coordinates":[-2.23457,-3.34568,-8.877]' in data
    assert b'"coordinates":[2.23457,3.34568,8.877]' in data

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    ds = None


###############################################################################
# Test geometry coordinate precision support


def test_ogr_geojson_geom_coord_precision_RFC7946(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojson")
    ds = gdal.GetDriverByName("GeoJSON").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbUnknown)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-3, 1e-3, 0)
    geom_fld.SetCoordinatePrecision(prec)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:32631+3855")
    geom_fld.SetSpatialRef(srs)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld, ["RFC7946=YES"])
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(8.983152841195214e-09)
    assert prec.GetZResolution() == 1e-3
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(8.983152841195214e-09)
    assert prec.GetZResolution() == 1e-3


###############################################################################
# Test opening a file that has a featureType property, but is not JSONFG.


def test_ogr_geojson_open_with_featureType_non_jsonfg():

    ds = gdal.OpenEx("data/geojson/featuretype.json")
    assert ds.GetDriver().GetDescription() == "GeoJSON"


###############################################################################
# Test force opening a JSONFG file with the GeoJSON driver


def test_ogr_geojson_open_jsonfg_with_geojson():

    ds = gdal.OpenEx("data/jsonfg/crs_none.json", allowed_drivers=["GeoJSON"])
    assert ds.GetDriver().GetDescription() == "GeoJSON"

    if gdal.GetDriverByName("JSONFG"):
        ds = gdal.OpenEx("data/jsonfg/crs_none.json")
        assert ds.GetDriver().GetDescription() == "JSONFG"

        ds = gdal.OpenEx(
            "data/jsonfg/crs_none.json", allowed_drivers=["GeoJSON", "JSONFG"]
        )
        assert ds.GetDriver().GetDescription() == "JSONFG"


###############################################################################
# Test force identifying a JSONFG file with the GeoJSON driver


def test_ogr_geojson_identify_jsonfg_with_geojson():

    drv = gdal.IdentifyDriverEx(
        "data/jsonfg/crs_none.json", allowed_drivers=["GeoJSON"]
    )
    assert drv.GetDescription() == "GeoJSON"

    if gdal.GetDriverByName("JSONFG"):
        drv = gdal.IdentifyDriverEx("data/jsonfg/crs_none.json")
        assert drv.GetDescription() == "JSONFG"

        drv = gdal.IdentifyDriverEx(
            "data/jsonfg/crs_none.json", allowed_drivers=["GeoJSON", "JSONFG"]
        )
        assert drv.GetDescription() == "JSONFG"


###############################################################################
# Test opening a file that has a "type: "Topology" feature property


def test_ogr_geojson_feature_with_type_Topology_property():

    ds = gdal.OpenEx("data/geojson/feature_with_type_Topology_property.json")
    assert ds.GetDriver().GetDescription() == "GeoJSON"


###############################################################################
# Test force opening a GeoJSON file


def test_ogr_geojson_force_opening(tmp_vsimem):

    filename = str(tmp_vsimem / "test.json")

    with gdaltest.vsi_open(filename, "wb") as f:
        f.write(
            b"{"
            + b" " * (1000 * 1000)
            + b' "type": "FeatureCollection", "features":[]}'
        )

    with pytest.raises(Exception):
        gdal.OpenEx(filename)

    ds = gdal.OpenEx(filename, allowed_drivers=["GeoJSON"])
    assert ds.GetDriver().GetDescription() == "GeoJSON"

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["GeoJSON"])
    assert drv.GetDescription() == "GeoJSON"


###############################################################################
# Test force opening a STACTA file with GeoJSON


def test_ogr_geojson_force_opening_stacta():

    if gdal.GetDriverByName("STACTA"):
        ds = gdal.OpenEx("../gdrivers/data/stacta/test.json")
        assert ds.GetDriver().GetDescription() == "STACTA"

    ds = gdal.OpenEx("../gdrivers/data/stacta/test.json", allowed_drivers=["GeoJSON"])
    assert ds.GetDriver().GetDescription() == "GeoJSON"


######################################################################
# Test schema override open option with GeoJSON driver
#
@pytest.mark.parametrize(
    "open_options, expected_field_types, expected_field_names, expected_warning",
    [
        (
            [],
            [
                ogr.OFTString,
                ogr.OFTInteger,
                ogr.OFTReal,
                ogr.OFTInteger,  # bool subType
                ogr.OFTString,  # int string
                ogr.OFTString,  # real string
                (ogr.OFTString, ogr.OFSTNone),  # json subType
                ogr.OFTString,  # uuid subType
            ],
            [],
            None,
        ),
        # Override string field with integer
        (
            [
                r'OGR_SCHEMA={"layers": [{"name": "test_point", "fields": [{ "name": "str", "type": "Integer" }]}]}'
            ],
            [
                ogr.OFTInteger,  # <-- overridden
                ogr.OFTInteger,
                ogr.OFTReal,
                ogr.OFTInteger,  # bool subType
                ogr.OFTString,  # int string
                ogr.OFTString,  # real string
                ogr.OFTString,  # json subType
                ogr.OFTString,  # uuid subType
            ],
            [],
            None,
        ),
        # Override full schema and JSON/UUID subtype
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "schemaType": "Full", "fields": [{ "name": "json_str", "subType": "JSON", "newName": "json_str" }, {"name": "uuid_str", "subType": "UUID" }]}]}'
            ],
            [
                (ogr.OFTString, ogr.OFSTJSON),  # json subType
                (ogr.OFTString, ogr.OFSTUUID),  # uuid subType
            ],
            ["json_str"],
            None,
        ),
        # Test width and precision override
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "fields": [{ "name": "real", "width": 7, "precision": 3 }]}]}'
            ],
            [
                ogr.OFTString,
                ogr.OFTInteger,
                ogr.OFTReal,
                ogr.OFTInteger,  # bool subType
                ogr.OFTString,  # int string
                ogr.OFTString,  # real string
                (ogr.OFTString, ogr.OFSTNone),  # json subType
                ogr.OFTString,  # uuid subType
            ],
            [],
            None,
        ),
        # Test boolean and short integer subtype
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "fields": [{ "name": "int", "subType": "Boolean" }, { "name": "real", "type": "Integer", "subType": "Int16" }]}]}'
            ],
            [
                ogr.OFTString,
                (ogr.OFTInteger, ogr.OFSTBoolean),  # bool overridden subType
                (ogr.OFTInteger, ogr.OFSTInt16),  # int16 overridden subType
                ogr.OFTInteger,  # bool subType
                ogr.OFTString,  # int string
                ogr.OFTString,  # real string
                ogr.OFTString,  # json subType
                ogr.OFTString,  # uuid subType
            ],
            [],
            None,
        ),
        # Test real and int str override
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "fields": [{ "name": "int_str", "type": "Integer" }, { "name": "real_str", "type": "Real" }]}]}'
            ],
            [
                ogr.OFTString,
                ogr.OFTInteger,
                ogr.OFTReal,
                ogr.OFTInteger,  # bool subType
                ogr.OFTInteger,  # int string
                ogr.OFTReal,  # real string
                ogr.OFTString,  # json subType
                ogr.OFTString,  # uuid subType
            ],
            [],
            None,
        ),
        # Test invalid schema
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "fields": [{ "name": "str", "type": "xxxxx" }]}]}'
            ],
            [],
            [],
            "Unsupported field type: xxxxx for field str",
        ),
        # Test invalid field name
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "test_point", "fields": [{ "name": "xxxxx", "type": "String", "newName": "new_str" }]}]}'
            ],
            [],
            [],
            "Field xxxxx not found",
        ),
        # Test invalid layer name
        (
            [
                r'OGR_SCHEMA={ "layers": [{"name": "xxxxx", "fields": [{ "name": "str", "type": "String" }]}]}'
            ],
            [],
            [],
            "Layer xxxxx not found",
        ),
    ],
)
def test_ogr_geojson_schema_override(
    tmp_path, open_options, expected_field_types, expected_field_names, expected_warning
):

    json_data = {
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "str": "1",
                    "int": 2,
                    "real": 3.4,
                    "bool": 1,
                    "int_str": "2",
                    "real_str": "3.4",
                    "json_str": '{"foo": 1}',
                    "uuid_str": "123e4567-e89b-12d3-a456-426614174000",
                },
                "geometry": {"type": "Point", "coordinates": [1, 2]},
            }
        ],
    }

    json_file = tmp_path / "test_point.json"
    with open(json_file, "w") as f:
        json.dump(json_data, f)

    gdal.ErrorReset()

    try:
        schema = open_options[0].split("=")[1]
        open_options = open_options[1:]
    except IndexError:
        schema = None

    with gdal.quiet_errors():

        if schema:
            open_options.append("OGR_SCHEMA=" + schema)
        else:
            open_options = []

        # Validate the JSON schema
        if not expected_warning and schema:
            schema = json.loads(schema)
            gdaltest.validate_json(schema, "ogr_fields_override.schema.json")

        # Check error if expected_field_types is empty
        if not expected_field_types:
            with gdaltest.disable_exceptions():
                ds = gdal.OpenEx(
                    tmp_path / "test_point.json",
                    gdal.OF_VECTOR | gdal.OF_READONLY,
                    open_options=open_options,
                    allowed_drivers=["GeoJSON"],
                )
                assert (
                    gdal.GetLastErrorMsg().find(expected_warning) != -1
                ), f"Warning {expected_warning} not found, got {gdal.GetLastErrorMsg()} instead"
                assert ds is None
        else:

            ds = gdal.OpenEx(
                tmp_path / "test_point.json",
                gdal.OF_VECTOR | gdal.OF_READONLY,
                open_options=open_options,
                allowed_drivers=["GeoJSON"],
            )

            assert ds is not None

            lyr = ds.GetLayer(0)

            assert lyr.GetFeatureCount() == 1

            lyr_defn = lyr.GetLayerDefn()

            assert lyr_defn.GetFieldCount() == len(expected_field_types)

            if len(expected_field_names) == 0:
                expected_field_names = [
                    "str",
                    "int",
                    "real",
                    "bool",
                    "int_str",
                    "real_str",
                    "json_str",
                    "uuid_str",
                ]

            feat = lyr.GetNextFeature()

            # Check field types
            for i in range(len(expected_field_names)):
                try:
                    expected_type, expected_subtype = expected_field_types[i]
                    assert feat.GetFieldDefnRef(i).GetType() == expected_type
                    assert feat.GetFieldDefnRef(i).GetSubType() == expected_subtype
                except TypeError:
                    expected_type = expected_field_types[i]
                    assert feat.GetFieldDefnRef(i).GetType() == expected_type
                assert feat.GetFieldDefnRef(i).GetName() == expected_field_names[i]

            # Test width and precision override
            if len(open_options) > 0 and "precision" in open_options[0]:
                assert feat.GetFieldDefnRef(2).GetWidth() == 7
                assert feat.GetFieldDefnRef(2).GetPrecision() == 3

            # Check feature content
            if len(expected_field_names) > 0:
                if "int" in expected_field_names:
                    int_sub_type = feat.GetFieldDefnRef("int").GetSubType()
                    assert (
                        feat.GetFieldAsInteger("int") == 1
                        if int_sub_type == ogr.OFSTBoolean
                        else 2
                    )
                if "str" in expected_field_names:
                    assert feat.GetFieldAsString("str") == "1"
                if "new_str" in expected_field_names:
                    assert feat.GetFieldAsString("new_str") == "1"
                if "real_str" in expected_field_names:
                    assert feat.GetFieldAsDouble("real_str") == 3.4
                if "int_str" in expected_field_names:
                    assert feat.GetFieldAsInteger("int_str") == 2
            else:
                assert feat.GetFieldAsInteger("int") == 2
                assert feat.GetFieldAsString("str") == "1"

            if expected_warning:
                assert (
                    gdal.GetLastErrorMsg().find(expected_warning) != -1
                ), f"Warning {expected_warning} not found, got {gdal.GetLastErrorMsg()} instead"


###############################################################################
# Test FOREIGN_MEMBERS open option


@pytest.mark.parametrize(
    "foreign_members_option", [None, "AUTO", "ALL", "NONE", "STAC"]
)
def test_ogr_geojson_foreign_members(foreign_members_option):

    open_options = {}
    if foreign_members_option:
        open_options["FOREIGN_MEMBERS"] = foreign_members_option
    ds = gdal.OpenEx(
        "data/geojson/stac_item.json", gdal.OF_VECTOR, open_options=open_options
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if foreign_members_option is None or foreign_members_option in ("AUTO", "STAC"):
        assert lyr.GetLayerDefn().GetFieldCount() == 39
        assert f["stac_version"] == "1.0.0"
        assert f["assets.thumbnail.title"] == "Thumbnail image"
        assert json.loads(f["assets.thumbnail.alternate"]) == {
            "s3": {
                "storage:platform": "AWS",
                "storage:requester_pays": True,
                "href": "s3://example/thumb_small.jpeg",
            }
        }
    elif foreign_members_option == "ALL":
        assert lyr.GetLayerDefn().GetFieldCount() == 35
        assert f["stac_version"] == "1.0.0"
        assert f["assets"] != ""
    else:
        assert lyr.GetLayerDefn().GetFieldCount() == 29


###############################################################################


@pytest.mark.parametrize(
    "geojson",
    [
        {"type": "Point"},
        {"type": "Point", "coordinates": None},
        {"type": "Point", "coordinates": "invalid"},
        {"type": "Point", "coordinates": []},
        {"type": "Point", "coordinates": [1]},
        {"type": "Point", "coordinates": [None, 2]},
        {"type": "Point", "coordinates": ["invalid", 2]},
        {"type": "MultiPoint"},
        {"type": "MultiPoint", "coordinates": None},
        {"type": "MultiPoint", "coordinates": "invalid"},
        {"type": "MultiPoint", "coordinates": ["invalid"]},
        {"type": "MultiPoint", "coordinates": [["invalid", 2]]},
        {"type": "LineString"},
        {"type": "LineString", "coordinates": None},
        {"type": "LineString", "coordinates": "invalid"},
        {"type": "LineString", "coordinates": ["invalid"]},
        {"type": "LineString", "coordinates": [["invalid", 2]]},
        {"type": "MultiLineString"},
        {"type": "MultiLineString", "coordinates": None},
        {"type": "MultiLineString", "coordinates": "invalid"},
        {"type": "MultiLineString", "coordinates": ["invalid"]},
        {"type": "MultiLineString", "coordinates": [["invalid"]]},
        {"type": "MultiLineString", "coordinates": [[["invalid", 2]]]},
        {"type": "Polygon"},
        {"type": "Polygon", "coordinates": None},
        {"type": "Polygon", "coordinates": "invalid"},
        {"type": "Polygon", "coordinates": ["invalid"]},
        {"type": "Polygon", "coordinates": [["invalid"]]},
        {"type": "Polygon", "coordinates": [[["invalid", 2]]]},
        {"type": "MultiPolygon"},
        {"type": "MultiPolygon", "coordinates": None},
        {"type": "MultiPolygon", "coordinates": "invalid"},
        {"type": "MultiPolygon", "coordinates": ["invalid"]},
        {"type": "MultiPolygon", "coordinates": [["invalid"]]},
        {"type": "MultiPolygon", "coordinates": [[["invalid"]]]},
        {"type": "MultiPolygon", "coordinates": [[[["invalid", 2]]]]},
    ],
)
@gdaltest.disable_exceptions()
def test_ogr_geojson_invalid_geoms(geojson):

    with gdal.quiet_errors():
        gdal.ErrorReset()
        ogr.Open(json.dumps(geojson))
        assert gdal.GetLastErrorMsg() != "", json.dumps(geojson)


@pytest.mark.require_driver("SQLite")
def test_ogr_geojson_sqlite_dialect_id_property():

    j = {
        "type": "FeatureCollection",
        "name": "test",
        "features": [
            {"type": "Feature", "properties": {"id": 5, "foo": "bar"}, "geometry": None}
        ],
    }

    with ogr.Open(json.dumps(j)) as ds:

        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "id"
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "id"

        with ds.ExecuteSQL("SELECT * FROM test", dialect="SQLite") as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 1
            f = sql_lyr.GetNextFeature()
            assert f["id"] == 5
            assert f["foo"] == "bar"

        with ds.ExecuteSQL(
            "SELECT * FROM test WHERE id = 5", dialect="SQLite"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 1
            f = sql_lyr.GetNextFeature()
            assert f["id"] == 5
            assert f["foo"] == "bar"

        with ds.ExecuteSQL(
            "SELECT * FROM test WHERE ROWID = 5", dialect="SQLite"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 1
            f = sql_lyr.GetNextFeature()
            assert f["id"] == 5
            assert f["foo"] == "bar"


def test_ogr_geojson_invalid_number(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "out.json",
        """{
        "type": "FeatureCollection",
        "features": [
            {"type": "Feature", "properties": {"foo": 12345678123456781234567812345678}, "geometry": null}
        ]
    }""",
    )

    with ogr.Open(tmp_vsimem / "out.json") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["foo"] == float(12345678123456781234567812345678)

    gdal.FileFromMemBuffer(
        tmp_vsimem / "out.json",
        """{
        "type": "FeatureCollection",
        "features": [
            {"type": "Feature", "properties": {"foo": 123456781234567-81234567812345678}, "geometry": null}
        ]
    }""",
    )
    with pytest.raises(
        Exception, match="Unrecognized number: 123456781234567-81234567812345678"
    ):
        ogr.Open(tmp_vsimem / "out.json")

    gdal.FileFromMemBuffer(
        tmp_vsimem / "out.json",
        """{
        "type": "FeatureCollection",
        "features": [
            {"type": "Feature", "properties": {"foo": 12345678.1234567-81234567812345678}, "geometry": null}
        ]
    }""",
    )
    with pytest.raises(
        Exception, match="Unrecognized number: 12345678.1234567-81234567812345678"
    ):
        ogr.Open(tmp_vsimem / "out.json")

    gdal.FileFromMemBuffer(
        tmp_vsimem / "out.json", '{"":{"":"","features":[{"":4a},0]}}'
    )

    with pytest.raises(Exception, match="Unrecognized number: 4a"):
        ogr.Open(tmp_vsimem / "out.json")


def test_ogr_geojson_export_to_json_geom_collection_with_curve():

    g = ogr.CreateGeometryFromWkt(
        "GEOMETRYCOLLECTION(GEOMETRYCOLLECTION(CIRCULARSTRING EMPTY),POINT (0 0))"
    )
    assert json.loads(g.ExportToJson()) == {
        "type": "GeometryCollection",
        "geometries": [
            {
                "type": "GeometryCollection",
                "geometries": [{"type": "LineString", "coordinates": []}],
            },
            {"type": "Point", "coordinates": [0.0, 0.0]},
        ],
    }
