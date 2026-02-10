#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ESRIJson driver test suite.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2019, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import contextlib

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("ESRIJson")

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


###############################################################################
# Test reading ESRI point file


def test_ogr_esrijson_read_point():

    ds = ogr.Open("data/esrijson/esripoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayerByName("esripoint")
    assert lyr is not None, "Missing layer called esripoint"

    extent = (2, 2, 49, 49)

    rc = validate_layer(lyr, "esripoint", 1, ogr.wkbPoint, 12, extent)
    assert rc

    layer_defn = lyr.GetLayerDefn()

    fld_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("objectid"))
    assert fld_defn.GetAlternativeName() == "Object ID"

    fld_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("fooDate"))
    assert fld_defn.GetType() == ogr.OFTDateTime
    assert fld_defn.GetWidth() == 0

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode("GEOGCS"))

    assert gcs == 4326, "Spatial reference was not valid"

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "POINT(2 49)")

    assert feature.GetFID() == 1
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("fooSmallInt"))
        .GetSubType()
        == ogr.OFSTInt16
    )
    assert feature["fooSmallInt"] == 2
    assert feature["fooInt"] == 1234567890
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("fooSingle"))
        .GetSubType()
        == ogr.OFSTFloat32
    )
    assert feature["fooSingle"] == 1.5
    assert feature["fooDouble"] == 3.4
    assert feature["fooString"] == "56"
    assert feature["fooDate"] == "2021/12/31 00:00:00+00"
    assert feature["fooDateOnly"] == "2025/09/20"
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("fooTimeOnly"))
        .GetType()
        == ogr.OFTTime
    )
    assert feature["fooTimeOnly"] == "12:34:56"
    assert feature["fooBigInteger"] == 1234567890123456
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("fooGlobalID"))
        .GetSubType()
        == ogr.OFSTUUID
    )
    assert feature["fooGlobalID"] == "{FD04C39C-69C6-4DCC-88D6-7E3E673DD0CB}"
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("fooGUID"))
        .GetSubType()
        == ogr.OFSTUUID
    )
    assert feature["fooGUID"] == "{3BFE6840-A9E6-432A-AD34-B2067C8A276F}"

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI linestring file


def test_ogr_esrijson_read_linestring():

    ds = ogr.Open("data/esrijson/esrilinestring.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbLineString, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "LINESTRING (2 49,3 50)")

    lyr = None
    ds = None

    # MultiLineString
    ds = ogr.Open("""{
  "geometryType": "esriGeometryPolyline",
  "fields": [],
  "features": [
  {
   "geometry": {
      "paths" : [
       [ [2,49],[2.1,49.1] ],
       [ [3,50],[3.1,50.1] ]
      ]
   }
  }
 ]
}""")
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feature, "MULTILINESTRING ((2 49,2.1 49.1),(3 50,3.1 50.1))"
    )


###############################################################################
# Test reading ESRI polygon file


def test_ogr_esrijson_read_polygon():

    ds = ogr.Open("data/esrijson/esripolygon.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (-3, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbPolygon, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON (((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.1 49.9,2.9 49.9,2.9 49.1,2.1 49.1)),((-2 49,-2 50,-3 50,-3 49,-2 49)))"
    )
    ogrtest.check_feature_geometry(feature, ref_geom)

    lyr = None
    ds = None

    ds = ogr.Open("data/esrijson/esripolygonempty.json")
    assert ds is not None, "Failed to open datasource"
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()
    if feature.GetGeometryRef().ExportToWkt() != "POLYGON EMPTY":
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI multipoint file


def test_ogr_esrijson_read_multipoint():

    ds = ogr.Open("data/esrijson/esrimultipoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "MULTIPOINT (2 49,3 50)")

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI point file with z value


def test_ogr_esrijson_read_pointz():

    ds = ogr.Open("data/esrijson/esrizpoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 2, 49, 49, 1, 1)

    rc = validate_layer(lyr, None, 1, ogr.wkbPoint, 4, extent)
    assert rc

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode("GEOGCS"))

    assert gcs == 4326, "Spatial reference was not valid"

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "POINT(2 49 1)")

    if feature.GetFID() != 1:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsInteger("fooInt") != 2:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsDouble("fooDouble") != 3.4:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsString("fooString") != "56":
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI linestring file with z


def test_ogr_esrijson_read_linestringz():

    ds = ogr.Open("data/esrijson/esrizlinestring.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 2)

    rc = validate_layer(lyr, None, 1, ogr.wkbLineString, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "LINESTRING (2 49 1,3 50 2)")

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI multipoint file with z


def test_ogr_esrijson_read_multipointz():

    ds = ogr.Open("data/esrijson/esrizmultipoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 2)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "MULTIPOINT (2 49 1,3 50 2)")

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI polygon file with z


def test_ogr_esrijson_read_polygonz():

    ds = ogr.Open("data/esrijson/esrizpolygon.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 4)

    rc = validate_layer(lyr, None, 1, ogr.wkbPolygon, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feature, "POLYGON ((2 49 1,2 50 2,3 50 3,3 49 4,2 49 1))"
    )

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI multipoint file with m, but no z (hasM=true, hasZ omitted)


def test_ogr_esrijson_read_multipointm():

    ds = ogr.Open("data/esrijson/esrihasmnozmultipoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "MULTIPOINT M ((2 49 1),(3 50 2))")

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI multipoint file with hasZ=true, but only 2 components.


def test_ogr_esrijson_read_pointz_withou_z():

    ds = ogr.Open("data/esrijson/esriinvalidhaszmultipoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "MULTIPOINT (2 49,3 50)")

    lyr = None
    ds = None


###############################################################################
# Test reading ESRI multipoint file with z and m


def test_ogr_esrijson_read_multipointzm():

    ds = ogr.Open("data/esrijson/esrizmmultipoint.json")
    assert ds is not None, "Failed to open datasource"

    assert ds.GetLayerCount() == 1, "Wrong number of layers"

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feature, "MULTIPOINT ZM ((2 49 1 100),(3 50 2 100))")

    lyr = None
    ds = None


###############################################################################
# Test ESRI FeatureService scrolling


@pytest.mark.parametrize("prefix", ["", "ESRIJSON:"])
def test_ogr_esrijson_featureservice_scrolling(prefix):
    @contextlib.contextmanager
    def cleanup_after_me():
        yield
        files = gdal.ReadDir("/vsimem/esrijson")
        if files:
            for f in files:
                gdal.Unlink("/vsimem/esrijson/" + f)

    with cleanup_after_me():
        with gdaltest.config_option("CPL_CURL_ENABLE_VSIMEM", "YES"):

            resultOffset0 = """
        { "type":"FeatureCollection",
        "properties" : {
            "exceededTransferLimit" : true
        },
        "features" :
        [
            {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [ 2, 49 ]
            },
            "properties": {
                "id": 1,
                "a_property": 1,
            }
            } ] }"""

            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=1", resultOffset0
            )
            ds = ogr.Open("/vsimem/esrijson/test.json?resultRecordCount=1")
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            ds = None
            gdal.Unlink("/vsimem/esrijson/test.json?resultRecordCount=1")

            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=10", resultOffset0
            )
            with gdal.quiet_errors():
                ds = ogr.Open("/vsimem/esrijson/test.json?resultRecordCount=10")
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            ds = None
            gdal.Unlink("/vsimem/esrijson/test.json?resultRecordCount=10")

            gdal.FileFromMemBuffer("/vsimem/esrijson/test.json?", resultOffset0)
            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=0",
                resultOffset0,
            )

            ds = ogr.Open("/vsimem/esrijson/test.json?")
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1

            resultOffset1 = """
        { "type":"FeatureCollection",
        "features" :
        [
            {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [ 2, 49 ]
            },
            "properties": {
                "id": 2,
                "a_property": 1,
            }
            } ] }"""
            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1",
                resultOffset1,
            )
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 2
            f = lyr.GetNextFeature()
            assert f is None

            with gdal.quiet_errors():
                fc = lyr.GetFeatureCount()
            assert fc == 2

            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?returnCountOnly=true",
                """{ "count": 123456}""",
            )
            fc = lyr.GetFeatureCount()
            assert fc == 123456

            with gdal.quiet_errors():
                extent = lyr.GetExtent()
            assert extent == (2, 2, 49, 49)

            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?returnExtentOnly=true&f=geojson",
                """{"type":"FeatureCollection","bbox":[1, 2, 3, 4],"features":[]}""",
            )
            extent = lyr.GetExtent()
            assert extent == (1.0, 3.0, 2.0, 4.0)

            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

            assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0

            assert lyr.TestCapability("foo") == 0

            # Test scrolling with ESRI json
            resultOffset0 = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "fields" : [
            {
            "name" : "objectid",
            "alias" : "Object ID",
            "type" : "esriFieldTypeOID"
            },

        ],
        "features" : [
            {
            "geometry" : {
                "x" : 2,
                "y" : 49,
                "z" : 1
            },
            "attributes" : {
                "objectid" : 1
            }
            }
        ],
        "exceededTransferLimit": true
        }
        """

            resultOffset1 = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "fields" : [
            {
            "name" : "objectid",
            "alias" : "Object ID",
            "type" : "esriFieldTypeOID"
            },

        ],
        "features" : [
            {
            "geometry": null,
            "attributes" : {
                "objectid" : 20
            }
            }
        ]
        }
        """

            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=1", resultOffset0
            )
            gdal.FileFromMemBuffer(
                "/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1",
                resultOffset1,
            )
            ds = ogr.Open(prefix + "/vsimem/esrijson/test.json?resultRecordCount=1")
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 20
            ds = None
            gdal.Unlink("/vsimem/esrijson/test.json?resultRecordCount=1")
            gdal.Unlink("/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1")


###############################################################################
# Test reading ESRIJSON files starting with {"features":[{"geometry":.... (#7198)


def test_ogr_esrijson_read_starting_with_features_geometry():

    ds = ogr.Open("data/esrijson/esrijsonstartingwithfeaturesgeometry.json")
    assert ds is not None
    assert ds.GetDriver().GetName() == "ESRIJSON"
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test ogr.CreateGeometryFromEsriJson()


def test_ogr_esrijson_create_geometry_from_esri_json():

    with pytest.raises(Exception):
        ogr.CreateGeometryFromEsriJson("error")

    g = ogr.CreateGeometryFromEsriJson('{ "x": 2, "y": 49 }')
    assert g.ExportToWkt() == "POINT (2 49)"


###############################################################################
# Test for https://github.com/OSGeo/gdal/issues/2007


def test_ogr_esrijson_identify_srs():

    data = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "spatialReference":{"wkt":"GEOGCS[\\"GCS_WGS_1984\\",DATUM[\\"D_WGS_1984\\",SPHEROID[\\"WGS_1984\\",6378137.0,298.257223563]],PRIMEM[\\"Greenwich\\",0.0],UNIT[\\"Degree\\",0.0174532925199433]]"},
        "fields" : [],
        "features" : []
        }
        """

    ds = ogr.Open(data)
    assert ds is not None
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr
    assert sr.GetAuthorityCode(None) == "4326"


###############################################################################
# Test for https://github.com/OSGeo/gdal/issues/9996


def test_ogr_esrijson_read_CadastralSpecialServices():

    ds = ogr.Open("data/esrijson/GetLatLon.json")
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr
    assert sr.GetAuthorityCode(None) == "4326"
    assert lyr.GetGeomType() != ogr.wkbNone
    f = lyr.GetNextFeature()
    assert f["landdescription"] == "WA330160N0260E0SN070"
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon


###############################################################################
# Test force opening a ESRIJSON file


def test_ogr_esrijson_force_opening(tmp_vsimem):

    filename = str(tmp_vsimem / "test.json")

    with open("data/esrijson/esripoint.json", "rb") as fsrc:
        with gdaltest.vsi_open(filename, "wb") as fdest:
            fdest.write(fsrc.read(1))
            fdest.write(b" " * (1000 * 1000))
            fdest.write(fsrc.read())

    with pytest.raises(Exception):
        gdal.OpenEx(filename)

    ds = gdal.OpenEx(filename, allowed_drivers=["ESRIJSON"])
    assert ds.GetDriver().GetDescription() == "ESRIJSON"


###############################################################################
# Test force opening a URL as ESRIJSON


def test_ogr_esrijson_force_opening_url():

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["ESRIJSON"])
    assert drv.GetDescription() == "ESRIJSON"
