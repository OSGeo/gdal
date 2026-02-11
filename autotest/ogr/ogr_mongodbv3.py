#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MongoDBv3 driver testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015-2019, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import uuid

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("MongoDBv3")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test if driver is available


def test_ogr_mongodbv3_init():

    ogrtest.mongodbv3_drv = ogr.GetDriverByName("MongoDBv3")
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    ogrtest.mongodbv3_test_uri = None
    ogrtest.mongodbv3_test_host = None
    ogrtest.mongodbv3_test_port = None
    ogrtest.mongodbv3_test_dbname = None
    ogrtest.mongodbv3_test_user = None
    ogrtest.mongodbv3_test_password = None

    if "MONGODBV3_TEST_URI" in os.environ:
        ogrtest.mongodbv3_test_uri = os.environ["MONGODBV3_TEST_URI"]
        pos = ogrtest.mongodbv3_test_uri.rfind("/")
        assert pos > 0
        ogrtest.mongodbv3_test_dbname = ogrtest.mongodbv3_test_uri[pos + 1 :]
    elif "MONGODBV3_TEST_HOST" in os.environ:
        ogrtest.mongodbv3_test_host = os.environ["MONGODBV3_TEST_HOST"]
        if "MONGODBV3_TEST_PORT" in os.environ:
            ogrtest.mongodbv3_test_port = int(os.environ["MONGODBV3_TEST_PORT"])
        else:
            ogrtest.mongodbv3_test_port = 27017
        if "MONGODBV3_TEST_DBNAME" in os.environ:
            ogrtest.mongodbv3_test_dbname = os.environ["MONGODBV3_TEST_DBNAME"]
        else:
            ogrtest.mongodbv3_test_dbname = "gdalautotest"
        if "MONGODBV3_TEST_USER" in os.environ:
            ogrtest.mongodbv3_test_user = os.environ["MONGODBV3_TEST_USER"]
        else:
            ogrtest.mongodbv3_test_user = None
        if "MONGODBV3_TEST_PASSWORD" in os.environ:
            ogrtest.mongodbv3_test_password = os.environ["MONGODBV3_TEST_PASSWORD"]
        else:
            ogrtest.mongodbv3_test_password = None
    else:
        ogrtest.mongodbv3_drv = None
        pytest.skip()

    if ogrtest.mongodbv3_test_uri is None:
        if ogrtest.mongodbv3_test_user is not None:
            ogrtest.mongodbv3_test_uri = "mongodbv3:mongodb://%s:%s@%s:%d/%s" % (
                ogrtest.mongodbv3_test_user,
                ogrtest.mongodbv3_test_password,
                ogrtest.mongodbv3_test_host,
                ogrtest.mongodbv3_test_port,
                ogrtest.mongodbv3_test_dbname,
            )
        else:
            ogrtest.mongodbv3_test_uri = "mongodbv3:mongodb://%s:%d/%s" % (
                ogrtest.mongodbv3_test_host,
                ogrtest.mongodbv3_test_port,
                ogrtest.mongodbv3_test_dbname,
            )

    ogrtest.mongodbv3_layer_name = None
    ogrtest.mongodbv3_layer_name_no_ogr_metadata = None
    ogrtest.mongodbv3_layer_name_guess_types = None
    ogrtest.mongodbv3_layer_name_with_2d_index = None
    ogrtest.mongodbv3_layer_name_no_spatial_index = None
    ogrtest.mongodbv3_layer_name_upsert_feature = None

    ds = ogr.Open(ogrtest.mongodbv3_test_uri)
    if ds is None:
        ogrtest.mongodbv3_drv = None
        pytest.skip("cannot open %s" % ogrtest.mongodbv3_test_uri)


###############################################################################
# Test various open methods


def test_ogr_mongodbv3_1():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    # Might work or not depending on how the db is set up
    with gdal.quiet_errors():
        ds = ogr.Open("mongodbv3:")

    # Wrong URI
    with gdal.quiet_errors():
        ds = ogr.Open("mongodbv3:mongodb://")
    assert ds is None

    # URI to non existent host.
    with gdal.quiet_errors():
        ds = ogr.Open("mongodbv3:mongodb://non_existing")
    assert ds is None

    # Connect to non existent host.
    with gdal.quiet_errors():
        ds = gdal.OpenEx("mongodbv3:", open_options=["HOST=non_existing"])
    assert ds is None

    if ogrtest.mongodbv3_test_host is None:
        return

    # All arguments split up
    open_options = []
    open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
    open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
    open_options += ["DBNAME=" + ogrtest.mongodbv3_test_dbname]
    if ogrtest.mongodbv3_test_user is not None:
        open_options += ["USER=" + ogrtest.mongodbv3_test_user]
        open_options += ["PASSWORD=" + ogrtest.mongodbv3_test_password]
    ds = gdal.OpenEx("mongodbv3:", open_options=open_options)
    assert ds is not None

    # Without DBNAME
    open_options = []
    open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
    open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
    if ogrtest.mongodbv3_test_user is not None:
        open_options += ["AUTH_DBNAME=" + ogrtest.mongodbv3_test_dbname]
        open_options += ["USER=" + ogrtest.mongodbv3_test_user]
        open_options += ["PASSWORD=" + ogrtest.mongodbv3_test_password]
    # Will succeed only against server in single mode
    with gdal.quiet_errors():
        ds = gdal.OpenEx("mongodbv3:", open_options=open_options)

    # A few error cases with authentication
    if ogrtest.mongodbv3_test_user is not None:
        open_options = []
        open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
        open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
        open_options += ["DBNAME=" + ogrtest.mongodbv3_test_dbname]
        # Missing user and password
        with gdal.quiet_errors():
            ds = gdal.OpenEx("mongodbv3:", open_options=open_options)
        assert ds is None

        open_options = []
        open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
        open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
        open_options += ["DBNAME=" + ogrtest.mongodbv3_test_dbname]
        open_options += ["USER=" + ogrtest.mongodbv3_test_user]
        # Missing password
        with gdal.quiet_errors():
            ds = gdal.OpenEx("mongodbv3:", open_options=open_options)
        assert ds is None

        open_options = []
        open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
        open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
        open_options += ["USER=" + ogrtest.mongodbv3_test_user]
        open_options += ["PASSWORD=" + ogrtest.mongodbv3_test_password]
        # Missing DBNAME
        with gdal.quiet_errors():
            ds = gdal.OpenEx("mongodbv3:", open_options=open_options)
        assert ds is None

        open_options = []
        open_options += ["HOST=" + ogrtest.mongodbv3_test_host]
        open_options += ["PORT=" + str(ogrtest.mongodbv3_test_port)]
        open_options += ["DBNAME=" + ogrtest.mongodbv3_test_dbname]
        open_options += ["USER=" + ogrtest.mongodbv3_test_user]
        open_options += ["PASSWORDv3=" + ogrtest.mongodbv3_test_password + "_wrong"]
        # Wrong password
        with gdal.quiet_errors():
            ds = gdal.OpenEx("mongodb:", open_options=open_options)
        assert ds is None


###############################################################################
# Basic tests


def test_ogr_mongodbv3_2():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    ogrtest.mongodbv3_ds = ogr.Open(ogrtest.mongodbv3_test_uri, update=1)
    assert ogrtest.mongodbv3_ds.GetLayerByName("not_existing") is None

    assert ogrtest.mongodbv3_ds.TestCapability(ogr.ODsCCreateLayer) == 1

    assert ogrtest.mongodbv3_ds.TestCapability(ogr.ODsCDeleteLayer) == 1

    assert (
        ogrtest.mongodbv3_ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer)
        == 1
    )

    # Create layer
    a_uuid = str(uuid.uuid1()).replace("-", "_")
    ogrtest.mongodbv3_layer_name = "test_" + a_uuid
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +ellps=WGS84 +towgs84=0,0,0")
    lyr = ogrtest.mongodbv3_ds.CreateLayer(
        ogrtest.mongodbv3_layer_name,
        geom_type=ogr.wkbPolygon,
        srs=srs,
        options=["GEOMETRY_NAME=location.mygeom", "FID="],
    )
    assert lyr.GetDataset().GetDescription() == ogrtest.mongodbv3_ds.GetDescription()

    with gdal.quiet_errors():
        ret = lyr.CreateGeomField(ogr.GeomFieldDefn("location.mygeom", ogr.wkbPoint))
    assert ret != 0

    ret = lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    assert ret == 0

    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    assert ret != 0

    lyr.CreateField(ogr.FieldDefn("location.name", ogr.OFTString))
    bool_field = ogr.FieldDefn("bool", ogr.OFTInteger)
    bool_field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(bool_field)
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("embed.str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("binary", ogr.OFTBinary))
    lyr.CreateField(ogr.FieldDefn("strlist", ogr.OFTStringList))
    lyr.CreateField(ogr.FieldDefn("intlist", ogr.OFTIntegerList))
    lyr.CreateField(ogr.FieldDefn("int64list", ogr.OFTInteger64List))
    lyr.CreateField(ogr.FieldDefn("realist", ogr.OFTRealList))
    lyr.CreateField(ogr.FieldDefn("embed.embed2.int", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("embed.embed2.real", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("str_is_null", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("str_is_unset", ogr.OFTString))

    # Test CreateFeature()
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "str"
    f["location.name"] = "Paris"
    f["bool"] = 1
    f["int"] = 1
    f["int64"] = (
        1234567890123456  # put a number larger than 1 << 40 so that fromjson() doesn't pick double
    )
    f["real"] = 1.23
    f["dt"] = "1234/12/31 23:59:59.123+00"
    f["binary"] = b"\x00\xff"
    f["strlist"] = ["a", "b"]
    f["intlist"] = [1, 2]
    f["int64list"] = [1234567890123456, 1234567890123456]
    f["realist"] = [1.23, 4.56]
    f["embed.str"] = "foo"
    f["embed.embed2.int"] = 3
    f["embed.embed2.real"] = 3.45
    f.SetFieldNull("str_is_null")
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POLYGON((2 49,2 50,3 50,3 49,2 49))")
    )
    assert lyr.CreateFeature(f) == 0
    assert f["_id"] is not None
    f_ref = f.Clone()

    # Test GetFeatureCount()
    assert lyr.GetFeatureCount() == 1

    # Test GetNextFeature()
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        f.DumpReadable()
        f_ref.DumpReadable()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef(), max_error=0)
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    # Test GetFeature()
    f = lyr.GetFeature(1)
    if not f.Equal(f_ref):
        f.DumpReadable()
        pytest.fail()

    # Test SetFeature()
    f["bool"] = 0
    assert lyr.SetFeature(f) == 0
    f_ref = f.Clone()
    f = lyr.GetFeature(1)
    if f["bool"] != 0:
        f.DumpReadable()
        pytest.fail()

    # Test (not working) DeleteFeature()
    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(1)
    assert ret != 0

    # Test Mongo filter
    lyr.SetAttributeFilter('{ "int": 1 }')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        f.DumpReadable()
        pytest.fail()

    lyr.SetAttributeFilter('{ "int": 2 }')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    # Test OGR filter
    lyr.SetAttributeFilter("int = 1")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        f.DumpReadable()
        pytest.fail()

    lyr.SetAttributeFilter("int = 2")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    # Test geometry filter
    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilterRect(2.1, 49.1, 2.9, 49.9)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        f.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(1.1, 49.1, 1.9, 49.9)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    f = f_ref.Clone()
    f.SetFID(-1)
    f.SetGeometryDirectly(None)
    assert lyr.CreateFeature(f) == 0

    # Duplicate key
    with gdal.quiet_errors():
        ret = lyr.SyncToDisk()
    assert ret != 0

    f["_id"] = None
    lyr.CreateFeature(f)
    ret = lyr.SyncToDisk()
    assert ret == 0

    # Missing _id
    f.UnsetField("_id")
    with gdal.quiet_errors():
        ret = lyr.SetFeature(f)
    assert ret != 0

    # MongoDB dialect of ExecuteSQL() with invalid JSON
    with gdal.quiet_errors():
        sql_lyr = ogrtest.mongodbv3_ds.ExecuteSQL("{", dialect="MongoDB")

    # MongoDB dialect of ExecuteSQL() with nonexistent command.
    with gdal.quiet_errors():
        sql_lyr = ogrtest.mongodbv3_ds.ExecuteSQL('{ "foo": 1 }', dialect="MongoDB")
    assert sql_lyr is None

    # MongoDB dialect of ExecuteSQL() with existing command
    sql_lyr = ogrtest.mongodbv3_ds.ExecuteSQL(
        '{ "listCommands" : 1 }', dialect="MongoDB"
    )
    assert sql_lyr is not None
    f = sql_lyr.GetNextFeature()
    assert f is not None
    f = sql_lyr.GetNextFeature()
    assert f is None
    sql_lyr.GetLayerDefn()
    sql_lyr.ResetReading()
    sql_lyr.TestCapability("")
    ogrtest.mongodbv3_ds.ReleaseResultSet(sql_lyr)

    # Regular ExecuteSQL()
    sql_lyr = ogrtest.mongodbv3_ds.ExecuteSQL(
        "SELECT * FROM " + ogrtest.mongodbv3_layer_name
    )
    assert sql_lyr is not None
    ogrtest.mongodbv3_ds.ReleaseResultSet(sql_lyr)

    # Test CreateLayer again with same name
    with gdal.quiet_errors():
        lyr = ogrtest.mongodbv3_ds.CreateLayer(ogrtest.mongodbv3_layer_name)
    assert lyr is None

    ogrtest.mongodbv3_ds = gdal.OpenEx(
        ogrtest.mongodbv3_test_uri,
        gdal.OF_UPDATE,
        open_options=[
            "FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=-1",
            "BULK_INSERT=NO",
            "JSON_FIELD=TRUE",
        ],
    )

    # Check after reopening
    lyr = ogrtest.mongodbv3_ds.GetLayerByName(ogrtest.mongodbv3_layer_name)
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0
    f = lyr.GetNextFeature()
    json_field = f["_json"]
    # We cannot use feature.Equal() has the C++ layer defn has changed
    for i in range(f_ref.GetDefnRef().GetFieldCount()):
        if (
            f.GetField(i) != f_ref.GetField(i)
            or f.GetFieldDefnRef(i).GetType() != f_ref.GetFieldDefnRef(i).GetType()
            or f.GetFieldDefnRef(i).GetSubType()
            != f_ref.GetFieldDefnRef(i).GetSubType()
        ):
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()
    for i in range(f_ref.GetDefnRef().GetGeomFieldCount()):
        if (
            not f.GetGeomFieldRef(i).Equals(f_ref.GetGeomFieldRef(i))
            or f.GetGeomFieldDefnRef(i).GetName()
            != f_ref.GetGeomFieldDefnRef(i).GetName()
            or f.GetGeomFieldDefnRef(i).GetType()
            != f_ref.GetGeomFieldDefnRef(i).GetType()
        ):
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()

    lyr.SetSpatialFilterRect(2.1, 49.1, 2.9, 49.9)
    lyr.ResetReading()
    if f is None:
        f.DumpReadable()
        pytest.fail()

    # Create a feature only from its _json content and do not store any ogr metadata related to the layer
    ogrtest.mongodbv3_layer_name_no_ogr_metadata = (
        ogrtest.mongodbv3_layer_name + "_no_ogr_metadata"
    )
    lyr = ogrtest.mongodbv3_ds.CreateLayer(
        ogrtest.mongodbv3_layer_name_no_ogr_metadata,
        options=["GEOMETRY_NAME=location.mygeom", "FID=", "WRITE_OGR_METADATA=NO"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f["_json"] = json_field
    assert lyr.CreateFeature(f) == 0

    ogrtest.mongodbv3_layer_name_guess_types = (
        ogrtest.mongodbv3_layer_name + "_guess_types"
    )
    lyr = ogrtest.mongodbv3_ds.CreateLayer(
        ogrtest.mongodbv3_layer_name_guess_types,
        geom_type=ogr.wkbNone,
        options=["FID=", "WRITE_OGR_METADATA=NO"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f["_json"] = "{"
    f["_json"] += '"int": 2, '
    f["_json"] += '"int64": { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"real": 2.34, '
    f["_json"] += '"intlist" : [2], '
    f["_json"] += '"reallist" : [2.34], '
    f["_json"] += '"int64list" : [{ "$numberLong" : "1234567890123456" }], '
    f["_json"] += '"int_str" : 2, '
    f["_json"] += '"str_int" : "2", '
    f["_json"] += '"int64_str" : { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"str_int64" : "2", '
    f["_json"] += '"int_int64": 2, '
    f["_json"] += '"int64_int": { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"int_real": 2, '
    f["_json"] += '"real_int": 3.45, '
    f["_json"] += '"int64_real": { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"real_int64": 3.45, '
    f["_json"] += '"real_str": 3.45, '
    f["_json"] += '"str_real": "3.45", '
    f["_json"] += '"int_bool" : 2, '
    f["_json"] += '"bool_int" : true, '
    f["_json"] += '"intlist_strlist" : [2], '
    f["_json"] += '"strlist_intlist" : ["2"], '
    f["_json"] += '"intlist_int64list": [2], '
    f["_json"] += '"int64list_intlist": [{ "$numberLong" : "1234567890123456" }], '
    f["_json"] += '"intlist_reallist": [2], '
    f["_json"] += '"reallist_intlist": [3.45], '
    f["_json"] += '"int64list_reallist": [{ "$numberLong" : "1234567890123456" }], '
    f["_json"] += '"reallist_int64list": [3.45], '
    f["_json"] += '"intlist_boollist" : [2], '
    f["_json"] += '"boollist_intlist" : [true], '
    f["_json"] += '"mixedlist": [true,1,{ "$numberLong" : "1234567890123456" },3.45],'
    f[
        "_json"
    ] += '"mixedlist2": [true,1,{ "$numberLong" : "1234567890123456" },3.45,"str"]'
    f["_json"] += "}"
    assert lyr.CreateFeature(f) == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f["_json"] = "{"
    f["_json"] += '"int_str" : "3", '
    f["_json"] += '"str_int" : 3, '
    f["_json"] += '"int64_str" : "2", '
    f["_json"] += '"str_int64" : { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"int_int64": { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"int64_int": 2, '
    f["_json"] += '"int_real" : 3.45, '
    f["_json"] += '"real_int": 2, '
    f["_json"] += '"int64_real": 3.45, '
    f["_json"] += '"real_int64": { "$numberLong" : "1234567890123456" }, '
    f["_json"] += '"real_str": "3.45", '
    f["_json"] += '"str_real": 3.45, '
    f["_json"] += '"int_bool" : true, '
    f["_json"] += '"bool_int" : 2, '
    f["_json"] += '"intlist_strlist" : ["3"], '
    f["_json"] += '"strlist_intlist" : [3], '
    f["_json"] += '"intlist_int64list": [{ "$numberLong" : "1234567890123456" }], '
    f["_json"] += '"int64list_intlist": [2], '
    f["_json"] += '"intlist_reallist": [3.45], '
    f["_json"] += '"reallist_intlist": [2], '
    f["_json"] += '"int64list_reallist": [3.45], '
    f["_json"] += '"reallist_int64list": [{ "$numberLong" : "1234567890123456" }], '
    f["_json"] += '"intlist_boollist" : [true], '
    f["_json"] += '"boollist_intlist" : [2]'
    f["_json"] += "}"
    assert lyr.CreateFeature(f) == 0

    # This new features will not be taken into account by below the FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=2
    f = ogr.Feature(lyr.GetLayerDefn())
    f["_json"] = "{"
    f["_json"] += '"int": { "$minKey": 1 }, '
    f["_json"] += '"int64": { "$minKey": 1 }, '
    f["_json"] += '"real": { "$minKey": 1 }, '
    f[
        "_json"
    ] += '"intlist" : [1, "1", { "$minKey": 1 },{ "$maxKey": 1 },{ "$numberLong" : "-1234567890123456" }, { "$numberLong" : "1234567890123456" }, -1234567890123456.1, 1234567890123456.1, { "$numberLong" : "1" }, 1.23 ], '
    f[
        "_json"
    ] += '"int64list" : [1, { "$numberLong" : "1234567890123456" }, "1", { "$minKey": 1 },{ "$maxKey": 1 }, -1e300, 1e300, 1.23 ], '
    f[
        "_json"
    ] += '"reallist" : [1, { "$numberLong" : "1234567890123456" }, 1.0, "1", { "$minKey": 1 },{ "$maxKey": 1 }, { "$numberLong" : "1234567890123456" } ] '
    f["_json"] += "}"
    assert lyr.CreateFeature(f) == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f["_json"] = "{"
    f["_json"] += '"int": { "$maxKey": 1 }, '
    f["_json"] += '"int64": { "$maxKey": 1 }, '
    f["_json"] += '"real": { "$maxKey": 1 } '
    f["_json"] += "}"
    assert lyr.CreateFeature(f) == 0

    ogrtest.mongodbv3_layer_name_with_2d_index = (
        ogrtest.mongodbv3_layer_name + "_with_2d_index"
    )
    with gdal.config_option("OGR_MONGODB_SPAT_INDEX_TYPE", "2d"):
        lyr = ogrtest.mongodbv3_ds.CreateLayer(
            ogrtest.mongodbv3_layer_name_with_2d_index,
            geom_type=ogr.wkbPoint,
            options=["FID=", "WRITE_OGR_METADATA=NO"],
        )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    assert lyr.CreateFeature(f) == 0

    ogrtest.mongodbv3_layer_name_no_spatial_index = (
        ogrtest.mongodbv3_layer_name + "_no_spatial_index"
    )
    for i in range(2):
        lyr = ogrtest.mongodbv3_ds.CreateLayer(
            ogrtest.mongodbv3_layer_name_no_spatial_index,
            options=["SPATIAL_INDEX=NO", "OVERWRITE=YES"],
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(2 49)"))
        assert lyr.CreateFeature(f) == 0
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "WRITE_OGR_METADATA " + ogrtest.mongodbv3_layer_name_no_spatial_index
        )

    # Open "ghost" layer
    lyr = ogrtest.mongodbv3_ds.GetLayerByName("_ogr_metadata")
    assert lyr is not None
    lyr.SetAttributeFilter("layer LIKE '%s%%'" % ogrtest.mongodbv3_layer_name)
    assert lyr.GetFeatureCount() == 2

    assert ogrtest.mongodbv3_ds.DeleteLayer(-1) != 0

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(
        ogrtest.mongodbv3_test_dbname + "." + "_ogr_metadata"
    )
    assert lyr is not None

    ogrtest.mongodbv3_ds = None

    # Reopen in read-only
    ogrtest.mongodbv3_ds = gdal.OpenEx(
        ogrtest.mongodbv3_test_uri,
        0,
        open_options=["FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=2", "JSON_FIELD=TRUE"],
    )

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(
        ogrtest.mongodbv3_layer_name_no_ogr_metadata
    )
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0
    f = lyr.GetNextFeature()
    for i in range(f_ref.GetDefnRef().GetFieldCount()):
        if f_ref.GetFieldDefnRef(i).GetNameRef() == "str_is_null":
            continue
        if f_ref.GetFieldDefnRef(i).GetNameRef() == "str_is_unset":
            continue
        # Order might be a bit different...
        j = f.GetDefnRef().GetFieldIndex(f_ref.GetFieldDefnRef(i).GetNameRef())
        if (
            f.GetField(j) != f_ref.GetField(i)
            or f.GetFieldDefnRef(j).GetType() != f_ref.GetFieldDefnRef(i).GetType()
            or f.GetFieldDefnRef(j).GetSubType()
            != f_ref.GetFieldDefnRef(i).GetSubType()
        ):
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()
    for i in range(f_ref.GetDefnRef().GetGeomFieldCount()):
        # Order might be a bit different...
        j = f.GetDefnRef().GetGeomFieldIndex(f_ref.GetGeomFieldDefnRef(i).GetNameRef())
        if (
            not f.GetGeomFieldRef(j).Equals(f_ref.GetGeomFieldRef(i))
            or f.GetGeomFieldDefnRef(j).GetName()
            != f_ref.GetGeomFieldDefnRef(i).GetName()
            or f.GetGeomFieldDefnRef(j).GetType()
            != f_ref.GetGeomFieldDefnRef(i).GetType()
        ):
            f.DumpReadable()
            f_ref.DumpReadable()
            print(f_ref.GetGeomFieldDefnRef(i).GetType())
            pytest.fail(f.GetGeomFieldDefnRef(j).GetType())

    lyr.SetSpatialFilterRect(2.1, 49.1, 2.9, 49.9)
    lyr.ResetReading()
    if f is None:
        f.DumpReadable()
        pytest.fail()

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(ogrtest.mongodbv3_layer_name_guess_types)

    expected_fields = [
        ("int", ogr.OFTInteger),
        ("int64", ogr.OFTInteger64),
        ("real", ogr.OFTReal),
        ("intlist", ogr.OFTIntegerList),
        ("reallist", ogr.OFTRealList),
        ("int64list", ogr.OFTInteger64List),
        ("int_str", ogr.OFTString),
        ("str_int", ogr.OFTString),
        ("int64_str", ogr.OFTString),
        ("str_int64", ogr.OFTString),
        ("int_int64", ogr.OFTInteger64),
        ("int64_int", ogr.OFTInteger64),
        ("int_real", ogr.OFTReal),
        ("real_int", ogr.OFTReal),
        ("int64_real", ogr.OFTReal),
        ("real_int64", ogr.OFTReal),
        ("real_str", ogr.OFTString),
        ("str_real", ogr.OFTString),
        ("int_bool", ogr.OFTInteger),
        ("bool_int", ogr.OFTInteger),
        ("intlist_strlist", ogr.OFTStringList),
        ("strlist_intlist", ogr.OFTStringList),
        ("intlist_int64list", ogr.OFTInteger64List),
        ("int64list_intlist", ogr.OFTInteger64List),
        ("intlist_reallist", ogr.OFTRealList),
        ("reallist_intlist", ogr.OFTRealList),
        ("int64list_reallist", ogr.OFTRealList),
        ("reallist_int64list", ogr.OFTRealList),
        ("intlist_boollist", ogr.OFTIntegerList),
        ("boollist_intlist", ogr.OFTIntegerList),
        ("mixedlist", ogr.OFTRealList),
        ("mixedlist2", ogr.OFTStringList),
    ]
    for fieldname, fieldtype in expected_fields:
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex(fieldname)
        )
        assert fld_defn.GetType() == fieldtype, fieldname
        assert fld_defn.GetSubType() == ogr.OFSTNone

    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if (
        f["intlist"]
        != [
            1,
            1,
            -2147483648,
            2147483647,
            -2147483648,
            2147483647,
            -2147483648,
            2147483647,
            1,
            1,
        ]
        or f["int64list"]
        != [
            1,
            1234567890123456,
            1,
            -9223372036854775808,
            9223372036854775807,
            -9223372036854775808,
            9223372036854775807,
            1,
        ]
        or f["int"] != -2147483648
        or f["int64"] != -9223372036854775808
        or f["real"] - 1 != f["real"]
    ):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if (
        f["int"] != 2147483647
        or f["int64"] != 9223372036854775807
        or f["real"] + 1 != f["real"]
    ):
        f.DumpReadable()
        pytest.fail()

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(
        ogrtest.mongodbv3_layer_name_with_2d_index
    )
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0
    lyr.SetSpatialFilterRect(1.9, 48.9, 2.1, 49.1)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    lyr.SetSpatialFilterRect(1.9, 48.9, 1.95, 48.95)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is None

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(
        ogrtest.mongodbv3_layer_name_no_spatial_index
    )
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0
    lyr.SetSpatialFilterRect(1.9, 48.9, 2.1, 49.1)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None

    with gdal.quiet_errors():
        lyr = ogrtest.mongodbv3_ds.CreateLayer("foo")
    assert lyr is None

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "WRITE_OGR_METADATA " + ogrtest.mongodbv3_layer_name
        )
    assert gdal.GetLastErrorMsg() != ""

    lyr_count_before = ogrtest.mongodbv3_ds.GetLayerCount()
    with gdal.quiet_errors():
        ogrtest.mongodbv3_ds.ExecuteSQL("DELLAYER:" + ogrtest.mongodbv3_layer_name)
    assert ogrtest.mongodbv3_ds.GetLayerCount() == lyr_count_before

    lyr = ogrtest.mongodbv3_ds.GetLayerByName(ogrtest.mongodbv3_layer_name)

    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.CreateGeomField(ogr.GeomFieldDefn("foo", ogr.wkbPoint))
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(f)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.SetFeature(f)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(1)
    assert ret != 0

    # Upsert fails in read-only
    with gdal.quiet_errors():
        ret = lyr.UpsertFeature(f)
    assert ret != 0


###############################################################################
# test_ogrsf


def test_ogr_mongodbv3_3():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro " + ogrtest.mongodbv3_test_uri
    )
    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test upserting a feature.


def test_ogr_mongodbv3_upsert_feature():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    # Open database in read-write
    ogrtest.mongodbv3_ds = None
    ogrtest.mongodbv3_ds = ogr.Open(ogrtest.mongodbv3_test_uri, update=1)

    # Create a layer
    ogrtest.mongodbv3_layer_name_upsert_feature = (
        ogrtest.mongodbv3_layer_name + "_upsert_feature"
    )
    lyr = ogrtest.mongodbv3_ds.CreateLayer(
        ogrtest.mongodbv3_layer_name_upsert_feature,
        geom_type=ogr.wkbNone,
        options=["FID=", "WRITE_OGR_METADATA=NO"],
    )

    # Add a string field
    lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    # Create a feature with some data
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["_id"] = "000000000000000000000001"  # 24-digit hex MongoDB object id
    f.SetField("test", "original")
    assert lyr.CreateFeature(f) == 0

    # Upsert an existing feature
    f = lyr.GetFeature(1)
    assert f is not None
    f.SetField("test", "updated")
    assert lyr.UpsertFeature(f) == 0

    # Verify that we have set an existing feature
    f = lyr.GetFeature(1)
    assert f is not None
    assert f.GetField("test") == "updated"

    # Upsert a new feature
    f.SetFID(2)
    f["_id"] = "000000000000000000000002"
    assert lyr.UpsertFeature(f) == 0

    # Verify that we have created a feature
    assert lyr.GetFeatureCount() == 2


###############################################################################
# Test updating a feature.


def test_ogr_mongodbv3_update_feature():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    # Open database in read-write
    ogrtest.mongodbv3_ds = None
    ogrtest.mongodbv3_ds = ogr.Open(ogrtest.mongodbv3_test_uri, update=1)

    # Create a layer
    ogrtest.mongodbv3_layer_name_update_feature = (
        ogrtest.mongodbv3_layer_name + "_update_feature"
    )
    lyr = ogrtest.mongodbv3_ds.CreateLayer(
        ogrtest.mongodbv3_layer_name_update_feature,
        geom_type=ogr.wkbNone,
        options=["FID=", "WRITE_OGR_METADATA=NO"],
    )

    # Add a string field
    lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    # Create a feature with some data
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["_id"] = "000000000000000000000001"  # 24-digit hex MongoDB object id
    f.SetField("test", "original")
    assert lyr.CreateFeature(f) == 0

    # Update an existing feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["_id"] = "000000000000000000000001"
    f.SetField("test", "updated")
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NONE
    )

    # Verify that we have set an existing feature
    f = lyr.GetFeature(1)
    assert f is not None
    assert f.GetField("test") == "updated"

    # Update an existing feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["_id"] = "000000000000000000000001"
    f.SetField("test", "updated2")
    assert (
        lyr.UpdateFeature(
            f,
            [
                lyr.GetLayerDefn().GetFieldIndex("_id"),
                lyr.GetLayerDefn().GetFieldIndex("test"),
            ],
            [],
            False,
        )
        == ogr.OGRERR_NONE
    )

    # Verify that we have set an existing feature
    f = lyr.GetFeature(1)
    assert f is not None
    assert f.GetField("test") == "updated2"

    # Update a feature without _id
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f.SetField("test", "updated")
    with gdal.quiet_errors():
        assert (
            lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
            != ogr.OGRERR_NONE
        )

    # Update a non-existing feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f["_id"] = "000000000000000000000002"
    f.SetField("test", "updated")
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NON_EXISTING_FEATURE
    )


###############################################################################
# Cleanup


def test_ogr_mongodbv3_cleanup():
    if ogrtest.mongodbv3_drv is None:
        pytest.skip()

    ogrtest.mongodbv3_ds = None

    # Reopen in read-write
    ogrtest.mongodbv3_ds = ogr.Open(ogrtest.mongodbv3_test_uri, update=1)

    if ogrtest.mongodbv3_layer_name is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL("DELLAYER:" + ogrtest.mongodbv3_layer_name)
    if ogrtest.mongodbv3_layer_name_no_ogr_metadata is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_no_ogr_metadata
        )
    if ogrtest.mongodbv3_layer_name_guess_types is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_guess_types
        )
    if ogrtest.mongodbv3_layer_name_with_2d_index is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_with_2d_index
        )
    if ogrtest.mongodbv3_layer_name_no_spatial_index is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_no_spatial_index
        )
    if ogrtest.mongodbv3_layer_name_upsert_feature is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_upsert_feature
        )
    if ogrtest.mongodbv3_layer_name_update_feature is not None:
        ogrtest.mongodbv3_ds.ExecuteSQL(
            "DELLAYER:" + ogrtest.mongodbv3_layer_name_update_feature
        )
    ogrtest.mongodbv3_ds = None
