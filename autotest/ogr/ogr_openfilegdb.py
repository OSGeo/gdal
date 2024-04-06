#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

import os
import shutil
import sys

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("OpenFileGDB")

ogrtest.openfilegdb_datalist = [
    ["none", ogr.wkbNone, None],
    ["point", ogr.wkbPoint, "POINT (1 2)"],
    ["multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)"],
    [
        "linestring",
        ogr.wkbLineString,
        "LINESTRING (1 2,3 4)",
        "MULTILINESTRING ((1 2,3 4))",
    ],
    ["multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4))"],
    [
        "multilinestring_multipart",
        ogr.wkbMultiLineString,
        "MULTILINESTRING ((1 2,3 4),(5 6,7 8))",
    ],
    [
        "polygon",
        ogr.wkbPolygon,
        "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
        "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))",
    ],
    [
        "multipolygon",
        ogr.wkbMultiPolygon,
        "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))",
    ],
    ["point25D", ogr.wkbPoint25D, "POINT (1 2 3)"],
    ["multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)"],
    [
        "linestring25D",
        ogr.wkbLineString25D,
        "LINESTRING (1 2 -10,3 4 -20)",
        "MULTILINESTRING ((1 2 -10,3 4 -20))",
    ],
    [
        "multilinestring25D",
        ogr.wkbMultiLineString25D,
        "MULTILINESTRING ((1 2 -10,3 4 -20))",
    ],
    [
        "multilinestring25D_multipart",
        ogr.wkbMultiLineString25D,
        "MULTILINESTRING ((1 2 -10,3 4 -20),(5 6 -30,7 8 -40))",
    ],
    [
        "polygon25D",
        ogr.wkbPolygon25D,
        "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))",
        "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))",
    ],
    [
        "multipolygon25D",
        ogr.wkbMultiPolygon25D,
        "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))",
    ],
    [
        "multipatch",
        ogr.wkbGeometryCollection25D,
        "GEOMETRYCOLLECTION Z (TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0))),TIN Z (((10.0 0.0 0,10.0 1.0 0,11.0 0.0 0,10.0 0.0 0)),((10.0 0.0 0,11.0 0.0 0,10.0 -1.0 0,10.0 0.0 0))),TIN Z (((5.0 0.0 0,5.0 1.0 0,6.0 0.0 0,5.0 0.0 0))),MULTIPOLYGON Z (((100.0 0.0 0,100.0 1.0 0,101.0 1.0 0,101.0 0.0 0,100.0 0.0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0))))",
    ],
    ["null_polygon", ogr.wkbPolygon, None],
    ["empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
    ["empty_multipoint", ogr.wkbMultiPoint, "MULTIPOINT EMPTY", None],
]


ogrtest.openfilegdb_datalist_m = [
    ["pointm", ogr.wkbPointM, "POINT M (1 2 3)"],
    ["pointzm", ogr.wkbPointZM, "POINT ZM (1 2 3 4)"],
    ["multipointm", ogr.wkbMultiPointM, "MULTIPOINT M ((1 2 3),(4 5 6))"],
    ["multipointzm", ogr.wkbMultiPointZM, "MULTIPOINT ZM ((1 2 3 4),(5 6 7 8))"],
    [
        "linestringm",
        ogr.wkbLineStringM,
        "LINESTRING M (1 2 3,4 5 6)",
        "MULTILINESTRING M ((1 2 3,4 5 6))",
    ],
    [
        "linestringzm",
        ogr.wkbLineStringZM,
        "LINESTRING ZM (1 2 3 4,5 6 7 8)",
        "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))",
    ],
    ["multilinestringm", ogr.wkbMultiLineStringM, "MULTILINESTRING M ((1 2 3,4 5 6))"],
    [
        "multilinestringzm",
        ogr.wkbMultiLineStringZM,
        "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))",
    ],
    [
        "polygonm",
        ogr.wkbPolygonM,
        "POLYGON M ((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1))",
        "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))",
    ],
    [
        "polygonzm",
        ogr.wkbPolygonZM,
        "POLYGON ZM ((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1))",
        "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))",
    ],
    [
        "multipolygonm",
        ogr.wkbMultiPolygonM,
        "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))",
    ],
    [
        "multipolygonzm",
        ogr.wkbMultiPolygonZM,
        "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))",
    ],
    ["empty_polygonm", ogr.wkbPolygonM, "POLYGON M EMPTY", None],
]


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    # remove FileGDB driver before running tests
    filegdb_driver = ogr.GetDriverByName("FileGDB")
    if filegdb_driver is not None:
        filegdb_driver.Deregister()

    yield

    if filegdb_driver is not None:
        print("Reregistering FileGDB driver")
        filegdb_driver.Register()


@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities

    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip("ogrsf test utility not found")

    return path


@pytest.fixture(
    params=[
        {"src": "data/filegdb/testopenfilegdb.gdb.zip", "version_10": True},
        {"src": "data/filegdb/testopenfilegdb92.gdb.zip", "version_10": False},
        {"src": "data/filegdb/testopenfilegdb93.gdb.zip", "version_10": False},
    ]
)
def gdb_source(request):
    return request.param


###############################################################################
# Make test data


def ogr_openfilegdb_make_test_data():

    try:
        shutil.rmtree("data/filegdb/testopenfilegdb.gdb")
    except OSError:
        pass
    ds = ogr.GetDriverByName("FileGDB").CreateDataSource(
        "data/filegdb/testopenfilegdb.gdb"
    )

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    options = [
        "COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML"
    ]

    for data in ogrtest.openfilegdb_datalist:
        if data[1] == ogr.wkbNone:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], options=options)
        elif data[0] == "multipatch":
            lyr = ds.CreateLayer(
                data[0],
                geom_type=data[1],
                srs=srs,
                options=["CREATE_MULTIPATCH=YES", options[0]],
            )
        else:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=options)
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("smallint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("float", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("adate", ogr.OFTDateTime))
        lyr.CreateField(ogr.FieldDefn("guid", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("xml", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("binary", ogr.OFTBinary))
        lyr.CreateField(ogr.FieldDefn("nullint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("binary2", ogr.OFTBinary))

        # We need at least 5 features so that test_ogrsf can test SetFeature()
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            if data[1] != ogr.wkbNone and data[2] is not None:
                feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
            feat.SetField("id", i + 1)
            feat.SetField("str", "foo_é")
            feat.SetField("smallint", -13)
            feat.SetField("int", 123)
            feat.SetField("float", 1.5)
            feat.SetField("real", 4.56)
            feat.SetField("adate", "2013/12/26 12:34:56")
            feat.SetField("guid", "{12345678-9abc-DEF0-1234-567890ABCDEF}")
            feat.SetField("binary", b"\x00\xFF\x7F")
            feat.SetField("xml", "<foo></foo>")
            feat.SetField("binary2", b"\x12\x34\x56")
            lyr.CreateFeature(feat)

        if data[0] == "none":
            # Create empty feature
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)

    if False:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer("sparse_layer", geom_type=ogr.wkbPoint)
        for i in range(4096):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            lyr.DeleteFeature(feat.GetFID())
        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer("big_layer", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        with gdal.config_option("FGDB_BULK_LOAD", "YES"):
            # for i in range(340*341+1):
            for i in range(340 + 1):
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField(0, i % 4)
                lyr.CreateFeature(feat)

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer("hole", geom_type=ogr.wkbPoint, srs=None)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "f1")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "fid2")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "fid3")
        lyr.CreateFeature(feat)
        feat = None

        lyr.CreateField(ogr.FieldDefn("int0", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString))

        for i in range(8):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField("str", "fid%d" % (4 + i))
            feat.SetField("int0", 4 + i)
            feat.SetField("str2", "                                            ")
            lyr.CreateFeature(feat)
        feat = None

        for i in range(8):
            lyr.CreateField(ogr.FieldDefn("int%d" % (i + 1), ogr.OFTInteger))

        lyr.DeleteFeature(1)

        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)
        feat = None

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "fid13")
        lyr.CreateFeature(feat)
        feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer("no_field", geom_type=ogr.wkbNone, srs=None)
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer("several_polygons", geom_type=ogr.wkbPolygon, srs=None)
        for i in range(3):
            for j in range(3):
                feat = ogr.Feature(lyr.GetLayerDefn())
                x1 = 2 * i
                x2 = 2 * i + 1
                y1 = 2 * j
                y2 = 2 * j + 1
                geom = ogr.CreateGeometryFromWkt(
                    "POLYGON((%d %d,%d %d,%d %d,%d %d,%d %d))"
                    % (x1, y1, x1, y2, x2, y2, x2, y1, x1, y1)
                )
                feat.SetGeometry(geom)
                lyr.CreateFeature(feat)
                feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer(
            "testnotnullable",
            geom_type=ogr.wkbPoint,
            srs=None,
            options=["GEOMETRY_NULLABLE=NO"],
        )
        field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
        field_defn.SetNullable(0)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
        lyr.CreateField(field_defn)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("field_not_nullable", "not_null")
        f.SetGeomFieldDirectly(
            "geomfield_not_nullable", ogr.CreateGeometryFromWkt("POINT(0 0)")
        )
        lyr.CreateFeature(f)
        f = None

    for data in ogrtest.openfilegdb_datalist_m:
        lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=[])

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
        lyr.CreateFeature(feat)

    for fld_name in [
        "id",
        "str",
        "smallint",
        "int",
        "float",
        "real",
        "adate",
        "guid",
        "nullint",
    ]:
        ds.ExecuteSQL("CREATE INDEX idx_%s ON point(%s)" % (fld_name, fld_name))
    ds.ExecuteSQL("CREATE INDEX idx_id ON none(id)")
    ds.ExecuteSQL("CREATE INDEX idx_real ON big_layer(real)")
    ds = None

    gdal.Unlink("data/filegdb/testopenfilegdb.gdb.zip")
    os.chdir("data/filegdb")
    os.system("zip -r -9 testopenfilegdb.gdb.zip testopenfilegdb.gdb")
    os.chdir("../..")
    shutil.rmtree("data/filegdb/testopenfilegdb.gdb")


###############################################################################
# Basic tests


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_1(gdb_source):
    filename = gdb_source["src"]
    version10 = gdb_source["version_10"]

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    assert gdal.OpenEx(filename, gdal.OF_RASTER) is None

    assert gdal.OpenEx(filename, gdal.OF_RASTER | gdal.OF_VECTOR) is not None

    ds = ogr.Open(filename)

    for data in ogrtest.openfilegdb_datalist:
        lyr_name = data[0]
        if lyr_name == "multilinestring_multipart" and not version10:
            continue
        if lyr_name == "multilinestring25D_multipart" and not version10:
            continue
        lyr = ds.GetLayerByName(lyr_name)
        assert lyr.GetDataset().GetDescription() == ds.GetDescription()
        expected_geom_type = data[1]
        if expected_geom_type == ogr.wkbLineString:
            expected_geom_type = ogr.wkbMultiLineString
        elif expected_geom_type == ogr.wkbLineString25D:
            expected_geom_type = ogr.wkbMultiLineString25D
        elif expected_geom_type == ogr.wkbPolygon:
            expected_geom_type = ogr.wkbMultiPolygon
        elif expected_geom_type == ogr.wkbPolygon25D:
            expected_geom_type = ogr.wkbMultiPolygon25D
        assert lyr.GetGeomType() == expected_geom_type, lyr.GetName()
        assert (
            expected_geom_type is ogr.wkbNone
            or lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1
        )
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("str"))
            .GetWidth()
            == 0
        )
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("smallint"))
            .GetSubType()
            == ogr.OFSTInt16
        )
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("float"))
            .GetSubType()
            == ogr.OFSTFloat32
        )
        if data[1] != ogr.wkbNone:
            assert (
                lyr.GetSpatialRef().IsSame(
                    srs, options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
                )
                == 1
            )
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except IndexError:
                expected_wkt = data[2]
            geom = feat.GetGeometryRef()
            if geom:
                geom = geom.ExportToWkt()
            if geom != expected_wkt:
                ogrtest.check_feature_geometry(feat, expected_wkt)

        if (
            feat.GetField("id") != 1
            or feat.GetField("smallint") != -13
            or feat.GetField("int") != 123
            or feat.GetField("float") != 1.5
            or feat.GetField("real") != 4.56
            or feat.GetField("adate") != "2013/12/26 12:34:56"
            or feat.GetField("guid") != "{12345678-9ABC-DEF0-1234-567890ABCDEF}"
            or (version10 and feat.GetField("xml") != "<foo></foo>")
            or feat.GetField("binary") != "00FF7F"
            or feat.GetField("binary2") != "123456"
        ):
            feat.DumpReadable()
            pytest.fail()

        if version10:
            sql_lyr = ds.ExecuteSQL("GetLayerDefinition %s" % lyr.GetName())
            assert sql_lyr is not None
            feat = sql_lyr.GetNextFeature()
            assert feat is not None
            feat = sql_lyr.GetNextFeature()
            assert feat is None
            lyr.ResetReading()
            lyr.TestCapability("foo")
            ds.ReleaseResultSet(sql_lyr)

            sql_lyr = ds.ExecuteSQL("GetLayerMetadata %s" % lyr.GetName())
            assert sql_lyr is not None
            feat = sql_lyr.GetNextFeature()
            assert feat is not None
            ds.ReleaseResultSet(sql_lyr)

    if version10:
        sql_lyr = ds.ExecuteSQL("GetLayerDefinition foo")
        assert sql_lyr is None

        sql_lyr = ds.ExecuteSQL("GetLayerMetadata foo")
        assert sql_lyr is None

    if version10:
        for data in ogrtest.openfilegdb_datalist_m:
            lyr = ds.GetLayerByName(data[0])
            expected_geom_type = data[1]
            if expected_geom_type == ogr.wkbLineStringM:
                expected_geom_type = ogr.wkbMultiLineStringM
            elif expected_geom_type == ogr.wkbLineStringZM:
                expected_geom_type = ogr.wkbMultiLineStringZM
            elif expected_geom_type == ogr.wkbPolygonM:
                expected_geom_type = ogr.wkbMultiPolygonM
            elif expected_geom_type == ogr.wkbPolygonZM:
                expected_geom_type = ogr.wkbMultiPolygonZM

            assert lyr.GetGeomType() == expected_geom_type, data
            feat = lyr.GetNextFeature()
            try:
                expected_wkt = data[3]
            except IndexError:
                expected_wkt = data[2]

            ogrtest.check_feature_geometry(feat, expected_wkt)

    ds = None


###############################################################################
# Run test_ogrsf


@pytest.fixture()
def ogrsf_run(ogrsf_path, gdb_source):
    ret = gdaltest.runexternal(ogrsf_path + " -ro " + gdb_source["src"])

    success = "INFO" in ret and "ERROR" not in ret
    assert success


def test_ogr_openfilegdb_2(ogrsf_run, gdb_source):
    pass


###############################################################################
# Open a .gdbtable directly


def test_ogr_openfilegdb_3():

    ds = ogr.Open(
        "/vsizip/data/filegdb/testopenfilegdb.gdb.zip/testopenfilegdb.gdb/a00000009.gdbtable"
    )
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "none"

    # Try opening a system table
    lyr = ds.GetLayerByName("GDB_SystemCatalog")
    assert lyr.GetName() == "GDB_SystemCatalog"
    feat = lyr.GetNextFeature()
    assert feat.GetField("Name") == "GDB_SystemCatalog"
    lyr = ds.GetLayerByName("GDB_SystemCatalog")
    assert lyr.GetName() == "GDB_SystemCatalog"

    ds = None


###############################################################################
# Test use of attribute indexes


def test_ogr_openfilegdb_4():

    ds = ogr.Open("/vsizip/data/filegdb/testopenfilegdb.gdb.zip/testopenfilegdb.gdb")

    lyr = ds.GetLayerByName("point")
    tests = [
        ("id = 1", [1]),
        ("1 = id", [1]),
        ("id = 5", [5]),
        ("id = 0", []),
        ("id = 6", []),
        ("id <= 1", [1]),
        ("1 >= id", [1]),
        ("id >= 5", [5]),
        ("5 <= id", [5]),
        ("id < 1", []),
        ("1 > id", []),
        ("id >= 1", [1, 2, 3, 4, 5]),
        ("id > 0", [1, 2, 3, 4, 5]),
        ("0 < id", [1, 2, 3, 4, 5]),
        ("id <= 5", [1, 2, 3, 4, 5]),
        ("id < 6", [1, 2, 3, 4, 5]),
        ("id <> 0", [1, 2, 3, 4, 5]),
        ("id IS NOT NULL", [1, 2, 3, 4, 5]),
        ("id IS NULL", []),
        ("nullint IS NOT NULL", []),
        ("nullint IS NULL", [1, 2, 3, 4, 5]),
        ("str = 'foo_e'", [], 1),
        ("str = 'foo_é'", [1, 2, 3, 4, 5], 1),
        ("str <= 'foo_é'", [1, 2, 3, 4, 5], 0),
        ("str >= 'foo_é'", [1, 2, 3, 4, 5], 1),
        ("str <> 'foo_é'", [], 0),
        ("str < 'foo_é'", [], 0),
        ("str > 'foo_é'", [], 0),
        ("smallint = -13", [1, 2, 3, 4, 5]),
        ("smallint <= -13", [1, 2, 3, 4, 5]),
        ("smallint >= -13", [1, 2, 3, 4, 5]),
        ("smallint < -13", []),
        ("smallint > -13", []),
        ("int = 123", [1, 2, 3, 4, 5]),
        ("int <= 123", [1, 2, 3, 4, 5]),
        ("int >= 123", [1, 2, 3, 4, 5]),
        ("int < 123", []),
        ("int > 123", []),
        ("float = 1.5", [1, 2, 3, 4, 5]),
        ("float <= 1.5", [1, 2, 3, 4, 5]),
        ("float >= 1.5", [1, 2, 3, 4, 5]),
        ("float < 1.5", []),
        ("float > 1.5", []),
        ("real = 4.56", [1, 2, 3, 4, 5]),
        ("real <= 4.56", [1, 2, 3, 4, 5]),
        ("real >= 4.56", [1, 2, 3, 4, 5]),
        ("real < 4.56", []),
        ("real > 4.56", []),
        ("adate = '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
        ("adate <= '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
        ("adate >= '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
        ("adate < '2013/12/26 12:34:56'", []),
        ("adate > '2013/12/26 12:34:56'", []),
        ("guid = '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
        ("guid <= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
        ("guid >= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
        ("guid < '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
        ("guid > '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
        ("guid = '{'", []),
        ("guid > '{'", [1, 2, 3, 4, 5]),
        ("NOT(id = 1)", [2, 3, 4, 5]),
        ("id = 1 OR id = -1", [1]),
        ("id = -1 OR id = 1", [1]),
        ("id = 1 OR id = 1", [1]),
        ("id = 1 OR id = 2", [1, 2]),  # exclusive branches
        ("id < 3 OR id > 3", [1, 2, 4, 5]),  # exclusive branches
        ("id > 3 OR id < 3", [1, 2, 4, 5]),  # exclusive branches
        ("id <= 3 OR id >= 4", [1, 2, 3, 4, 5]),  # exclusive branches
        ("id >= 4 OR id <= 3", [1, 2, 3, 4, 5]),  # exclusive branches
        ("id < 3 OR id >= 3", [1, 2, 3, 4, 5]),
        ("id <= 3 OR id >= 3", [1, 2, 3, 4, 5]),
        ("id <= 5 OR id >= 1", [1, 2, 3, 4, 5]),
        ("id <= 1.5 OR id >= 2", [1, 2, 3, 4, 5]),
        ("id IS NULL OR id IS NOT NULL", [1, 2, 3, 4, 5]),
        ("float < 1.5 OR float > 1.5", []),
        ("float <= 1.5 OR float >= 1.5", [1, 2, 3, 4, 5]),
        ("float < 1.5 OR float > 2", []),
        ("float < 1 OR float > 2.5", []),
        ("str < 'foo_é' OR str > 'z'", [], 0),
        ("adate < '2013/12/26 12:34:56' OR adate > '2014/01/01'", []),
        ("id = 1 AND id = -1", []),
        ("id = -1 AND id = 1", []),
        ("id = 1 AND id = 1", [1]),
        ("id = 1 AND id = 2", []),
        ("id <= 5 AND id >= 1", [1, 2, 3, 4, 5]),
        ("id <= 3 AND id >= 3", [3]),
        ("id = 1 AND float = 1.5", [1]),
        ("id BETWEEN 1 AND 5", [1, 2, 3, 4, 5]),
        ("id IN (1)", [1]),
        ("id IN (5,4,3,2,1)", [1, 2, 3, 4, 5]),
        ("fid = 1", [1], 0),  # no index used
        ("fid BETWEEN 1 AND 1", [1], 0),  # no index used
        ("fid IN (1)", [1], 0),  # no index used
        ("fid IS NULL", [], 0),  # no index used
        ("fid IS NOT NULL", [1, 2, 3, 4, 5], 0),  # no index used
        ("xml <> ''", [1, 2, 3, 4, 5], 0),  # no index used
        ("id = 1 AND xml <> ''", [1], 1),  # index partially used
        ("xml <> '' AND id = 1", [1], 1),  # index partially used
        ("NOT(id = 1 AND xml <> '')", [2, 3, 4, 5], 0),  # no index used
        ("id = 1 OR xml <> ''", [1, 2, 3, 4, 5], 0),  # no index used
        ("id = id", [1, 2, 3, 4, 5], 0),  # no index used
        ("id = 1 + 0", [1], 0),  # no index used (currently...)
    ]
    for test in tests:

        if len(test) == 2:
            (where_clause, fids) = test
            expected_attr_index_use = 2
        else:
            (where_clause, fids, expected_attr_index_use) = test

        lyr.SetAttributeFilter(where_clause)
        sql_lyr = ds.ExecuteSQL("GetLayerAttrIndexUse %s" % lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert attr_index_use == expected_attr_index_use, (
            where_clause,
            fids,
            expected_attr_index_use,
        )
        assert lyr.GetFeatureCount() == len(fids), (where_clause, fids)
        for fid in fids:
            feat = lyr.GetNextFeature()
            assert feat.GetFID() == fid, (where_clause, fids)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, fids)

    lyr = ds.GetLayerByName("none")
    tests = [
        ("id = 1", [1]),
        ("id IS NULL", [6]),
        ("id IS NOT NULL", [1, 2, 3, 4, 5]),
        ("id IS NULL OR id IS NOT NULL", [1, 2, 3, 4, 5, 6]),
        ("id = 1 OR id IS NULL", [1, 6]),
        ("id IS NULL OR id = 1", [1, 6]),
    ]
    for test in tests:

        if len(test) == 2:
            (where_clause, fids) = test
            expected_attr_index_use = 2
        else:
            (where_clause, fids, expected_attr_index_use) = test

        lyr.SetAttributeFilter(where_clause)
        sql_lyr = ds.ExecuteSQL("GetLayerAttrIndexUse %s" % lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert attr_index_use == expected_attr_index_use, (
            where_clause,
            fids,
            expected_attr_index_use,
        )
        assert lyr.GetFeatureCount() == len(fids), (where_clause, fids)
        for fid in fids:
            feat = lyr.GetNextFeature()
            assert feat.GetFID() == fid, (where_clause, fids)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, fids)

    lyr = ds.GetLayerByName("big_layer")
    tests = [
        ("real = 0", 86, 1),
        ("real = 1", 85, 2),
        ("real = 2", 85, 3),
        ("real = 3", 85, 4),
        ("real >= 0", 86 + 3 * 85, None),
        ("real < 4", 86 + 3 * 85, None),
        ("real > 1 AND real < 2", 0, None),
        ("real < 0", 0, None),
    ]
    for (where_clause, count, start) in tests:

        lyr.SetAttributeFilter(where_clause)
        assert lyr.GetFeatureCount() == count, (where_clause, count)
        for i in range(count):
            feat = lyr.GetNextFeature()
            assert not (
                feat is None or (start is not None and feat.GetFID() != i * 4 + start)
            ), (where_clause, count)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, count)

    ds = None


###############################################################################
# Test use of attribute indexes on truncated strings


def test_ogr_openfilegdb_str_indexed_truncated():

    ds = ogr.Open("data/filegdb/test_str_indexed_truncated.gdb")

    lyr = ds.GetLayerByName("test")

    IDX_NOT_USED = 0
    IDX_USED = 1

    tests = [
        ("str = 'a'", [1], IDX_USED),
        ("str = 'aa'", [2], IDX_USED),
        ("str != 'aa'", [1, 3], IDX_NOT_USED),
        ("str = 'aaa'", [3], IDX_USED),
        ("str >= 'aaa'", [3], IDX_USED),
        ("str > 'aaa'", [], IDX_NOT_USED),
        ("str > 'aa_'", [3], IDX_NOT_USED),
        ("str <= 'aab'", [1, 2, 3], IDX_NOT_USED),
        ("str = 'aaa '", [], IDX_USED),
        ("str != 'aaa '", [1, 2, 3], IDX_NOT_USED),
        ("str <= 'aaa '", [1, 2, 3], IDX_NOT_USED),
        ("str <= 'aaaX'", [1, 2, 3], IDX_NOT_USED),
        ("str >= 'aaa '", [], IDX_USED),
        ("str = 'aaaX'", [], IDX_USED),
        ("str = 'aaaXX'", [], IDX_USED),
        ("str = 'aaa  '", [], IDX_USED),
        ("str IN ('a', 'b')", [1], IDX_USED),
        ("str IN ('aaa')", [3], IDX_USED),
        ("str IN ('aaa', 'aaa ')", [3], IDX_USED),
        ("str IN ('aaa ')", [], IDX_USED),
        ("str IN ('aaaX')", [], IDX_USED),
        ("str IN ('aaaXX')", [], IDX_USED),
    ]
    for where_clause, fids, expected_attr_index_use in tests:

        lyr.SetAttributeFilter(where_clause)
        sql_lyr = ds.ExecuteSQL("GetLayerAttrIndexUse %s" % lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert attr_index_use == expected_attr_index_use, (
            where_clause,
            fids,
            expected_attr_index_use,
        )
        assert [f.GetFID() for f in lyr] == fids, (where_clause, fids)


###############################################################################
# Test opening an unzipped dataset


@pytest.fixture()
def testopenfilegdb(tmp_path):

    try:
        gdaltest.unzip(tmp_path, "data/filegdb/testopenfilegdb.gdb.zip")
    except OSError:
        pytest.skip()

    try:
        os.stat(tmp_path / "testopenfilegdb.gdb")
    except OSError:
        pytest.skip()

    return tmp_path / "testopenfilegdb.gdb"


def test_ogr_openfilegdb_5(testopenfilegdb):

    ds = ogr.Open(testopenfilegdb)
    assert ds is not None


###############################################################################
# Test special SQL processing for min/max/count/sum/avg values


def test_ogr_openfilegdb_6():

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")

    # With indices
    sql_lyr = ds.ExecuteSQL(
        "select min(id), max(id), count(id), sum(id), avg(id), min(str), min(smallint), "
        "avg(smallint), min(float), avg(float), min(real), avg(real), min(adate), avg(adate), min(guid), min(nullint), avg(nullint) from point"
    )
    assert sql_lyr is not None
    feat = sql_lyr.GetNextFeature()
    if (
        feat.GetField("MIN_id") != 1
        or feat.GetField("MAX_id") != 5
        or feat.GetField("COUNT_id") != 5
        or feat.GetField("SUM_id") != 15.0
        or feat.GetField("AVG_id") != 3.0
        or feat.GetField("MIN_str")[0:4] != "foo_"
        or feat.GetField("MIN_smallint") != -13
        or feat.GetField("AVG_smallint") != -13
        or feat.GetField("MIN_float") != 1.5
        or feat.GetField("AVG_float") != 1.5
        or feat.GetField("MIN_real") != 4.56
        or feat.GetField("AVG_real") != 4.56
        or feat.GetField("MIN_adate") != "2013/12/26 12:34:56"
        or feat.GetField("AVG_adate") != "2013/12/26 12:34:56"
        or feat.GetField("MIN_guid") != "{12345678-9ABC-DEF0-1234-567890ABCDEF}"
        or feat.IsFieldSet("MIN_nullint")
        or feat.IsFieldSet("AVG_nullint")
    ):
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # No index
    sql_lyr = ds.ExecuteSQL("select min(id),  avg(id) from multipoint")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField("MIN_id") != 1 or feat.GetField("AVG_id") != 3.0:
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test special SQL processing for ORDER BY


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_7():

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")

    tests = [  # Optimized:
        ("select * from point order by id", 5, 1, 1),
        ("select id, str from point order by id desc", 5, 5, 1),
        ("select * from point where id = 1 order by id", 1, 1, 1),
        ("select * from big_layer order by real", 86 + 3 * 85, 1, 1),
        ("select * from big_layer order by real limit 0", 0, None, 1),
        ("select * from big_layer order by real offset 10000", 0, None, 1),
        ("select * from big_layer order by real limit 1", 1, 1, 1),
        ("select * from big_layer order by real limit 1 offset 0", 1, 1, 1),
        ("select * from big_layer order by real limit 1 offset 1", 1, 5, 1),
        ("select * from big_layer order by real limit 2", 2, 1, 1),
        ("select * from big_layer order by real limit 100000", 86 + 3 * 85, 1, 1),
        (
            "select * from big_layer order by real limit 100000 offset 1",
            86 + 3 * 85 - 1,
            5,
            1,
        ),
        ("select * from big_layer order by real desc", 86 + 3 * 85, 4 * 85, 1),
        # Invalid :
        ("select foo from", None, None, None),
        ("select foo from bar", None, None, None),
        ("select * from point order by foo", None, None, None),
        # Non-optimized :
        ("select * from point order by xml", None, None, 0),
        ("select fid from point order by id", None, None, 0),
        ("select cast(id as float) from point order by id", None, None, 0),
        ("select distinct id from point order by id", None, None, 0),
        ("select 1 from point order by id", None, None, 0),
        ("select count(*) from point order by id", None, None, 0),
        ("select * from point order by nullint", None, None, 0),
        ("select * from point where id = 1 or id = 2 order by id", None, None, 0),
        ("select * from point where id = 1 order by id, float", None, None, 0),
        ("select * from point where float > 0 order by id", None, None, 0),
    ]

    for (sql, feat_count, first_fid, expected_optimized) in tests:
        if expected_optimized is None:
            gdal.PushErrorHandler("CPLQuietErrorHandler")
        sql_lyr = ds.ExecuteSQL(sql)
        if expected_optimized is None:
            gdal.PopErrorHandler()
        if expected_optimized is None:
            if sql_lyr is not None:
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail(sql, feat_count, first_fid)
            continue
        assert sql_lyr is not None, (sql, feat_count, first_fid)
        if expected_optimized:
            if sql_lyr.GetFeatureCount() != feat_count:
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail(sql, feat_count, first_fid)
            feat = sql_lyr.GetNextFeature()
            if feat_count > 0:
                if feat.GetFID() != first_fid:
                    ds.ReleaseResultSet(sql_lyr)
                    feat.DumpReadable()
                    pytest.fail(sql, feat_count, first_fid)
            else:
                assert first_fid is None
                assert feat is None
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("GetLastSQLUsedOptimizedImplementation")
        optimized = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert optimized == expected_optimized, (sql, feat_count, first_fid)

        if optimized and "big_layer" not in sql:
            import test_cli_utilities

            if test_cli_utilities.get_test_ogrsf_path() is not None:
                ret = gdaltest.runexternal(
                    test_cli_utilities.get_test_ogrsf_path()
                    + ' -ro data/filegdb/testopenfilegdb.gdb.zip -sql "%s"' % sql
                )
                assert ret.find("INFO") != -1 and ret.find("ERROR") == -1, (
                    sql,
                    feat_count,
                    first_fid,
                )


###############################################################################
# Test reading a .gdbtable without .gdbtablx


def test_ogr_openfilegdb_8():

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    dict_feat_count = {}
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        dict_feat_count[lyr.GetName()] = lyr.GetFeatureCount()
    ds = None

    dict_feat_count2 = {}
    with gdal.config_option("OPENFILEGDB_IGNORE_GDBTABLX", "YES"):
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        for i in range(ds.GetLayerCount()):
            lyr = ds.GetLayer(i)
            dict_feat_count2[lyr.GetName()] = lyr.GetFeatureCount()

    assert dict_feat_count == dict_feat_count2

    lyr = ds.GetLayerByName("hole")
    # Not exactly in the order that one might expect, but logical when
    # looking at the structure of the .gdbtable
    expected_str = [
        "fid13",
        "fid2",
        "fid3",
        "fid4",
        "fid5",
        "fid6",
        "fid7",
        "fid8",
        "fid9",
        "fid10",
        "fid11",
        None,
    ]
    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if feat.GetField("str") != expected_str[i]:
            feat.DumpReadable()
            pytest.fail()
        i = i + 1
        feat = lyr.GetNextFeature()


###############################################################################
# Test reading a .gdbtable outside a .gdb


def test_ogr_openfilegdb_9(tmp_path, testopenfilegdb):

    shutil.copy(testopenfilegdb / "a00000009.gdbtable", tmp_path / "a00000009.gdbtable")
    shutil.copy(testopenfilegdb / "a00000009.gdbtablx", tmp_path / "a00000009.gdbtablx")
    ds = ogr.Open(tmp_path / "a00000009.gdbtable")
    assert ds is not None
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None


###############################################################################
# Test various error conditions


def fuzz(filename, offset):
    with open(filename, "rb+") as f:
        f.seek(offset, 0)
        v = ord(f.read(1))
        f.seek(offset, 0)
        f.write(chr(255 - v).encode("ISO-8859-1"))
    return (filename, offset, v)


def unfuzz(backup):
    (filename, offset, v) = backup
    with open(filename, "rb+") as f:
        f.seek(offset, 0)
        f.write(chr(v).encode("ISO-8859-1"))


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_10(testopenfilegdb, tmp_path):

    shutil.copytree(testopenfilegdb, tmp_path / "testopenfilegdb_fuzzed.gdb")

    if False:  # pylint: disable=using-constant-test
        for filename in [
            tmp_path / "testopenfilegdb_fuzzed.gdb/a00000001.gdbtable",
            tmp_path / "testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx",
        ]:
            errors = set()
            offsets = []
            last_error_msg = ""
            last_offset = -1
            for offset in range(os.stat(filename).st_size):
                # print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                # print(offset)
                ds = ogr.Open(tmp_path / "testopenfilegdb_fuzzed.gdb")
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName("GDB_SystemCatalog")
                    if error_msg == "":
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == "":
                            error_msg = gdal.GetLastErrorMsg()
                if feat is None or error_msg != "":
                    if offset - last_offset >= 4 or last_error_msg != error_msg:
                        if error_msg != "" and error_msg not in errors:
                            errors.add(error_msg)
                            offsets.append(offset)
                        else:
                            offsets.append(offset)
                    last_offset = offset
                    last_error_msg = error_msg
                ds = None
                unfuzz(backup)
            print(offsets)

        for filename in [
            tmp_path / "testopenfilegdb_fuzzed.gdb/a00000004.gdbindexes",
            tmp_path
            / "testopenfilegdb_fuzzed.gdb/a00000004.CatItemsByPhysicalName.atx",
        ]:
            errors = set()
            offsets = []
            last_error_msg = ""
            last_offset = -1
            for offset in range(os.stat(filename).st_size):
                # print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                # print(offset)
                ds = ogr.Open(tmp_path / "testopenfilegdb_fuzzed.gdb")
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName("GDB_Items")
                    lyr.SetAttributeFilter("PhysicalName = 'NO_FIELD'")
                    if error_msg == "":
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == "":
                            error_msg = gdal.GetLastErrorMsg()
                if feat is None or error_msg != "":
                    if offset - last_offset >= 4 or last_error_msg != error_msg:
                        if error_msg != "" and error_msg not in errors:
                            errors.add(error_msg)
                            offsets.append(offset)
                        else:
                            offsets.append(offset)
                    last_offset = offset
                    last_error_msg = error_msg
                ds = None
                unfuzz(backup)
            print(offsets)

    else:

        for (filename, offsets) in [
            (
                tmp_path / "testopenfilegdb_fuzzed.gdb/a00000001.gdbtable",
                [
                    4,
                    5,
                    6,
                    7,
                    32,
                    33,
                    41,
                    42,
                    52,
                    59,
                    60,
                    63,
                    64,
                    72,
                    73,
                    77,
                    78,
                    79,
                    80,
                    81,
                    101,
                    102,
                    104,
                    105,
                    111,
                    180,
                ],
            ),
            (
                tmp_path / "testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx",
                [4, 7, 11, 12, 16, 31, 5136, 5140, 5142, 5144],
            ),
        ]:
            for offset in offsets:
                backup = fuzz(filename, offset)
                with gdal.quiet_errors():
                    gdal.ErrorReset()
                    ds = ogr.Open(tmp_path / "testopenfilegdb_fuzzed.gdb")
                    error_msg = gdal.GetLastErrorMsg()
                    feat = None
                    if ds is not None:
                        gdal.ErrorReset()
                        lyr = ds.GetLayerByName("GDB_SystemCatalog")
                        if error_msg == "":
                            error_msg = gdal.GetLastErrorMsg()
                        if lyr is not None:
                            gdal.ErrorReset()
                            feat = lyr.GetNextFeature()
                            if error_msg == "":
                                error_msg = gdal.GetLastErrorMsg()
                    if feat is not None and error_msg == "":
                        print(
                            "%s: expected problem at offset %d, but did not find"
                            % (filename, offset)
                        )
                    ds = None
                unfuzz(backup)

        for (filename, offsets) in [
            (
                tmp_path / "testopenfilegdb_fuzzed.gdb/a00000004.gdbindexes",
                [
                    0,
                    4,
                    5,
                    44,
                    45,
                    66,
                    67,
                    100,
                    101,
                    116,
                    117,
                    148,
                    149,
                    162,
                    163,
                    206,
                    207,
                    220,
                    221,
                    224,
                    280,
                    281,
                ],
            ),
            (
                tmp_path
                / "testopenfilegdb_fuzzed.gdb/a00000004.CatItemsByPhysicalName.atx",
                [4, 12, 8196, 8300, 8460, 8620, 8780, 8940, 9100, 12290, 12294, 12298],
            ),
        ]:
            for offset in offsets:
                # print(offset)
                backup = fuzz(filename, offset)
                with gdal.quiet_errors():
                    gdal.ErrorReset()
                    ds = ogr.Open(tmp_path / "testopenfilegdb_fuzzed.gdb")
                    error_msg = gdal.GetLastErrorMsg()
                    feat = None
                    if ds is not None:
                        gdal.ErrorReset()
                        lyr = ds.GetLayerByName("GDB_Items")
                        lyr.SetAttributeFilter("PhysicalName = 'NO_FIELD'")
                        if error_msg == "":
                            error_msg = gdal.GetLastErrorMsg()
                        if lyr is not None:
                            gdal.ErrorReset()
                            feat = lyr.GetNextFeature()
                            if error_msg == "":
                                error_msg = gdal.GetLastErrorMsg()
                    if feat is not None and error_msg == "":
                        print(
                            "%s: expected problem at offset %d, but did not find"
                            % (filename, offset)
                        )
                    ds = None
                unfuzz(backup)


###############################################################################
# Test spatial filtering


SPI_IN_BUILDING = 0
SPI_COMPLETED = 1
SPI_INVALID = 2


def get_spi_state(ds, lyr):
    sql_lyr = ds.ExecuteSQL("GetLayerSpatialIndexState %s" % lyr.GetName())
    value = int(sql_lyr.GetNextFeature().GetField(0))
    ds.ReleaseResultSet(sql_lyr)
    return value


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_in_memory_spatial_filter():

    with gdaltest.config_option("OPENFILEGDB_USE_SPATIAL_INDEX", "NO"):

        # Test building spatial index with GetFeatureCount()
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("several_polygons")
        assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
        lyr.ResetReading()
        assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
        lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
        assert lyr.GetFeatureCount() == 1
        assert get_spi_state(ds, lyr) == SPI_COMPLETED
        # Should return cached value
        assert lyr.GetFeatureCount() == 1
        # Should use index
        c = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            c = c + 1
            feat = lyr.GetNextFeature()
        assert c == 1
        feat = None
        lyr = None
        ds = None

        # Test iterating without spatial index already built
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("several_polygons")
        lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
        c = 0
        feat = lyr.GetNextFeature()
        assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
        while feat is not None:
            c = c + 1
            feat = lyr.GetNextFeature()
        assert c == 1
        assert get_spi_state(ds, lyr) == SPI_COMPLETED
        feat = None
        lyr = None
        ds = None

        # Test GetFeatureCount() without spatial index already built, with no matching feature
        # when GEOS is available
        if ogrtest.have_geos():
            expected_count = 0
        else:
            expected_count = 5

        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("multipolygon")
        lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
        assert lyr.GetFeatureCount() == expected_count
        lyr = None
        ds = None

        # Test iterating without spatial index already built, with no matching feature
        # when GEOS is available
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("multipolygon")
        lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
        c = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            c = c + 1
            feat = lyr.GetNextFeature()
        assert c == expected_count
        assert lyr.GetFeatureCount() == expected_count
        feat = None
        lyr = None
        ds = None

        # GetFeature() should not impact spatial index building
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("several_polygons")
        lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
        feat = lyr.GetFeature(1)
        feat = lyr.GetFeature(1)
        assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
        feat = lyr.GetNextFeature()
        while feat is not None:
            feat = lyr.GetNextFeature()
        assert get_spi_state(ds, lyr) == SPI_COMPLETED
        lyr.ResetReading()
        c = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            c = c + 1
            feat = lyr.GetNextFeature()
        assert c == 1
        assert get_spi_state(ds, lyr) == SPI_COMPLETED

        # This will create an array of filtered features
        lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
        assert lyr.TestCapability(ogr.OLCFastSetNextByIndex) == 1
        # Test SetNextByIndex() with filtered features
        assert lyr.SetNextByIndex(-1) != 0
        assert lyr.SetNextByIndex(1) != 0
        assert lyr.SetNextByIndex(0) == 0
        feat = lyr.GetNextFeature()
        assert feat.GetFID() == 1
        assert get_spi_state(ds, lyr) == SPI_COMPLETED

        feat = None
        lyr = None
        ds = None

        # SetNextByIndex() impacts spatial index building
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("multipolygon")
        lyr.SetNextByIndex(3)
        assert get_spi_state(ds, lyr) == SPI_INVALID
        feat = None
        lyr = None
        ds = None

        # and ResetReading() as well
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("multipolygon")
        feat = lyr.GetNextFeature()
        lyr.ResetReading()
        assert get_spi_state(ds, lyr) == SPI_INVALID
        feat = None
        lyr = None
        ds = None

        # and SetAttributeFilter() with an index too
        ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
        lyr = ds.GetLayerByName("point")
        lyr.SetAttributeFilter("id = 1")
        assert get_spi_state(ds, lyr) == SPI_INVALID
        feat = None
        lyr = None
        ds = None


def test_ogr_openfilegdb_spx_spatial_filter():

    # Test GetFeatureCount() and then iterating
    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    lyr = ds.GetLayerByName("several_polygons")
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    c = 0
    for f in lyr:
        c += 1
    assert c == 1

    # Set another spatial filter
    lyr.SetSpatialFilterRect(0, 2, 1, 5)
    assert lyr.GetFeatureCount() == 2

    # Unset spatial filter
    lyr.SetSpatialFilter(None)
    assert lyr.GetFeatureCount() == 9

    # Set again a spatial filter
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1

    lyr = None
    ds = None

    # Test iterating without spatial index already built
    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    lyr = ds.GetLayerByName("several_polygons")
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    c = 0
    for f in lyr:
        c += 1
    assert c == 1
    lyr = None
    ds = None

    # Test GetFeatureCount(), with no matching feature when GEOS is available
    if ogrtest.have_geos():
        expected_count = 0
    else:
        expected_count = 5

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    lyr = ds.GetLayerByName("multipolygon")
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    assert lyr.GetFeatureCount() == expected_count
    lyr = None
    ds = None

    # Test iterating, with no matching feature when GEOS is available
    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    lyr = ds.GetLayerByName("multipolygon")
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    c = 0
    for f in lyr:
        c += 1
    assert c == expected_count
    assert lyr.GetFeatureCount() == expected_count
    lyr = None
    ds = None

    # test with a SetAttributeFilter() with an index too
    ds = ogr.Open("data/filegdb/test_spatial_index.gdb.zip")
    lyr = ds.GetLayerByName("test")
    lyr.SetAttributeFilter("id = 1")
    lyr.SetSpatialFilterRect(400000, 0, 500100, 4500100)
    assert lyr.GetFeatureCount() == 1
    c = 0
    for f in lyr:
        c += 1
    assert c == 1

    # No intersection between filters
    lyr.SetSpatialFilterRect(500100, 4500000, 500200, 4500100)
    assert lyr.GetFeatureCount() == 0
    c = 0
    for f in lyr:
        c += 1
    assert c == 0

    lyr.SetAttributeFilter(None)
    assert lyr.GetFeatureCount() == 154


###############################################################################
# Test reading a broken .spx that has an index depth of 1 instead of 2
# Simulates scenario of SWISSTLM3D_2022_LV95_LN02.gdb/a00000019.spx
# from https://data.geo.admin.ch/ch.swisstopo.swisstlm3d/swisstlm3d_2022-03/swisstlm3d_2022-03_2056_5728.gdb.zip
# which advertises nIndexDepth == 1 whereas it seems to be it should be 2.


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_read_broken_spx_wrong_index_depth():

    dirname = "/vsimem/test_ogr_openfilegdb_read_broken_spx_wrong_index_depth.gdb"
    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    for j in range(50):
        for i in range(50):
            p = ogr.CreateGeometryFromWkt("POINT(%d %d)" % (i, j))
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetGeometry(p)
            lyr.CreateFeature(f)
    ds = None

    # Manually patch index depth from 2 to 1
    f = gdal.VSIFOpenL(dirname + "/a00000009.spx", "rb+")
    assert f
    gdal.VSIFSeekL(f, 0, os.SEEK_END)
    pos = gdal.VSIFTellL(f) - 22 + 6
    gdal.VSIFSeekL(f, pos, os.SEEK_SET)
    assert gdal.VSIFReadL(1, 1, f) == b"\x02"
    gdal.VSIFSeekL(f, pos, os.SEEK_SET)
    gdal.VSIFWriteL(b"\x01", 1, 1, f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(0.5, 0.5, 48.5, 48.5)
    with gdal.quiet_errors():
        assert lyr.GetFeatureCount() == 48 * 48
    assert (
        "Cannot use /vsimem/test_ogr_openfilegdb_read_broken_spx_wrong_index_depth.gdb/a00000009.spx"
        in gdal.GetLastErrorMsg()
    )
    ds = None

    gdal.RmdirRecursive(dirname)


###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)


def test_ogr_openfilegdb_12():

    ds = ogr.Open("/vsizip/data/filegdb/test3005.gdb.zip")
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToWkt()
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3005)
    expected_wkt = sr.ExportToWkt()
    assert got_wkt == expected_wkt
    ds = None


###############################################################################
# Test opening a FGDB v9 with a non spatial table (#5673)


def test_ogr_openfilegdb_13():

    ds = ogr.Open("/vsizip/data/filegdb/ESSENCE_NAIPF_ORI_PROV_sub93.gdb.zip")
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "DDE_ESSEN_NAIPF_ORI_VUE"
    assert lyr.GetSpatialRef() is None
    assert lyr.GetGeomType() == ogr.wkbNone
    f = lyr.GetNextFeature()
    if f.GetField("GEOCODE") != "-673985,22+745574,77":
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test not nullable fields


def test_ogr_openfilegdb_14():

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    lyr = ds.GetLayerByName("testnotnullable")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_not_nullable"))
        .IsNullable()
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_nullable"))
        .IsNullable()
        == 1
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    ds = None


###############################################################################
# Test default values


def test_ogr_openfilegdb_15():

    ds = ogr.Open("data/filegdb/test_default_val.gdb.zip")
    lyr = ds.GetLayer(0)
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("STR"))
        .GetDefault()
        == "'default_val'"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("STR"))
        .GetWidth()
        == 50
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("INT32"))
        .GetDefault()
        == "123456788"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("INT16"))
        .GetDefault()
        == "12345"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("FLOAT32"))
        .GetDefault()
        .find("1.23")
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("FLOAT64"))
        .GetDefault()
        .find("1.23456")
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("DATETIME"))
        .GetDefault()
        == "'2015/06/30 12:34:56'"
    )


###############################################################################
# Read layers with sparse pages


def test_ogr_openfilegdb_16():

    ds = ogr.Open("data/filegdb/sparse.gdb.zip")
    lyr = ds.GetLayer(0)
    for fid in [2, 3, 4, 7, 8, 9, 10, 2049, 8191, 16384, 10000000, 10000001]:
        f = lyr.GetNextFeature()
        assert f.GetFID() == fid

    f = lyr.GetFeature(100000)
    assert f is None

    f = lyr.GetFeature(10000000 - 1)
    assert f is None
    f = lyr.GetNextFeature()
    assert f is None

    f = lyr.GetFeature(16384)
    assert f is not None


###############################################################################
# Read a MULTILINESTRING ZM with a dummy M array (#6528)


def test_ogr_openfilegdb_17():

    ds = ogr.Open("data/filegdb/multilinestringzm_with_dummy_m_array.gdb.zip")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None


###############################################################################
# Read curves


@pytest.mark.require_driver("CSV")
def test_ogr_openfilegdb_18():

    ds = ogr.Open("data/filegdb/curves.gdb")
    lyr = ds.GetLayerByName("line")
    ds_ref = ogr.Open("data/filegdb/curves_line.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    lyr = ds.GetLayerByName("polygon")
    ds_ref = ogr.Open("data/filegdb/curves_polygon.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    ds = ogr.Open("data/filegdb/curve_circle_by_center.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open("data/filegdb/curve_circle_by_center.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())


###############################################################################
# Test opening '.'


def test_ogr_openfilegdb_19():

    os.chdir("data/filegdb/curves.gdb")
    ds = ogr.Open(".")
    os.chdir("../../..")
    assert ds is not None


###############################################################################
# Read polygons with M component where the M of the closing point is not the
# one of the starting point (#7017)


@pytest.mark.require_driver("CSV")
def test_ogr_openfilegdb_20():

    ds = ogr.Open("data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open(
        "data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb.csv"
    )
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    ds = ogr.Open("data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open("data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())


###############################################################################
# Test selecting FID column with OGRSQL


def test_ogr_openfilegdb_21():

    ds = ogr.Open("data/filegdb/curves.gdb")
    sql_lyr = ds.ExecuteSQL("SELECT OBJECTID FROM polygon WHERE OBJECTID = 2")
    assert sql_lyr is not None
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName("polygon")
    lyr.SetAttributeFilter("OBJECTID = 2")
    f = lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation


@pytest.mark.require_geos
def test_ogr_openfilegdb_weird_winding_order():

    ds = ogr.Open(
        "/vsizip/data/filegdb/weird_winding_order_fgdb.zip/roads_clip Drawing.gdb"
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryCount() == 1
    assert g.GetGeometryRef(0).GetGeometryCount() == 17


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation


def test_ogr_openfilegdb_utc_datetime():

    ds = ogr.Open("data/filegdb/testdatetimeutc.gdb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # Check that the timezone +00 is present
    assert f.GetFieldAsString("EditDate") == "2020/06/22 07:49:36+00"


###############################################################################
# Test that field alias are correctly read and mapped to OGR field alternativ
# names


def test_ogr_fgdb_alias():
    ds = ogr.Open("data/filegdb/field_alias.gdb")

    lyr = ds.GetLayer(0)
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("text"))
        .GetAlternativeName()
        == "My Text Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("short_int"))
        .GetAlternativeName()
        == "My Short Int Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("long_int"))
        .GetAlternativeName()
        == "My Long Int Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("float"))
        .GetAlternativeName()
        == "My Float Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("double"))
        .GetAlternativeName()
        == "My Double Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("date"))
        .GetAlternativeName()
        == "My Date Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("blob"))
        .GetAlternativeName()
        == "My Blob Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("guid"))
        .GetAlternativeName()
        == "My GUID field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("raster"))
        .GetAlternativeName()
        == "My Raster Field"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("SHAPE_Length"))
        .GetAlternativeName()
        == ""
    )


###############################################################################
# Test reading field domains


def _check_domains(ds):

    assert set(ds.GetFieldDomainNames()) == {
        "MedianType",
        "RoadSurfaceType",
        "SpeedLimit",
    }

    with gdal.quiet_errors():
        assert ds.GetFieldDomain("i_dont_exist") is None
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("MaxSpeed"))
    assert fld_defn.GetDomainName() == "SpeedLimit"

    domain = ds.GetFieldDomain("SpeedLimit")
    assert domain is not None
    assert domain.GetName() == "SpeedLimit"
    assert domain.GetDescription() == "The maximun speed of the road"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetMinAsDouble() == 40.0
    assert domain.GetMaxAsDouble() == 100.0

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("MedianType"))
    assert fld_defn.GetDomainName() == "MedianType"

    domain = ds.GetFieldDomain("MedianType")
    assert domain is not None
    assert domain.GetName() == "MedianType"
    assert domain.GetDescription() == "Road median types."
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetEnumeration() == {"0": "None", "1": "Cement"}


###############################################################################
# Test reading field domains


def test_ogr_openfilegdb_read_domains():

    ds = gdal.OpenEx("data/filegdb/Domains.gdb", gdal.OF_VECTOR)
    _check_domains(ds)


###############################################################################
# Test writing field domains


def test_ogr_openfilegdb_write_domains_from_other_gdb(tmp_path):

    out_dir = tmp_path / "test_ogr_fgdb_write_domains.gdb"

    ds = gdal.VectorTranslate(
        out_dir, "data/filegdb/Domains.gdb", options="-f OpenFileGDB"
    )
    _check_domains(ds)

    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 1
    assert ds.TestCapability(ogr.ODsCDeleteFieldDomain) == 1
    assert ds.TestCapability(ogr.ODsCUpdateFieldDomain) == 1

    with gdal.quiet_errors():
        assert not ds.DeleteFieldDomain("not_existing")

    domain = ogr.CreateCodedFieldDomain(
        "unused_domain", "desc", ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None}
    )
    assert ds.AddFieldDomain(domain)
    assert ds.DeleteFieldDomain("unused_domain")
    domain = ds.GetFieldDomain("unused_domain")
    assert domain is None

    domain = ogr.CreateRangeFieldDomain(
        "SpeedLimit", "desc", ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, True
    )
    assert ds.UpdateFieldDomain(domain)

    ds = None

    ds = gdal.OpenEx(out_dir, allowed_drivers=["OpenFileGDB"])
    assert ds.GetFieldDomain("unused_domain") is None
    domain = ds.GetFieldDomain("SpeedLimit")
    assert domain.GetDescription() == "desc"
    ds = None

    try:
        shutil.rmtree(out_dir)
    except OSError:
        pass


###############################################################################
# Test reading layer hierarchy


@gdaltest.disable_exceptions()
def test_ogr_openfilegdb_read_layer_hierarchy():

    if False:
        # Test dataset produced with:
        from osgeo import ogr, osr

        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        ds = ogr.GetDriverByName("FileGDB").CreateDataSource("featuredataset.gdb")
        ds.CreateLayer(
            "fd1_lyr1", srs=srs, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd1"]
        )
        ds.CreateLayer(
            "fd1_lyr2", srs=srs, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd1"]
        )
        srs2 = osr.SpatialReference()
        srs2.ImportFromEPSG(32631)
        ds.CreateLayer("standalone", srs=srs2, geom_type=ogr.wkbPoint)
        srs3 = osr.SpatialReference()
        srs3.ImportFromEPSG(32632)
        ds.CreateLayer(
            "fd2_lyr", srs=srs3, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd2"]
        )

    ds = gdal.OpenEx("data/filegdb/featuredataset.gdb")
    rg = ds.GetRootGroup()

    assert rg.GetGroupNames() == ["fd1", "fd2"]
    assert rg.OpenGroup("not_existing") is None

    fd1 = rg.OpenGroup("fd1")
    assert fd1 is not None
    assert fd1.GetVectorLayerNames() == ["fd1_lyr1", "fd1_lyr2"]
    assert fd1.OpenVectorLayer("not_existing") is None
    assert len(fd1.GetGroupNames()) == 0

    fd1_lyr1 = fd1.OpenVectorLayer("fd1_lyr1")
    assert fd1_lyr1 is not None
    assert fd1_lyr1.GetName() == "fd1_lyr1"

    fd1_lyr2 = fd1.OpenVectorLayer("fd1_lyr2")
    assert fd1_lyr2 is not None
    assert fd1_lyr2.GetName() == "fd1_lyr2"

    fd2 = rg.OpenGroup("fd2")
    assert fd2 is not None
    assert fd2.GetVectorLayerNames() == ["fd2_lyr"]
    fd2_lyr = fd2.OpenVectorLayer("fd2_lyr")
    assert fd2_lyr is not None

    assert rg.GetVectorLayerNames() == ["standalone"]
    standalone = rg.OpenVectorLayer("standalone")
    assert standalone is not None


###############################################################################
# Test LIST_ALL_TABLES open option


def test_ogr_openfilegdb_list_all_tables_v10():
    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")
    assert ds is not None

    assert ds.GetLayerCount() == 37, "did not get expected layer count"
    layer_names = [ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]
    # should not be exposed by default
    for name in [
        "GDB_DBTune",
        "GDB_ItemRelationshipTypes",
        "GDB_ItemRelationships",
        "GDB_ItemTypes",
        "GDB_Items",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
    ]:
        assert name not in layer_names

    # Test LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx(
        "data/filegdb/testopenfilegdb.gdb.zip",
        gdal.OF_VECTOR,
        open_options=["LIST_ALL_TABLES=YES"],
    )

    assert ds_all_table.GetLayerCount() == 44, "did not get expected layer count"
    layer_names = [
        ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount())
    ]

    for name in [
        "linestring",
        "point",
        "multipoint",
        "GDB_DBTune",
        "GDB_ItemRelationshipTypes",
        "GDB_ItemRelationships",
        "GDB_ItemTypes",
        "GDB_Items",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
    ]:
        assert name in layer_names

    private_layers = [
        ds_all_table.GetLayer(i).GetName()
        for i in range(ds_all_table.GetLayerCount())
        if ds_all_table.IsLayerPrivate(i)
    ]
    for name in [
        "GDB_DBTune",
        "GDB_ItemRelationshipTypes",
        "GDB_ItemRelationships",
        "GDB_ItemTypes",
        "GDB_Items",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
    ]:
        assert name in private_layers
    for name in ["linestring", "point", "multipoint"]:
        assert name not in private_layers


def test_ogr_openfilegdb_list_all_tables_v9():
    ds = ogr.Open("data/filegdb/testopenfilegdb93.gdb.zip")
    assert ds is not None

    assert ds.GetLayerCount() == 22, "did not get expected layer count"
    layer_names = [ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]
    # should not be exposed by default
    for name in [
        "GDB_DBTune",
        "GDB_FeatureClasses",
        "GDB_FeatureDataset",
        "GDB_FieldInfo",
        "GDB_ObjectClasses",
        "GDB_Release",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
        "GDB_UserMetadata",
    ]:
        assert name not in layer_names

    # Test LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx(
        "data/filegdb/testopenfilegdb93.gdb.zip",
        gdal.OF_VECTOR,
        open_options=["LIST_ALL_TABLES=YES"],
    )

    assert ds_all_table.GetLayerCount() == 31, "did not get expected layer count"
    layer_names = [
        ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount())
    ]
    print(layer_names)

    for name in [
        "linestring",
        "point",
        "multipoint",
        "GDB_DBTune",
        "GDB_FeatureClasses",
        "GDB_FeatureDataset",
        "GDB_FieldInfo",
        "GDB_ObjectClasses",
        "GDB_Release",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
        "GDB_UserMetadata",
    ]:
        assert name in layer_names

    private_layers = [
        ds_all_table.GetLayer(i).GetName()
        for i in range(ds_all_table.GetLayerCount())
        if ds_all_table.IsLayerPrivate(i)
    ]
    for name in [
        "GDB_DBTune",
        "GDB_FeatureClasses",
        "GDB_FeatureDataset",
        "GDB_FieldInfo",
        "GDB_ObjectClasses",
        "GDB_Release",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
        "GDB_UserMetadata",
    ]:
        assert name in private_layers
    for name in ["linestring", "point", "multipoint"]:
        assert name not in private_layers


###############################################################################
# Test that non-spatial tables which are not present in GDB_Items are listed
# see https://github.com/OSGeo/gdal/issues/4463


def test_ogr_openfilegdb_non_spatial_table_outside_gdb_items():
    ds = ogr.Open("data/filegdb/table_outside_gdbitems.gdb")
    assert ds is not None

    assert ds.GetLayerCount() == 3, "did not get expected layer count"
    layer_names = set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()))
    assert layer_names == {"aquaduct", "flat_table1", "flat_table2"}

    # Test with the LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx(
        "data/filegdb/table_outside_gdbitems.gdb",
        gdal.OF_VECTOR,
        open_options=["LIST_ALL_TABLES=YES"],
    )

    assert ds_all_table.GetLayerCount() == 10, "did not get expected layer count"
    layer_names = set(
        ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount())
    )

    for name in [
        "aquaduct",
        "flat_table1",
        "flat_table2",
        "GDB_DBTune",
        "GDB_ItemRelationshipTypes",
        "GDB_ItemRelationships",
        "GDB_ItemTypes",
        "GDB_Items",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
    ]:
        assert name in layer_names

    private_layers = set(
        ds_all_table.GetLayer(i).GetName()
        for i in range(ds_all_table.GetLayerCount())
        if ds_all_table.IsLayerPrivate(i)
    )
    for name in [
        "GDB_DBTune",
        "GDB_ItemRelationshipTypes",
        "GDB_ItemRelationships",
        "GDB_ItemTypes",
        "GDB_Items",
        "GDB_SpatialRefs",
        "GDB_SystemCatalog",
    ]:
        assert name in private_layers
    for name in ["aquaduct", "flat_table1", "flat_table2"]:
        assert name not in private_layers


###############################################################################
# Test reading .gdb with strings encoded as UTF16 instead of UTF8
# (e.g. using -lco CONFIGURATION_KEYWORD=TEXT_UTF16 of FileGDB driver)


def test_ogr_openfilegdb_strings_utf16():
    ds = ogr.Open("data/filegdb/test_utf16.gdb.zip")
    assert ds is not None
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetDefault() == "'éven'"
    f = lyr.GetNextFeature()
    assert f["str"] == "évenéven"


###############################################################################
# Test reading .gdb where the CRS in the XML definition of the feature
# table is not consistent with the one of the feature dataset


def test_ogr_openfilegdb_inconsistent_crs_feature_dataset_and_feature_table():
    ds = ogr.Open("data/filegdb/inconsistent_crs_feature_dataset_and_feature_table.gdb")
    assert ds is not None
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"


###############################################################################
# Test reading a .spx file with the value_count field at 0
# (https://github.com/OSGeo/gdal/issues/5888)


def test_ogr_openfilegdb_spx_zero_in_value_count_trailer():
    ds = ogr.Open("data/filegdb/spx_zero_in_value_count_trailer.gdb")
    assert ds is not None
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(1, 1, 2, 2)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test reading .gdb with LengthFieldName / AreaFieldName


def test_ogr_openfilegdb_shape_length_shape_area_as_default_in_field_defn():
    ds = ogr.Open("data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb")
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Area")).GetDefault()
        == "FILEGEODATABASE_SHAPE_AREA"
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )


###############################################################################
# Test reading relationships


def test_ogr_openfilegdb_read_relationships():
    # no relationships
    ds = gdal.OpenEx("data/filegdb/Domains.gdb", gdal.OF_VECTOR)
    assert ds.GetRelationshipNames() is None

    # has relationships
    ds = gdal.OpenEx("data/filegdb/relationships.gdb", gdal.OF_VECTOR)
    assert set(ds.GetRelationshipNames()) == {
        "composite_many_to_many",
        "composite_one_to_many",
        "composite_one_to_one",
        "simple_attributed",
        "simple_backward_message_direction",
        "simple_both_message_direction",
        "simple_forward_message_direction",
        "simple_many_to_many",
        "simple_one_to_many",
        "simple_relationship_one_to_one",
        "points__ATTACHREL",
    }

    assert ds.GetRelationship("xxxx") is None

    rel = ds.GetRelationship("simple_relationship_one_to_one")
    assert rel is not None
    assert rel.GetName() == "simple_relationship_one_to_one"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_ONE
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetLeftMappingTableFields() is None
    assert rel.GetRightMappingTableFields() is None
    assert rel.GetForwardPathLabel() == "my forward path label"
    assert rel.GetBackwardPathLabel() == "my backward path label"
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("simple_one_to_many")
    assert rel is not None
    assert rel.GetName() == "simple_one_to_many"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("simple_many_to_many")
    assert rel is not None
    assert rel.GetName() == "simple_many_to_many"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == "simple_many_to_many"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetLeftMappingTableFields() == ["origin_foreign_key"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRightMappingTableFields() == ["destination_foreign_key"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_one_to_one")
    assert rel is not None
    assert rel.GetName() == "composite_one_to_one"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table3"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_ONE
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_one_to_many")
    assert rel is not None
    assert rel.GetName() == "composite_one_to_many"
    assert rel.GetLeftTableName() == "table5"
    assert rel.GetRightTableName() == "table4"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_many_to_many")
    assert rel is not None
    assert rel.GetName() == "composite_many_to_many"
    assert rel.GetLeftTableName() == "table6"
    assert rel.GetRightTableName() == "table7"
    assert rel.GetMappingTableName() == "composite_many_to_many"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetLeftMappingTableFields() == ["origin_foreign_key"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRightMappingTableFields() == ["dest_foreign_key"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("points__ATTACHREL")
    assert rel is not None
    assert rel.GetName() == "points__ATTACHREL"
    assert rel.GetLeftTableName() == "points"
    assert rel.GetRightTableName() == "points__ATTACH"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["OBJECTID"]
    assert rel.GetRightTableFields() == ["REL_OBJECTID"]
    assert rel.GetForwardPathLabel() == "attachment"
    assert rel.GetBackwardPathLabel() == "object"
    assert rel.GetRelatedTableType() == "media"


###############################################################################
# Test opening a read-only database in update mode


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
def test_ogr_openfilegdb_read_readonly_in_update_mode(tmp_path):

    if os.getuid() == 0:
        pytest.skip("running as root... skipping")

    shutil.copytree("data/filegdb/Domains.gdb", tmp_path / "testreadonly.gdb")
    os.chmod(tmp_path / "testreadonly.gdb", 0o555)
    for f in os.listdir(tmp_path / "testreadonly.gdb"):
        os.chmod(f"{tmp_path}/testreadonly.gdb/{f}", 0o555)

    try:
        with pytest.raises(Exception):
            ogr.Open(tmp_path / "testreadonly.gdb", update=1)

        assert ogr.Open(tmp_path / "testreadonly.gdb")

        # Only turn on a few system tables in read-write mode, but not the
        # layer of interest
        for f in os.listdir(tmp_path / "testreadonly.gdb"):
            if f.startswith("a00000001.") or f.startswith("a00000004."):
                os.chmod(f"{tmp_path}/testreadonly.gdb/{f}", 0o755)
        ds = ogr.Open(tmp_path / "testreadonly.gdb", update=1)
        lyr = ds.GetLayer(0)
        with pytest.raises(
            Exception, match="Cannot open Roads in update mode, but only in read-only"
        ):
            lyr.TestCapability(ogr.OLCSequentialWrite)
        assert lyr.TestCapability(ogr.OLCSequentialWrite) == 0

    finally:
        os.chmod(tmp_path / "testreadonly.gdb", 0o755)
        for f in os.listdir(tmp_path / "testreadonly.gdb"):
            os.chmod(f"{tmp_path}/testreadonly.gdb/{f}", 0o755)
        shutil.rmtree(tmp_path / "testreadonly.gdb")


###############################################################################
# Test reading a Integer64 field (ArcGIS Pro >= 3.2)


def test_ogr_openfilegdb_read_int64():

    ds = ogr.Open("data/filegdb/arcgis_pro_32_types.gdb")
    lyr = ds.GetLayerByName("big_int")
    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("big"))
    assert fld_defn.GetType() == ogr.OFTInteger64
    assert fld_defn.GetDefault() == "1234567890123456"

    f = lyr.GetNextFeature()
    assert f["short"] == 32767
    assert f["long"] == 2147483647
    assert f["big"] == 9007199254740991
    assert f["float"] == pytest.approx(3.4e38)
    assert f["double"] == pytest.approx(1.7976931348623e308)

    f = lyr.GetNextFeature()
    assert f["short"] == -32768
    assert f["long"] == -2147483647
    assert f["big"] == -9007199254740991
    assert f["float"] == pytest.approx(-3.4e38)
    assert f["double"] == pytest.approx(-1.7976931348623e308)

    lyr.SetAttributeFilter("big > 0")
    f = lyr.GetNextFeature()
    assert f["big"] == 9007199254740991

    lyr.SetAttributeFilter("big < 0")
    f = lyr.GetNextFeature()
    assert f["big"] == -9007199254740991

    with ds.ExecuteSQL("SELECT MIN(big), MAX(big) FROM big_int") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_big"] == -9007199254740991
        assert f["MAX_big"] == 9007199254740991


###############################################################################
# Test reading DateOnly, TimeOnly, TimestampOffset fields (ArcGIS Pro >= 3.2)


def test_ogr_openfilegdb_read_new_datetime_types():

    ds = ogr.Open("data/filegdb/arcgis_pro_32_types.gdb")

    lyr = ds.GetLayerByName("date_types")
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("date"))
    assert fld_defn.GetType() == ogr.OFTDateTime
    assert fld_defn.GetDefault() == "'2023/02/01 04:05:06'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("date_only"))
    assert fld_defn.GetType() == ogr.OFTDate
    assert fld_defn.GetDefault() == "'2023/02/01'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("time_only"))
    assert fld_defn.GetType() == ogr.OFTTime
    assert fld_defn.GetDefault() == "'04:05:06'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("timestamp_offset"))
    assert fld_defn.GetType() == ogr.OFTDateTime
    assert fld_defn.GetDefault() == "'2023/02/01 04:05:06.000+06:00'"

    f = lyr.GetNextFeature()
    assert f["date"] == "2023/11/29 13:14:15+00"
    assert f["date_only"] == "2023/11/29"
    assert f["time_only"] == "13:14:15"
    assert f["timestamp_offset"] == "2023/11/29 13:14:15-05"

    f = lyr.GetNextFeature()
    assert f["date"] == "2023/12/31 00:01:01+00"
    assert f["date_only"] == "2023/12/31"
    assert f["time_only"] == "00:01:01"
    assert f["timestamp_offset"] == "2023/12/31 00:01:01+10"

    with ds.ExecuteSQL('SELECT MIN("date"), MAX("date") FROM date_types') as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_date"] == "1901/01/01 00:01:01+00"
        assert f["MAX_date"] == "2023/12/31 00:01:01+00"

    with ds.ExecuteSQL(
        'SELECT MIN("date_only"), MAX("date_only") FROM date_types'
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_date_only"] == "1901/01/01"
        assert f["MAX_date_only"] == "2023/12/31"

    with ds.ExecuteSQL(
        'SELECT MIN("time_only"), MAX("time_only") FROM date_types'
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_time_only"] == "00:01:01"
        assert f["MAX_time_only"] == "13:14:15"

    with ds.ExecuteSQL(
        'SELECT MIN("timestamp_offset"), MAX("timestamp_offset") FROM date_types'
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_timestamp_offset"] == "1901/01/01 00:01:01+10"
        assert f["MAX_timestamp_offset"] == "2023/12/31 00:01:01+10"

    lyr = ds.GetLayerByName("date_types_high_precision")
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("date"))
    assert fld_defn.GetType() == ogr.OFTDateTime
    assert fld_defn.GetDefault() == "'2023/01/02 04:05:06'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("date_only"))
    assert fld_defn.GetType() == ogr.OFTDate
    assert fld_defn.GetDefault() == "'2023/01/02'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("time_only"))
    assert fld_defn.GetType() == ogr.OFTTime
    assert fld_defn.GetDefault() == "'04:05:06'"

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("timestamp_offset"))
    assert fld_defn.GetType() == ogr.OFTDateTime
    assert fld_defn.GetDefault() == "'2023/01/02 04:05:06.000+06:00'"

    f = lyr.GetNextFeature()
    assert f["date"] == "2023/11/29 13:14:15.678+00"
    assert f["date_only"] == "2023/11/29"
    assert f["time_only"] == "13:14:15"
    assert f["timestamp_offset"] == "2023/11/29 13:14:15-05"

    f = lyr.GetNextFeature()
    assert f["date"] == "2023/12/31 00:01:01.001+00"
    assert f["date_only"] == "2023/12/31"
    assert f["time_only"] == "00:01:01"
    assert f["timestamp_offset"] == "2023/12/31 00:01:01+10"

    with ds.ExecuteSQL(
        'SELECT MIN("timestamp_offset"), MAX("timestamp_offset") FROM date_types_high_precision'
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_timestamp_offset"] == "1900/12/31 14:01:01"
        assert f["MAX_timestamp_offset"] == "2023/12/30 14:01:01"


###############################################################################
# Test GetExtent() and GetExtent3D()


def test_ogr_openfilegdb_get_extent_getextent3d():

    ds = ogr.Open("data/filegdb/testopenfilegdb.gdb.zip")

    lyr = ds.GetLayerByName("point")
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    assert lyr.GetExtent() == (1.0, 1.0, 2.0, 2.0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.GetExtent3D() == (1.0, 1.0, 2.0, 2.0, float("inf"), float("-inf"))

    lyr = ds.GetLayerByName("point25D")
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    assert lyr.GetExtent() == (1.0, 1.0, 2.0, 2.0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.GetExtent3D() == (1.0, 1.0, 2.0, 2.0, 3.0, 3.0)
    lyr.SetAttributeFilter("1=1")
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D) == 0
    assert lyr.GetExtent3D() == pytest.approx((1.0, 1.0, 2.0, 2.0, 3.0, 3.0))

    lyr = ds.GetLayerByName("none")
    lyr.TestCapability(ogr.OLCFastGetExtent)
    assert lyr.GetExtent(can_return_null=True) is None
    lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert lyr.GetExtent3D(can_return_null=True) is None

    ds = ogr.Open("data/filegdb/arcgis_pro_32_types.gdb")
    lyr = ds.GetLayerByName("date_types")
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    assert lyr.GetExtent() == pytest.approx(
        (
            -109.26723474299996,
            -104.69136945899999,
            40.407842636000055,
            41.69941751400006,
        )
    )
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D) == 0
    assert lyr.GetExtent3D() == pytest.approx(
        (
            -109.26723474299996,
            -104.69136945899999,
            40.407842636000055,
            41.69941751400006,
            0.0,
            0.0,
        )
    )
