#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB driver testing (write side)
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct
import sys

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("OpenFileGDB")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    # remove FileGDB driver before running tests
    filegdb_driver = ogr.GetDriverByName("FileGDB")
    if filegdb_driver is not None:
        filegdb_driver.Deregister()

    with gdaltest.config_option(
        "OGR_OPENFILEGDB_ERROR_ON_INCONSISTENT_BUFFER_MAX_SIZE", "YES"
    ):
        yield

    if filegdb_driver is not None:
        print("Reregistering FileGDB driver")
        filegdb_driver.Register()


###############################################################################


def test_ogr_openfilegdb_invalid_filename(tmp_vsimem):

    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(
            tmp_vsimem / "bad.extension"
        )
        assert ds is None

    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(
            "/parent/directory/does/not/exist.gdb"
        )
        assert ds is None


###############################################################################


def test_ogr_openfilegdb_write_empty(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"
    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds is not None
    assert ds.GetLayerCount() == 0
    ds = None

    gdal.RmdirRecursive(dirname)


###############################################################################


@pytest.mark.parametrize("use_synctodisk", [False, True])
def test_ogr_openfilegdb_write_field_types(tmp_vsimem, use_synctodisk):

    dirname = tmp_vsimem / "out.gdb"
    try:
        ds = gdal.GetDriverByName("OpenFileGDB").Create(
            dirname, 0, 0, 0, gdal.GDT_Unknown
        )
        assert ds.TestCapability(ogr.ODsCCreateLayer) == 1
        lyr = ds.CreateLayer(
            "test",
            geom_type=ogr.wkbPoint,
            options=[
                "COLUMN_TYPES=xml=esriFieldTypeXML,globalId=esriFieldTypeGlobalID,guid=esriFieldTypeGUID"
            ],
        )

        # Cannot create field with same name as an existing field (here the geometry one)
        with gdal.quiet_errors():
            fld_defn = ogr.FieldDefn("SHAPE", ogr.OFTString)
            assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("str", ogr.OFTString)
        fld_defn.SetAlternativeName("alias")
        fld_defn.SetDefault("'default val'")
        fld_defn.SetWidth(100)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("str_not_nullable", ogr.OFTString)
        fld_defn.SetNullable(False)
        with gdaltest.config_option("OPENFILEGDB_DEFAULT_STRING_WIDTH", "12345"):
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldCount() - 1)
            .GetWidth()
            == 12345
        )

        fld_defn = ogr.FieldDefn("str_default_width", ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldCount() - 1)
            .GetWidth()
            == 0
        )

        fld_defn = ogr.FieldDefn("int32", ogr.OFTInteger)
        fld_defn.SetDefault("-12345")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        if use_synctodisk:
            assert lyr.SyncToDisk() == ogr.OGRERR_NONE
            # To check that we can rewrite-in-place a growing field description
            # section when it is at end of file

        fld_defn = ogr.FieldDefn("int16", ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("float64", ogr.OFTReal)
        fld_defn.SetDefault("-1.25")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("float32", ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("int64", ogr.OFTInteger64)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("binary", ogr.OFTBinary)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("xml", ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("globalId", ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("guid", ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str", "my str")
        f.SetField("str_not_nullable", "my str_not_nullable")
        f.SetField("int32", 123456789)
        f.SetField("int16", -32768)
        f.SetField("float64", 1.23456789)
        f.SetField("float32", 1.25)
        f.SetField("int64", 12345678912345)
        f.SetField("dt", "2022-11-04T12:34:56+02:00")
        f.SetField("binary", b"\x00\xff\x7f")
        f.SetField("xml", "<some_elt/>")
        f.SetField("guid", "{12345678-9ABC-DEF0-1234-567890ABCDEF}")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str_not_nullable", "my str_not_nullable")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        with gdal.quiet_errors():
            gdal.ErrorReset()
            assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE
            assert (
                gdal.GetLastErrorMsg()
                == "Attempting to write null/empty field in non-nullable field"
            )

        assert ds.FlushCache() == gdal.CE_None
        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("str")
        )
        assert fld_defn.GetAlternativeName() == "alias"
        assert fld_defn.GetDefault() == "'default val'"
        assert fld_defn.GetWidth() == 100

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("str_not_nullable")
        )
        assert fld_defn.IsNullable() == False
        assert fld_defn.GetWidth() == 12345

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("str_default_width")
        )
        assert fld_defn.GetWidth() == 0

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("int16")
        )
        assert fld_defn.GetSubType() == ogr.OFSTInt16

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("int32")
        )
        assert fld_defn.GetSubType() == ogr.OFSTNone
        assert fld_defn.GetDefault() == "-12345"

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("float32")
        )
        assert fld_defn.GetSubType() == ogr.OFSTFloat32

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("float64")
        )
        assert fld_defn.GetSubType() == ogr.OFSTNone
        assert fld_defn.GetDefault() == "-1.25"

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("int64")
        )
        assert fld_defn.GetType() == ogr.OFTReal

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("dt")
        )
        assert fld_defn.GetType() == ogr.OFTDateTime

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("binary")
        )
        assert fld_defn.GetType() == ogr.OFTBinary

        f = lyr.GetNextFeature()
        assert f["str"] == "my str"
        assert f["str_not_nullable"] == "my str_not_nullable"
        assert f["int32"] == 123456789
        assert f["int16"] == -32768
        assert f["float64"] == 1.23456789
        assert f["float32"] == 1.25
        assert f["int64"] == 12345678912345.0
        assert f["dt"] == "2022/11/04 10:34:56+00"
        assert f["binary"] == "00FF7F"
        assert f["xml"] == "<some_elt/>"
        assert f["globalId"].startswith("{")  # check that a GlobaID has been generated
        assert f["guid"] == "{12345678-9ABC-DEF0-1234-567890ABCDEF}"

        f = lyr.GetNextFeature()
        assert f["str"] == "default val"
        assert f["str_not_nullable"] == "my str_not_nullable"
        assert not f.IsFieldSetAndNotNull("int16")

        ds = None

        with gdaltest.config_option("OPENFILEGDB_REPORT_GENUINE_FIELD_WIDTH", "YES"):
            ds = ogr.Open(dirname)
            lyr = ds.GetLayer(0)
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(
                lyr.GetLayerDefn().GetFieldIndex("str_default_width")
            )
            assert fld_defn.GetWidth() == 65536
            ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


testdata = [
    (ogr.wkbPoint, ogr.wkbPoint, None, None),
    (ogr.wkbPoint, ogr.wkbPoint, "POINT EMPTY", None),
    (ogr.wkbPoint, ogr.wkbPoint, "POINT (1 2)", None),
    (ogr.wkbPoint25D, ogr.wkbPoint25D, "POINT Z (1 2 3)", None),
    (ogr.wkbPointM, ogr.wkbPointM, "POINT M (1 2 3)", None),
    (ogr.wkbPointZM, ogr.wkbPointZM, "POINT ZM (1 2 3 4)", None),
    (ogr.wkbLineString, ogr.wkbMultiLineString, "LINESTRING EMPTY", None),
    (ogr.wkbLineString, ogr.wkbMultiLineString, "LINESTRING (1 2,3 4,-1 -2)", None),
    (
        ogr.wkbLineString25D,
        ogr.wkbMultiLineString25D,
        "LINESTRING Z (1 2 10,3 4 20,-1 -2 15)",
        None,
    ),
    (
        ogr.wkbLineStringM,
        ogr.wkbMultiLineStringM,
        "LINESTRING M (1 2 10,3 4 20,-1 -2 15)",
        None,
    ),
    (
        ogr.wkbLineStringZM,
        ogr.wkbMultiLineStringZM,
        "LINESTRING ZM (1 2 10 100,3 4 20 200,-1 -2 15 150)",
        None,
    ),
    (ogr.wkbPolygon, ogr.wkbMultiPolygon, "POLYGON EMPTY", None),
    (ogr.wkbPolygon, ogr.wkbMultiPolygon, "POLYGON ((0 0,0 1,1 1,0 0))", None),
    (
        ogr.wkbPolygon,
        ogr.wkbMultiPolygon,
        "POLYGON ((0 0,1 1,0 1,0 0))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
    ),  # must fix winding order
    (
        ogr.wkbPolygon,
        ogr.wkbMultiPolygon,
        "POLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
        None,
    ),
    (
        ogr.wkbPolygon,
        ogr.wkbMultiPolygon,
        "POLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)))",
    ),  # must fix winding order of inner ring
    (
        ogr.wkbPolygon25D,
        ogr.wkbMultiPolygon25D,
        "POLYGON Z ((0 0 10,0 1 30,1 1 20,0 0 10))",
        None,
    ),
    (
        ogr.wkbPolygonM,
        ogr.wkbMultiPolygonM,
        "POLYGON M ((0 0 10,0 1 30,1 1 20,0 0 10))",
        None,
    ),
    (
        ogr.wkbPolygonZM,
        ogr.wkbMultiPolygonZM,
        "POLYGON ZM ((0 0 10 100,0 1 30 300,1 1 20 200,0 0 10 100))",
        None,
    ),
    (ogr.wkbMultiPoint, ogr.wkbMultiPoint, "MULTIPOINT (1 2)", None),
    (ogr.wkbMultiPoint, ogr.wkbMultiPoint, "MULTIPOINT (1 2,-3 -4,5 6)", None),
    (
        ogr.wkbMultiPoint25D,
        ogr.wkbMultiPoint25D,
        "MULTIPOINT Z ((1 2 10),(-3 -4 30),(5 6 20))",
        None,
    ),
    (
        ogr.wkbMultiPointM,
        ogr.wkbMultiPointM,
        "MULTIPOINT M ((1 2 10),(-3 -4 30),(5 6 20))",
        None,
    ),
    (
        ogr.wkbMultiPointZM,
        ogr.wkbMultiPointZM,
        "MULTIPOINT ZM ((1 2 10 100),(-3 -4 30 300),(5 6 20 200))",
        None,
    ),
    (ogr.wkbMultiLineString, ogr.wkbMultiLineString, "MULTILINESTRING EMPTY", None),
    (
        ogr.wkbMultiLineString,
        ogr.wkbMultiLineString,
        "MULTILINESTRING ((1 2,3 4,-1 -2),(3 -4,5 6))",
        None,
    ),
    (
        ogr.wkbMultiLineString25D,
        ogr.wkbMultiLineString25D,
        "MULTILINESTRING Z ((1 2 10,3 4 20,-1 -2 15),(3 -4 10,5 6 20))",
        None,
    ),
    (
        ogr.wkbMultiLineStringM,
        ogr.wkbMultiLineStringM,
        "MULTILINESTRING M ((1 2 10,3 4 20,-1 -2 15),(3 -4 10,5 6 20))",
        None,
    ),
    (
        ogr.wkbMultiLineStringZM,
        ogr.wkbMultiLineStringZM,
        "MULTILINESTRING ZM ((1 2 10 100,3 4 20 200,-1 -2 15 150),(3 -4 10 200,5 6 20 100))",
        None,
    ),
    (ogr.wkbMultiPolygon, ogr.wkbMultiPolygon, "MULTIPOLYGON EMPTY", None),
    (
        ogr.wkbMultiPolygon,
        ogr.wkbMultiPolygon,
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        None,
    ),
    (
        ogr.wkbMultiPolygon,
        ogr.wkbMultiPolygon,
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)),((10 10,10 11,11 11,10 10)))",
        None,
    ),
    (
        ogr.wkbMultiPolygon25D,
        ogr.wkbMultiPolygon25D,
        "MULTIPOLYGON Z (((0 0 10,0 1 30,1 1 20,0 0 10)),((10 10 100,10 11 120,11 11 110,10 10 100)))",
        None,
    ),
    (
        ogr.wkbMultiPolygonM,
        ogr.wkbMultiPolygonM,
        "MULTIPOLYGON M (((0 0 10,0 1 30,1 1 20,0 0 10)),((10 10 100,10 11 120,11 11 110,10 10 100)))",
        None,
    ),
    (
        ogr.wkbMultiPolygonZM,
        ogr.wkbMultiPolygonZM,
        "MULTIPOLYGON ZM (((0 0 10 100,0 1 30 300,1 1 20 200,0 0 10 100)),((10 10 100 1000,10 11 120 1100,11 11 110 900,10 10 100 1000)))",
        None,
    ),
    (
        ogr.wkbCircularString,
        ogr.wkbMultiLineString,
        "CIRCULARSTRING (0 0,1 1,2 0)",
        "MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0)))",
    ),
    (
        ogr.wkbCircularStringZM,
        ogr.wkbMultiLineStringZM,
        "CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0)",
        "MULTICURVE ZM  (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0)))",
    ),
    (
        ogr.wkbCompoundCurve,
        ogr.wkbMultiLineString,
        "COMPOUNDCURVE ((0 0,1 1,2 0))",
        "MULTILINESTRING ((0 0,1 1,2 0))",
    ),
    (
        ogr.wkbCompoundCurve,
        ogr.wkbMultiLineString,
        "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0))",
        "MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0)))",
    ),
    (
        ogr.wkbCompoundCurveZM,
        ogr.wkbMultiLineStringZM,
        "COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0),(2 0 10 0,3 0 11 1))",
        "MULTICURVE ZM (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0),(2 0 10 0,3 0 11 1)))",
    ),
    (
        ogr.wkbMultiCurve,
        ogr.wkbMultiLineString,
        "MULTICURVE(CIRCULARSTRING (0 0,1 1,2 0),(0 0,1 1),COMPOUNDCURVE (CIRCULARSTRING(10 10,11 11,12 10)))",
        "MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0)),(0 0,1 1),COMPOUNDCURVE (CIRCULARSTRING(10 10,11 11,12 10)))",
    ),
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON ((0 0,1 1,0 1,0 0))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
    ),
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
    ),  # must fix winding order
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)))",
    ),  # must fix winding order of inner ring
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0)))",
        "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0))))",
    ),
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON (COMPOUNDCURVE((0 0,1 0),CIRCULARSTRING(1 0,1.5 0.5,1 1),(1 1,0 1,0 0)))",
        "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0))))",
    ),  # must fix winding order
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON (CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0))",
        "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0))))",
    ),
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON (CIRCULARSTRING(-10 0,0 -10,10 0,0 10,-10 0))",
        "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0))))",
    ),  # must fix winding order
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON ((-100 -100,-100 100,100 100,100 -100,-100 -100),CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0))",
        "MULTISURFACE (CURVEPOLYGON ((-100 -100,-100 100,100 100,100 -100,-100 -100),COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 -10,10 0),CIRCULARSTRING (10 0,0 10,-10 0))))",
    ),  # must fix winding order of inner ring
    (
        ogr.wkbCurvePolygon,
        ogr.wkbMultiPolygon,
        "CURVEPOLYGON (CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0),COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0)))",
        "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0)),COMPOUNDCURVE ((0 0,1 0),CIRCULARSTRING (1 0,1.5 0.5,1 1),(1 1,0 1,0 0))))",
    ),  # must fix winding order of inner ring
    (
        ogr.wkbMultiSurface,
        ogr.wkbMultiPolygon,
        "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
        None,
    ),
    (
        ogr.wkbMultiSurfaceZM,
        ogr.wkbMultiPolygonZM,
        "MULTISURFACE ZM (((100 0 10 100,100 1 10 101,101 1 10 102,100 0 10 100)),CURVEPOLYGON ZM (CIRCULARSTRING ZM(-10 0 10 0,0 10 10 0,10 0 10 0,0 -1010 0,-10 0 10 0)))",
        "MULTISURFACE ZM (CURVEPOLYGON ZM ((100 0 10 100,100 1 10 101,101 1 10 102,100 0 10 100)),CURVEPOLYGON ZM (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (-10 0 10 0,0 10 10 0,10 0 10 0),CIRCULARSTRING ZM (10 0 10 0,0 -1010 10 0,-10 0 10 0))))",
    ),
    (
        ogr.wkbGeometryCollection25D,
        ogr.wkbUnknown | ogr.wkb25DBit,
        "GEOMETRYCOLLECTION Z (TIN Z (((0 0 10,0 1 11,1 1 12,0 0 10)),((0 0 10,1 1 12,1 0 11,0 0 10))),TIN Z (((10 10 0,10 11 0,11 11 0,10 10 0))))",
        None,
    ),
]


@pytest.mark.parametrize("geom_type,read_geom_type,wkt,expected_wkt", testdata)
def test_ogr_openfilegdb_write_all_geoms(
    tmp_vsimem, geom_type, read_geom_type, wkt, expected_wkt
):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    options = [
        "XORIGIN=-1000",
        "YORIGIN=-2000",
        "XYSCALE=10000",
        "XYTOLERANCE=0.001",
    ]
    lyr = ds.CreateLayer("test", geom_type=geom_type, options=options)
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    if wkt:
        ref_geom = ogr.CreateGeometryFromWkt(wkt)
        assert ref_geom is not None
    else:
        ref_geom = None
    f.SetGeometry(ref_geom)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == read_geom_type
    f = lyr.GetNextFeature()
    got_geom = f.GetGeometryRef()
    if ref_geom is None or ref_geom.IsEmpty():
        assert got_geom is None
    else:
        if expected_wkt:
            expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
        else:
            expected_geom = ogr.ForceTo(ref_geom, read_geom_type)
        ogrtest.check_feature_geometry(got_geom, expected_geom)

    # Test presence of a spatial index
    if (
        ref_geom is not None
        and not ref_geom.IsEmpty()
        and ogr.GT_Flatten(geom_type) != ogr.wkbPoint
        and (
            ogr.GT_Flatten(geom_type) != ogr.wkbMultiPoint
            or ref_geom.GetPointCount() > 1
        )
        and geom_type != ogr.wkbGeometryCollection25D
    ):
        assert gdal.VSIStatL(f"{dirname}/a00000009.spx") is not None
        minx, maxx, miny, maxy = ref_geom.GetEnvelope()
        lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
        lyr.ResetReading()
        assert lyr.GetNextFeature() is not None

    ds = None


###############################################################################


@pytest.mark.parametrize(
    "geom_type,wkt",
    [
        (ogr.wkbPoint, "LINESTRING (0 0,1 1)"),
        (ogr.wkbLineString, "POINT (0 0)"),
        (ogr.wkbPolygon, "LINESTRING (0 0,1 1)"),
        (ogr.wkbTINZ, "LINESTRING (0 0,1 1)"),
    ],
)
def test_ogr_openfilegdb_write_bad_geoms(tmp_vsimem, geom_type, wkt):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    lyr = ds.CreateLayer("test", geom_type=geom_type)
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    ref_geom = ogr.CreateGeometryFromWkt(wkt)
    f.SetGeometry(ref_geom)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_text_utf16(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbNone, options=["CONFIGURATION_KEYWORD=TEXT_UTF16"]
    )
    assert lyr is not None
    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    fld_defn.SetDefault("'éven'")
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "évenéven")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname)
    assert ds is not None
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetDefault() == "'éven'"
    f = lyr.GetNextFeature()
    assert f["str"] == "évenéven"
    ds = None


###############################################################################


def gdbtablx_has_bitmap(gdbtablx_filename):
    def read_uint32(f):
        v = gdal.VSIFReadL(1, 4, f)
        return struct.unpack("<I", v)[0]

    f = gdal.VSIFOpenL(gdbtablx_filename, "rb")
    assert f
    gdal.VSIFSeekL(f, 4, 0)
    n1024Blocks = read_uint32(f)
    read_uint32(f)  # nfeaturesx
    size_tablx_offsets = read_uint32(f)
    if n1024Blocks != 0:
        gdal.VSIFSeekL(f, size_tablx_offsets * 1024 * n1024Blocks + 16, 0)
        nBitmapInt32Words = read_uint32(f)
        if nBitmapInt32Words != 0:
            gdal.VSIFCloseL(f)
            return True
    gdal.VSIFCloseL(f)
    return False


###############################################################################


@pytest.mark.parametrize(
    "has_bitmap,ids",
    [
        (False, (1,)),  # First feature of first page
        (False, (1, (1, False))),  # Inserting already inserted feature
        (False, (1024,)),  # Last feature of first page
        (False, (1, 1025)),
        (False, (1, 1025, 2049)),
        (False, (1, 1025, 2)),
        (False, (1, 1025, 2, 2049)),
        (True, (1025,)),  # First feature of second page
        (True, (1025, 1026)),
        (True, (1026, 2049, 1025)),
        (True, (1026, 2049, 1025, 2050)),
        (True, (1025, 1 + 4 * 1024)),
        (True, (1025, 1 + 9 * 1024)),  # 2-byte bitmap
        (True, ((1 << 31) - 1,)),  # Biggest possible FID
        (True, (1025, 1)),
        (True, (2049, 1025, 1)),
        (True, (1, 2049, 1025)),
        (True, (1, 2049, 2048, 2050, 1025)),
        (False, (((1 << 31), False),)),  # Illegal FID
        (False, ((0, False),)),  # Illegal FID
        (False, ((-2, False),)),  # Illegal FID
    ],
)
@pytest.mark.parametrize("sync", [True, False])
def test_ogr_openfilegdb_write_create_feature_with_id_set(
    tmp_vsimem, has_bitmap, ids, sync
):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    for id in ids:
        if isinstance(id, tuple):
            id, ok = id
        else:
            ok = True
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(id)
        if id < (1 << 31):
            f.SetField(0, id)
        if ok:
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        else:
            with gdal.quiet_errors():
                assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
        if sync:
            lyr.SyncToDisk()
    ds = None

    if has_bitmap:
        assert gdbtablx_has_bitmap(f"{dirname}/a00000009.gdbtablx")
    else:
        assert not gdbtablx_has_bitmap(f"{dirname}/a00000009.gdbtablx")

    # Check that everything has been written correctly
    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    ids_only = []
    for id in ids:
        if isinstance(id, tuple):
            id, ok = id
            if ok:
                ids_only.append(id)
        else:
            ids_only.append(id)
    for id in sorted(ids_only):
        gdal.ErrorReset()
        f = lyr.GetNextFeature()
        assert gdal.GetLastErrorMsg() == ""
        assert f.GetFID() == id
        assert f[0] == id
    assert lyr.GetNextFeature() is None
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_delete_feature(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
    assert lyr.DeleteFeature(1) == ogr.OGRERR_NONE
    assert lyr.DeleteFeature(0) == ogr.OGRERR_NON_EXISTING_FEATURE
    assert lyr.DeleteFeature(1) == ogr.OGRERR_NON_EXISTING_FEATURE
    assert lyr.DeleteFeature(3) == ogr.OGRERR_NON_EXISTING_FEATURE
    assert lyr.DeleteFeature(-1) == ogr.OGRERR_NON_EXISTING_FEATURE
    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_update_feature(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "one")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField("str", "three")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(4)
    f.SetField("str", "four")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(5)
    assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    # rewrite same size
    f.SetField("str", "ONE")
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(4)
    # larger feature
    f.SetField("str", "four4")
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    # smaller feature
    f.SetField("str", "3")
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3

    f = lyr.GetNextFeature()
    assert f["str"] == "ONE"

    f = lyr.GetNextFeature()
    assert f["str"] == "3"

    f = lyr.GetNextFeature()
    assert f["str"] == "four4"
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_add_field_to_non_empty_table(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "one")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "two")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    fld_defn = ogr.FieldDefn(
        "cannot_add_non_nullable_field_without_default_val", ogr.OFTString
    )
    fld_defn.SetNullable(False)
    with gdal.quiet_errors():
        assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

    # No need to rewrite the file
    assert lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str3", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str4", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str5", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str6", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str7", ogr.OFTString)) == ogr.OGRERR_NONE

    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["str"] == "one"
    assert f["str2"] is None
    assert f["str7"] is None
    f = lyr.GetNextFeature()
    assert f["str"] == "two"
    assert f["str2"] is None
    assert f["str7"] is None
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_nullable(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "one")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "two")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    assert lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str3", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str4", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str5", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str6", ogr.OFTString)) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("str7", ogr.OFTString)) == ogr.OGRERR_NONE

    # Will trigger a table rewrite
    assert lyr.CreateField(ogr.FieldDefn("str8", ogr.OFTString)) == ogr.OGRERR_NONE

    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["str"] == "one"
    assert f["str2"] is None
    assert f["str7"] is None
    assert f["str8"] is None
    f = lyr.GetNextFeature()
    assert f["str"] == "two"
    assert f["str2"] is None
    assert f["str7"] is None
    assert f["str8"] is None
    ds = None


###############################################################################

modify_inplace_options = [
    {"OPENFILEGDB_MODIFY_IN_PLACE": "FALSE"},
    {"OPENFILEGDB_MODIFY_IN_PLACE": "TRUE"},
]
if sys.platform != "win32":
    modify_inplace_options.append(
        {"OPENFILEGDB_MODIFY_IN_PLACE": "FALSE", "OPENFILEGDB_SIMUL_WIN32": "TRUE"}
    )


@pytest.mark.parametrize("options", modify_inplace_options)
@pytest.mark.parametrize("location", ["vsimem", "disk"])
def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_non_nullable(
    options, location, tmp_path, tmp_vsimem
):

    if location == "vsimem":
        dirname = tmp_vsimem / "out.gdb"
    else:
        dirname = tmp_path / "out.gdb"

    with gdaltest.config_options(options):
        ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str", "one")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str", "two")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("'default val'")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("int16", ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("-32768")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("int32", ogr.OFTInteger)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("123456789")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("float32", ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("1.25")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("float64", ogr.OFTReal)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("1.23456789")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("'2022-11-04T12:34:56+02:00'")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn("dt_invalid_default", ogr.OFTDateTime)
        fld_defn.SetDefault("'foo'")
        with gdal.quiet_errors():
            assert lyr.CreateField(fld_defn, False) == ogr.OGRERR_FAILURE
            assert gdal.GetLastErrorMsg() == "Cannot parse foo as a date time"

        fld_defn = ogr.FieldDefn("dt_CURRENT_TIMESTAMP", ogr.OFTDateTime)
        fld_defn.SetDefault("CURRENT_TIMESTAMP")
        with gdal.quiet_errors():
            assert lyr.CreateField(fld_defn, False) == ogr.OGRERR_FAILURE
            assert (
                gdal.GetLastErrorMsg()
                == "CURRENT_TIMESTAMP is not supported as a default value in File Geodatabase"
            )

        fld_defn = ogr.FieldDefn("dt_CURRENT_TIMESTAMP_2", ogr.OFTDateTime)
        fld_defn.SetDefault("CURRENT_TIMESTAMP")
        with gdal.quiet_errors():
            assert lyr.CreateField(fld_defn, True) == ogr.OGRERR_NONE
            assert (
                gdal.GetLastErrorMsg()
                == "CURRENT_TIMESTAMP is not supported as a default value in File Geodatabase"
            )

        assert lyr.SyncToDisk() == ogr.OGRERR_NONE

        ds = None

        assert gdal.VSIStatL(dirname / "a00000009.gdbtable.backup") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtablx.backup") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtable.compress") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtablx.compress") is None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["str"] == "one"
        assert f["str2"] == "default val"
        assert f["int16"] == -32768
        assert f["int32"] == 123456789
        assert f["float32"] == 1.25
        assert f["float64"] == 1.23456789
        assert f["dt"] == "2022/11/04 10:34:56+00"
        assert f.IsFieldNull("dt_CURRENT_TIMESTAMP_2")
        f = lyr.GetNextFeature()
        assert f["str"] == "two"
        assert f["str2"] == "default val"
        ds = None


###############################################################################


@pytest.mark.parametrize("options", modify_inplace_options)
@pytest.mark.parametrize("location", ["vsimem", "disk"])
def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_non_nullable_simul_error(
    location, options, tmp_path, tmp_vsimem
):

    if location == "vsimem":
        dirname = tmp_vsimem / "out.gdb"
    else:
        dirname = tmp_path / "out.gdb"

    with gdaltest.config_options(options):
        ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str", "one")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str", "two")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
        fld_defn.SetNullable(False)
        fld_defn.SetDefault("'default val'")
        with gdal.quiet_errors():
            with gdaltest.config_option(
                "OPENFILEGDB_SIMUL_ERROR_IN_RewriteTableToAddLastAddedField", "TRUE"
            ):
                assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

        ds = None

        assert gdal.VSIStatL(dirname / "a00000009.gdbtable.backup") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtablx.backup") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtable.compress") is None
        assert gdal.VSIStatL(dirname / "a00000009.gdbtablx.compress") is None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 1
        f = lyr.GetNextFeature()
        assert f["str"] == "one"
        f = lyr.GetNextFeature()
        assert f["str"] == "two"
        ds = None


###############################################################################


def test_ogr_openfilegdb_write_add_field_after_reopening(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "one")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "two")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString)) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f["str"] == "one"
    assert f["str2"] is None
    f = lyr.GetNextFeature()
    assert f["str"] == "two"

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)

    assert "<Name>str</Name>" in xml
    assert "<Name>str2</Name>" in xml

    ds = None


###############################################################################


@pytest.mark.parametrize("use_synctodisk", [False, True])
@pytest.mark.parametrize("field_to_delete", [0, 1])
def test_ogr_openfilegdb_write_delete_field(
    tmp_vsimem, use_synctodisk, field_to_delete
):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    assert lyr.CreateField(ogr.FieldDefn("str1", ogr.OFTString)) == ogr.OGRERR_NONE

    fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
    fld_defn.SetNullable(False)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    assert lyr.CreateField(ogr.FieldDefn("str3", ogr.OFTString)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str1", "str1_1")
    f.SetField("str2", "str2_1")
    f.SetField("str3", "str3_1")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str1", "str1_2")
    f.SetField("str2", "str2_2")
    f.SetField("str3", "str3_2")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    if use_synctodisk:
        assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    assert lyr.DeleteField(field_to_delete) == ogr.OGRERR_NONE
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    if field_to_delete == 0:
        other_field = "str2"
    else:
        other_field = "str1"

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(other_field, "str2_3")
    f.SetField("str3", "str3_3")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    def check_values(lyr):
        f = lyr.GetNextFeature()
        assert f[other_field].endswith("_1")
        assert f["str3"] == "str3_1"
        assert f.GetGeometryRef() is not None
        f = None

        f = lyr.GetNextFeature()
        assert f[other_field].endswith("_2")
        assert f["str3"] == "str3_2"
        assert f.GetGeometryRef() is None
        f = None

        f = lyr.GetNextFeature()
        assert f[other_field].endswith("_3")
        assert f["str3"] == "str3_3"
        assert f.GetGeometryRef() is not None

    check_values(lyr)

    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    lyr.ResetReading()
    check_values(lyr)

    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)

    check_values(lyr)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)

    if field_to_delete == 0:
        assert "<Name>str1</Name>" not in xml
        assert "<Name>str2</Name>" in xml
    else:
        assert "<Name>str1</Name>" in xml
        assert "<Name>str2</Name>" not in xml
    assert "<Name>str3</Name>" in xml

    ds = None


###############################################################################


def test_ogr_openfilegdb_write_delete_field_before_geom(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    with gdaltest.config_option("OPENFILEGDB_CREATE_FIELD_BEFORE_GEOMETRY", "YES"):
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    assert lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_before_geom", "to be deleted")
    f.SetField("str", "foo")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    assert (
        lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("field_before_geom"))
        == ogr.OGRERR_NONE
    )

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetField("str") == "foo"
    assert f.GetGeometryRef() is not None

    ds = None


###############################################################################


def test_ogr_openfilegdb_write_feature_dataset_no_crs(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["FEATURE_DATASET=my_feature_dataset"],
    )
    assert lyr is not None
    lyr = ds.CreateLayer(
        "test2",
        geom_type=ogr.wkbPoint,
        options=["FEATURE_DATASET=my_feature_dataset"],
    )
    assert lyr is not None
    ds = None

    ds = gdal.OpenEx(dirname)
    rg = ds.GetRootGroup()

    assert rg.GetGroupNames() == ["my_feature_dataset"]

    fd = rg.OpenGroup("my_feature_dataset")
    assert fd is not None
    assert fd.GetVectorLayerNames() == ["test", "test2"]

    lyr = ds.GetLayerByName("GDB_Items")
    assert lyr.GetFeatureCount() == 5  # == root, workspace, feature dataset, 2 layers

    lyr = ds.GetLayerByName("GDB_ItemRelationships")
    assert lyr.GetFeatureCount() == 3  # == feature dataset, 2 layers


###############################################################################


def test_ogr_openfilegdb_write_feature_dataset_crs(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        srs=srs,
        options=["FEATURE_DATASET=my_feature_dataset"],
    )
    assert lyr is not None

    lyr = ds.CreateLayer(
        "test2",
        geom_type=ogr.wkbPoint,
        srs=srs,
        options=["FEATURE_DATASET=my_feature_dataset"],
    )
    assert lyr is not None

    lyr = ds.CreateLayer(
        "inherited_srs",
        geom_type=ogr.wkbPoint,
        options=["FEATURE_DATASET=my_feature_dataset"],
    )
    assert lyr is not None

    other_srs = osr.SpatialReference()
    other_srs.ImportFromEPSG(4269)

    with gdal.quiet_errors():
        lyr = ds.CreateLayer(
            "other_srs",
            geom_type=ogr.wkbPoint,
            srs=other_srs,
            options=["FEATURE_DATASET=my_feature_dataset"],
        )
        assert lyr is None

    ds = None

    ds = gdal.OpenEx(dirname)
    lyr = ds.GetLayerByName("inherited_srs")
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"


###############################################################################


@pytest.mark.parametrize(
    "numPoints,maxFeaturesPerSpxPage",
    [
        (1, 2),  # depth 1
        (2, 2),
        (3, 2),  # depth 2
        (4, 2),
        (5, 2),
        (6, 2),  # depth 3
        (7, 2),
        (8, 2),
        (9, 2),
        (10, 2),
        (11, 2),
        (12, 2),
        (13, 2),
        (14, 2),
        (15, 2),  # depth 4
        (16, 2),
        (29, 2),
        (30, 2),
        (31, 2),  # depth 5 -> unsupported
        # With default value for maxFeaturesPerSpxPage (340)
        (339, None),  # depth 1
        (340, None),  # depth 1
        (341, None),  # depth 2
        #  (340*341, None), # depth 2   # a bit too slow for unit tests
    ],
)
def test_ogr_openfilegdb_write_spatial_index(
    tmp_vsimem, numPoints, maxFeaturesPerSpxPage
):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("points", geom_type=ogr.wkbPoint)
    for j in range(numPoints):
        feat = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt("POINT(%d %d)" % (j, j))
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
    with gdaltest.config_option(
        "OPENFILEGDB_MAX_FEATURES_PER_SPX_PAGE",
        str(maxFeaturesPerSpxPage) if maxFeaturesPerSpxPage else None,
    ):
        if maxFeaturesPerSpxPage == 2 and numPoints > 30:
            with gdal.quiet_errors():
                gdal.ErrorReset()
                lyr.SyncToDisk()
                assert gdal.GetLastErrorMsg() != ""
        else:
            gdal.ErrorReset()
            lyr.SyncToDisk()
            assert gdal.GetLastErrorMsg() == ""
    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    if numPoints > 1000:
        j = 0
        lyr.SetSpatialFilterRect(j - 0.1, j - 0.1, j + 0.1, j + 0.1)
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f is not None

        j = numPoints - 1
        lyr.SetSpatialFilterRect(j - 0.1, j - 0.1, j + 0.1, j + 0.1)
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f is not None
    else:
        for j in range(numPoints):
            lyr.SetSpatialFilterRect(j - 0.1, j - 0.1, j + 0.1, j + 0.1)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f is not None, j
    ds = None


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_openfilegdb_write_attribute_index(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    fld_defn = ogr.FieldDefn("int16", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("int32", ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("float32", ogr.OFTReal)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("lower_str", ogr.OFTString)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["int16"] = -1234
    f["int32"] = -12346789
    f["float32"] = 1.25
    f["float64"] = 1.256789
    f["str"] = "my str"
    f["lower_str"] = "MY STR"
    f["dt"] = "2022-06-03T16:06:00Z"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "x" * 100
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = ("x" * 100) + "y"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    # Errors of index creation
    with pytest.raises(
        Exception, match="Invalid index name: cannot be greater than 16 characters"
    ):
        ds.ExecuteSQL("CREATE INDEX this_name_is_wayyyyy_tooo_long ON test(int16)")

    with pytest.raises(Exception, match="Invalid layer name: non_existing_layer"):
        ds.ExecuteSQL("CREATE INDEX idx_int16 ON non_existing_layer(int16)")

    with pytest.raises(Exception, match="Cannot find field invalid_field"):
        ds.ExecuteSQL("CREATE INDEX invalid_field ON test(invalid_field)")

    with pytest.raises(
        Exception, match="Invalid index name: must not be a reserved keyword"
    ):
        ds.ExecuteSQL("CREATE INDEX SELECT ON test(int16)")

    with pytest.raises(Exception, match="Invalid index name: must start with a letter"):
        ds.ExecuteSQL("CREATE INDEX _starting_by_ ON test(int16)")

    with pytest.raises(
        Exception,
        match="Invalid index name: must contain only alpha numeric character or _",
    ):
        ds.ExecuteSQL("CREATE INDEX a&b ON test(int16)")

    with pytest.raises(
        Exception, match="Creation of multiple-column indices is not supported"
    ):
        ds.ExecuteSQL("CREATE INDEX index_on_two_cols ON test(int16, int32)")

    # Create indexes
    gdal.ErrorReset()
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        fld_name = lyr.GetLayerDefn().GetFieldDefn(i).GetName()
        if fld_name == "lower_str":
            ds.ExecuteSQL(
                "CREATE INDEX idx_%s ON test(LOWER(%s))" % (fld_name, fld_name)
            )
        else:
            ds.ExecuteSQL("CREATE INDEX idx_%s ON test(%s)" % (fld_name, fld_name))
        assert gdal.GetLastErrorMsg() == ""
        assert gdal.VSIStatL(dirname / f"a00000009.idx_{fld_name}.atx") is not None

    fld_defn = ogr.FieldDefn("unindexed", ogr.OFTString)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    with pytest.raises(Exception, match="An index with same name already exists"):
        # Re-using an index name
        ds.ExecuteSQL("CREATE INDEX idx_int16 ON test(unindexed)")

    with pytest.raises(Exception, match="Field int16 has already a registered index"):
        # Trying to index twice a field
        ds.ExecuteSQL("CREATE INDEX int16_again ON test(int16)")

    with pytest.raises(
        Exception, match="Field lower_str has already a registered index"
    ):
        ds.ExecuteSQL("CREATE INDEX lower_str_again ON test(lower_str)")

    ds = None

    def check_index_fully_used(ds, lyr):
        sql_lyr = ds.ExecuteSQL("GetLayerAttrIndexUse " + lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert attr_index_use == 2  # IteratorSufficientToEvaluateFilter

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    lyr.SetAttributeFilter("int16 = -1234")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("int16 = 1234")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("int32 = -12346789")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("int32 = 12346789")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("float32 = 1.25")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("float32 = -1.25")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("float64 = 1.256789")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("float64 = -1.256789")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("str = 'my str'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("str = 'MY STR'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("str = 'my st'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("str = 'my str2'")
    assert lyr.GetFeatureCount() == 0

    # Test truncation to 80 characters
    # lyr.SetAttributeFilter("str = '%s'" % ('x' * 100))
    # assert lyr.GetFeatureCount() == 1

    # lyr.SetAttributeFilter("str = '%s'" % ('x' * 100 + 'y'))
    # assert lyr.GetFeatureCount() == 1

    # lyr.SetAttributeFilter("str = '%s'" % ('x' * 100 + 'z'))
    # assert lyr.GetFeatureCount() == 0

    # Actually should be "LOWER(lower_str) = 'my str'" ...
    # so this test may break if we implement this in a cleaner way
    lyr.SetAttributeFilter("lower_str = 'my str'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt = '2022/06/03 16:06:00Z'")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    # Check that .gdbindexes is properly updated on field renaming
    fld_defn = ogr.FieldDefn("int32_renamed", ogr.OFTInteger)
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("int32"), fld_defn, ogr.ALTER_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )

    lyr.SetAttributeFilter("int32_renamed = -12346789")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    lyr.SetAttributeFilter("int32_renamed = -12346789")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    # Check that the index is destroy on field deletion
    assert gdal.VSIStatL(dirname / "a00000009.idx_int32.atx") is not None
    assert (
        lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("int32_renamed"))
        == ogr.OGRERR_NONE
    )
    assert gdal.VSIStatL(dirname / "a00000009.idx_int32.atx") is None

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    lyr.SetAttributeFilter("int16 = -1234")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("float32 = 1.25")
    check_index_fully_used(ds, lyr)
    assert lyr.GetFeatureCount() == 1

    ds = None


###############################################################################


def test_ogr_openfilegdb_write_delete_layer(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds.CreateLayer("test2", geom_type=ogr.wkbPoint)
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.TestCapability(ogr.ODsCDeleteLayer) == 1

    lyr = ds.GetLayerByName("GDB_SystemCatalog")
    assert lyr.GetFeatureCount() == 10  # 8 system tables + 2 layers

    lyr = ds.GetLayerByName("GDB_Items")
    assert lyr.GetFeatureCount() == 4  # root, workspace + 2 layers

    lyr = ds.GetLayerByName("GDB_ItemRelationships")
    assert lyr.GetFeatureCount() == 2  # 2 layers

    ds.ExecuteSQL("DELLAYER:test")
    assert ds.GetLayerCount() == 1

    for filename in gdal.ReadDir(dirname):
        assert not filename.startswith("a00000009.gdbtable")

    assert ds.DeleteLayer(-1) != ogr.OGRERR_NONE
    assert ds.DeleteLayer(1) != ogr.OGRERR_NONE

    # The following should not work
    with gdal.quiet_errors():
        gdal.ErrorReset()
        ds.ExecuteSQL("DELLAYER:not_existing")
        assert gdal.GetLastErrorMsg() != ""
    with gdal.quiet_errors():
        gdal.ErrorReset()
        ds.ExecuteSQL("DELLAYER:GDB_SystemCatalog")
        assert gdal.GetLastErrorMsg() != ""

    ds = None

    ds = ogr.Open(dirname)
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == "test2"

    lyr = ds.GetLayerByName("GDB_SystemCatalog")
    assert lyr.GetFeatureCount() == 9

    lyr = ds.GetLayerByName("GDB_Items")
    assert lyr.GetFeatureCount() == 3

    lyr = ds.GetLayerByName("GDB_ItemRelationships")
    assert lyr.GetFeatureCount() == 1


###############################################################################


def _check_freelist_consistency(ds, lyr):

    sql_lyr = ds.ExecuteSQL("CHECK_FREELIST_CONSISTENCY:" + lyr.GetName())
    f = sql_lyr.GetNextFeature()
    res = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    assert res == "1"


###############################################################################


def test_ogr_openfilegdb_write_freelist(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"
    table_filename = dirname / "a00000009.gdbtable"
    freelist_filename = dirname / "a00000009.freelist"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X" * 5)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.SyncToDisk()
    filesize = gdal.VSIStatL(table_filename).size

    assert lyr.DeleteFeature(1) == 0

    assert gdal.VSIStatL(freelist_filename) is not None
    _check_freelist_consistency(ds, lyr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * 5)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert filesize == gdal.VSIStatL(table_filename).size

    f = lyr.GetNextFeature()
    assert f["str"] == "Y" * 5

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X" * 6)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    to_delete = [f.GetFID()]

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X" * 6)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    to_delete.append(f.GetFID())

    filesize = gdal.VSIStatL(table_filename).size

    for fid in to_delete:
        assert lyr.DeleteFeature(fid) == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * 6)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * 6)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert filesize == gdal.VSIStatL(table_filename).size

    assert gdal.VSIStatL(freelist_filename) is not None
    _check_freelist_consistency(ds, lyr)

    lyr.SyncToDisk()
    assert gdal.VSIStatL(freelist_filename) is None


###############################################################################


def test_ogr_openfilegdb_write_freelist_not_exactly_matching_sizes(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"
    table_filename = dirname / "a00000009.gdbtable"
    freelist_filename = dirname / "a00000009.freelist"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X" * 500)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X" * 502)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.SyncToDisk()
    filesize = gdal.VSIStatL(table_filename).size

    assert lyr.DeleteFeature(1) == 0
    assert lyr.DeleteFeature(2) == 0

    assert gdal.VSIStatL(freelist_filename) is not None
    _check_freelist_consistency(ds, lyr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * 490)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * 501)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = lyr.GetNextFeature()
    assert f["str"] == "Y" * 490

    f = lyr.GetNextFeature()
    assert f["str"] == "Y" * 501

    assert filesize == gdal.VSIStatL(table_filename).size
    _check_freelist_consistency(ds, lyr)


###############################################################################


def test_ogr_openfilegdb_write_freelist_scenario_two_sizes(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"
    table_filename = dirname / "a00000009.gdbtable"
    freelist_filename = dirname / "a00000009.freelist"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    NFEATURES = 400

    # 500 and 600 are in the [440, 772[ range of the freelist Fibonacci suite
    SIZE1 = 600
    SIZE2 = 500
    assert SIZE2 < SIZE1

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE1
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE2
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE1
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE2
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.SyncToDisk()
    filesize = gdal.VSIStatL(table_filename).size

    for i in range(NFEATURES * 4):
        assert lyr.DeleteFeature(1 + i) == ogr.OGRERR_NONE

    _check_freelist_consistency(ds, lyr)

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE1
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE2
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE1
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    for i in range(NFEATURES):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * SIZE2
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert filesize == gdal.VSIStatL(table_filename).size

    assert gdal.VSIStatL(freelist_filename) is not None
    _check_freelist_consistency(ds, lyr)
    lyr.SyncToDisk()
    assert gdal.VSIStatL(freelist_filename) is None


###############################################################################


def test_ogr_openfilegdb_write_freelist_scenario_random(tmp_vsimem):

    import functools
    import random

    r = random.Random(0)

    dirname = tmp_vsimem / "out.gdb"
    table_filename = dirname / "a00000009.gdbtable"
    freelist_filename = dirname / "a00000009.freelist"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    assert ds is not None
    ds = ogr.Open(dirname, update=1)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    NFEATURES = 1000

    sizes = []
    fids = []
    # Ranges that are used to allocate a slot in a series of page
    fibo_suite = functools.reduce(
        lambda x, _: x + [x[-1] + x[-2]], range(20 - 2), [8, 16]
    )

    # Create features of random sizes
    for i in range(NFEATURES):
        series = r.randint(0, len(fibo_suite) - 2)
        size = r.randint(fibo_suite[series], fibo_suite[series + 1] - 1)
        sizes.append(size)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * size
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        fids.append(f.GetFID())

    # Delete them in random order
    for i in range(NFEATURES):
        idx = r.randint(0, len(fids) - 1)
        fid = fids[idx]
        del fids[idx]

        assert lyr.DeleteFeature(fid) == ogr.OGRERR_NONE

    _check_freelist_consistency(ds, lyr)
    lyr.SyncToDisk()
    filesize = gdal.VSIStatL(table_filename).size

    # Re-create feature of the same previous sizes, in random order
    for i in range(NFEATURES):
        idx = r.randint(0, len(sizes) - 1)
        size = sizes[idx]
        del sizes[idx]

        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "x" * size
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert filesize == gdal.VSIStatL(table_filename).size

    assert gdal.VSIStatL(freelist_filename) is not None
    _check_freelist_consistency(ds, lyr)
    lyr.SyncToDisk()
    assert gdal.VSIStatL(freelist_filename) is None


###############################################################################


def test_ogr_openfilegdb_write_freelist_scenario_issue_7504(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    N = 173

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "a" * N
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "b"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "c"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    # Length is > N: feature is rewritten at end of file
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["str"] = "d" * (N + 1)
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    # Before bugfix #7504, the space initially taken by feature 1 before
    # its edition would have been reused for feature 3, consequently
    # overwriting the first few bytes of feature 2...
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f["str"] = "e" * (N + 3)  # must not be greater than N+3 to test the bug
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    f = lyr.GetFeature(1)
    assert f["str"] == "d" * (N + 1)

    f = lyr.GetFeature(2)
    assert f["str"] == "b"

    f = lyr.GetFeature(3)
    assert f["str"] == "e" * (N + 3)

    ds = None


###############################################################################


def test_ogr_openfilegdb_write_repack(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"
    table_filename = dirname / "a00000009.gdbtable"
    freelist_filename = dirname / "a00000009.freelist"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "1" * 10)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "2" * 10)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "3" * 10)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.SyncToDisk()
    filesize = gdal.VSIStatL(table_filename).size

    with gdal.quiet_errors():
        assert ds.ExecuteSQL("REPACK unexisting_table") is None

    # Repack: nothing to do
    sql_lyr = ds.ExecuteSQL("REPACK")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    assert f[0] == "true"
    ds.ReleaseResultSet(sql_lyr)

    assert filesize == gdal.VSIStatL(table_filename).size

    # Suppress last feature
    assert lyr.DeleteFeature(3) == 0

    # Repack: truncate file
    sql_lyr = ds.ExecuteSQL("REPACK test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    assert f[0] == "true"
    ds.ReleaseResultSet(sql_lyr)

    assert gdal.VSIStatL(table_filename).size < filesize
    filesize = gdal.VSIStatL(table_filename).size

    # Suppress first feature
    assert lyr.DeleteFeature(1) == 0

    assert gdal.VSIStatL(freelist_filename) is not None

    # Repack: rewrite whole file
    sql_lyr = ds.ExecuteSQL("REPACK")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    assert f[0] == "true"
    ds.ReleaseResultSet(sql_lyr)

    assert gdal.VSIStatL(table_filename).size < filesize
    filesize = gdal.VSIStatL(table_filename).size

    assert gdal.VSIStatL(freelist_filename) is None

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetField(0) == "2" * 10

    # Repack: nothing to do
    sql_lyr = ds.ExecuteSQL("REPACK")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    assert f[0] == "true"
    ds.ReleaseResultSet(sql_lyr)

    assert gdal.VSIStatL(table_filename).size == filesize


###############################################################################


def test_ogr_openfilegdb_write_recompute_extent_on(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (5 6)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert lyr.GetExtent() == (1, 5, 2, 6)

    assert lyr.DeleteFeature(1) == ogr.OGRERR_NONE

    assert lyr.GetExtent() == (1, 5, 2, 6)

    gdal.ErrorReset()
    assert ds.ExecuteSQL("RECOMPUTE EXTENT ON test") is None
    assert gdal.GetLastErrorMsg() == ""

    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert ds.ExecuteSQL("RECOMPUTE EXTENT ON non_existing_layer") is None
        assert gdal.GetLastErrorMsg() != ""

    assert lyr.GetExtent() == (3, 5, 4, 6)

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetExtent() == (3, 5, 4, 6)

    assert lyr.DeleteFeature(2) == ogr.OGRERR_NONE
    assert lyr.DeleteFeature(3) == ogr.OGRERR_NONE

    assert ds.ExecuteSQL("RECOMPUTE EXTENT ON test") is None

    assert lyr.GetExtent(can_return_null=True) is None

    ds = None


###############################################################################


def test_ogr_openfilegdb_write_alter_field_defn(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert (
        lyr.CreateField(ogr.FieldDefn("other_field", ogr.OFTString)) == ogr.OGRERR_NONE
    )

    # No-op
    assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE

    # Invalid index
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(-1, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        assert (
            lyr.AlterFieldDefn(
                lyr.GetLayerDefn().GetFieldCount(), fld_defn, ogr.ALTER_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    # Changing type not supported
    fld_defn = ogr.FieldDefn("str", ogr.OFTInteger)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetType() == ogr.OFTString

    # Changing subtype not supported
    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTUUID)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetType() == ogr.OFTString
        assert fld_defn.GetSubType() == ogr.OFSTNone

    # Changing nullable state not supported
    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    fld_defn.SetNullable(False)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.IsNullable()

    # Renaming to an other existing field not supported
    fld_defn = ogr.FieldDefn("other_field", ogr.OFTString)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetName() == "str"

    fld_defn = ogr.FieldDefn("SHAPE", ogr.OFTString)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetName() == "str"

    fld_defn = ogr.FieldDefn("str_renamed", ogr.OFTString)
    fld_defn.SetAlternativeName("alias")
    fld_defn.SetWidth(10)
    fld_defn.SetDefault("'aaa'")

    assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE

    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetType() == ogr.OFTString
    assert fld_defn.GetName() == "str_renamed"
    assert fld_defn.GetAlternativeName() == "alias"
    assert fld_defn.GetWidth() == 10
    assert fld_defn.GetDefault() == "'aaa'"

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetType() == ogr.OFTString
    assert fld_defn.GetName() == "str_renamed"
    assert fld_defn.GetAlternativeName() == "alias"
    assert fld_defn.GetWidth() == 10
    assert fld_defn.GetDefault() == "'aaa'"

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<Name>str_renamed</Name>" in xml

    ds = None


###############################################################################
# Test writing field domains


@gdaltest.enable_exceptions()
def test_ogr_openfilegdb_write_domains(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = gdal.GetDriverByName("OpenFileGDB").Create(dirname, 0, 0, 0, gdal.GDT_Unknown)

    domain = ogr.CreateCodedFieldDomain(
        "domain", "desc", ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None}
    )
    assert ds.AddFieldDomain(domain)

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)

    fld_defn = ogr.FieldDefn("foo", ogr.OFTInteger)
    fld_defn.SetDomainName("domain")
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    fld_defn = ogr.FieldDefn("int_range", ogr.OFTInteger)
    fld_defn.SetDomainName("int_range_domain")
    domain = ogr.CreateRangeFieldDomain(
        "int_range_domain",
        "int_range_desc",
        ogr.OFTInteger,
        ogr.OFSTNone,
        1,
        True,
        2,
        True,
    )
    assert ds.AddFieldDomain(domain)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    fld_defn = ogr.FieldDefn("real_range", ogr.OFTReal)
    fld_defn.SetDomainName("real_range_domain")
    domain = ogr.CreateRangeFieldDomain(
        "real_range_domain", "desc", ogr.OFTReal, ogr.OFSTNone, 1.5, True, 2.5, True
    )
    assert ds.AddFieldDomain(domain)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    domain = ogr.CreateRangeFieldDomain(
        "int_range_without_bounds",
        "desc",
        ogr.OFTInteger,
        ogr.OFSTNone,
        None,
        False,
        None,
        False,
    )
    with pytest.raises(
        Exception,
        match="FileGeoDatabase requires that both minimum and maximum values of a range field domain are set",
    ):
        ds.AddFieldDomain(domain)

    domain = ogr.CreateRangeFieldDomainDateTime(
        "datetime_range",
        "datetime_range_desc",
        "2023-07-03T12:13:14",
        True,
        "2023-07-03T12:13:15",
        True,
    )
    assert ds.AddFieldDomain(domain)
    ds = None

    ds = gdal.OpenEx(dirname)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 4

    domain = ds.GetFieldDomain("int_range_domain")
    assert domain is not None
    assert domain.GetName() == "int_range_domain"
    assert domain.GetDescription() == "int_range_desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger
    assert domain.GetFieldSubType() == ogr.OFSTNone
    assert domain.GetMinAsDouble() == 1
    assert domain.GetMaxAsDouble() == 2

    domain = ds.GetFieldDomain("real_range_domain")
    assert domain is not None
    assert domain.GetName() == "real_range_domain"
    assert domain.GetDescription() == "desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetFieldSubType() == ogr.OFSTNone
    assert domain.GetMinAsDouble() == 1.5
    assert domain.GetMaxAsDouble() == 2.5

    domain = ds.GetFieldDomain("datetime_range")
    assert domain is not None
    assert domain.GetName() == "datetime_range"
    assert domain.GetDescription() == "datetime_range_desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTDateTime
    assert domain.GetFieldSubType() == ogr.OFSTNone
    assert domain.GetMinAsString() == "2023-07-03T12:13:14"
    assert domain.GetMaxAsString() == "2023-07-03T12:13:15"

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 3
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 2
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 1
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    fld_defn = ogr.FieldDefn("foo", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 1
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    fld_defn = ogr.FieldDefn("foo", ogr.OFTInteger)
    fld_defn.SetDomainName("domain")
    assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "domain"
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerByName("GDB_ItemRelationships").GetFeatureCount() == 2
    ds = None


###############################################################################
# Test writing relationships


def test_ogr_openfilegdb_write_relationships(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = gdal.GetDriverByName("OpenFileGDB").Create(dirname, 0, 0, 0, gdal.GDT_Unknown)

    relationship = gdal.Relationship(
        "my_relationship", "origin_table", "dest_table", gdal.GRC_ONE_TO_ONE
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("media")

    # no tables yet
    assert not ds.AddRelationship(relationship)

    lyr = ds.CreateLayer("origin_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    ds = gdal.OpenEx(dirname, gdal.GA_Update)

    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(1)
    assert f["Path"] == "\\"
    root_dataset_uuid = f["UUID"]

    f = items_lyr.GetFeature(3)
    assert f["Name"] == "origin_table"
    origin_table_uuid = f["UUID"]

    f = items_lyr.GetFeature(4)
    assert f["Name"] == "dest_table"
    dest_table_uuid = f["UUID"]

    ds = gdal.OpenEx(dirname, gdal.GA_Update)

    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {"my_relationship"}
    retrieved_rel = ds.GetRelationship("my_relationship")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_ONE
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "media"

    # check metadata contents
    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(5)
    relationship_uuid = f["UUID"]
    assert f["Name"] == "my_relationship"
    assert (
        f["Definition"]
        == """<DERelationshipClassInfo xsi:type="typens:DERelationshipClassInfo" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:typens="http://www.esri.com/schemas/ArcGIS/10.1">
  <CatalogPath>\\my_relationship</CatalogPath>
  <Name>my_relationship</Name>
  <ChildrenExpanded>false</ChildrenExpanded>
  <DatasetType>esriDTRelationshipClass</DatasetType>
  <DSID>5</DSID>
  <Versioned>false</Versioned>
  <CanVersion>false</CanVersion>
  <ConfigurationKeyword></ConfigurationKeyword>
  <RequiredGeodatabaseClientVersion>10.0</RequiredGeodatabaseClientVersion>
  <HasOID>false</HasOID>
  <GPFieldInfoExs xsi:type="typens:ArrayOfGPFieldInfoEx" />
  <OIDFieldName></OIDFieldName>
  <CLSID></CLSID>
  <EXTCLSID></EXTCLSID>
  <RelationshipClassNames xsi:type="typens:Names" />
  <AliasName></AliasName>
  <ModelName></ModelName>
  <HasGlobalID>false</HasGlobalID>
  <GlobalIDFieldName></GlobalIDFieldName>
  <RasterFieldName></RasterFieldName>
  <ExtensionProperties xsi:type="typens:PropertySet">
    <PropertyArray xsi:type="typens:ArrayOfPropertySetProperty" />
  </ExtensionProperties>
  <ControllerMemberships xsi:type="typens:ArrayOfControllerMembership" />
  <EditorTrackingEnabled>false</EditorTrackingEnabled>
  <CreatorFieldName></CreatorFieldName>
  <CreatedAtFieldName></CreatedAtFieldName>
  <EditorFieldName></EditorFieldName>
  <EditedAtFieldName></EditedAtFieldName>
  <IsTimeInUTC>true</IsTimeInUTC>
  <Cardinality>esriRelCardinalityOneToOne</Cardinality>
  <Notification>esriRelNotificationNone</Notification>
  <IsAttributed>false</IsAttributed>
  <IsComposite>false</IsComposite>
  <OriginClassNames xsi:type="typens:Names">
    <Name>origin_table</Name>
  </OriginClassNames>
  <DestinationClassNames xsi:type="typens:Names">
    <Name>dest_table</Name>
  </DestinationClassNames>
  <KeyType>esriRelKeyTypeSingle</KeyType>
  <ClassKey>esriRelClassKeyUndefined</ClassKey>
  <ForwardPathLabel></ForwardPathLabel>
  <BackwardPathLabel></BackwardPathLabel>
  <IsReflexive>false</IsReflexive>
  <OriginClassKeys xsi:type="typens:ArrayOfRelationshipClassKey">
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>o_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginPrimary</KeyRole>
    </RelationshipClassKey>
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>dest_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginForeign</KeyRole>
    </RelationshipClassKey>
  </OriginClassKeys>
  <RelationshipRules xsi:type="typens:ArrayOfRelationshipRule" />
  <IsAttachmentRelationship>true</IsAttachmentRelationship>
  <ChangeTracked>false</ChangeTracked>
  <ReplicaTracked>false</ReplicaTracked>
</DERelationshipClassInfo>\n"""
    )
    assert f["DatasetSubtype1"] == 1
    assert f["DatasetSubtype2"] == 0
    assert f["Documentation"] == """<metadata xml:lang="en">
  <Esri>
    <CreaDate></CreaDate>
    <CreaTime></CreaTime>
    <ArcGISFormat>1.0</ArcGISFormat>
    <SyncOnce>TRUE</SyncOnce>
    <DataProperties>
      <lineage />
    </DataProperties>
  </Esri>
</metadata>
"""
    assert f["ItemInfo"] == """<ESRI_ItemInformation culture="">
  <name>my_relationship</name>
  <catalogPath>\\my_relationship</catalogPath>
  <snippet></snippet>
  <description></description>
  <summary></summary>
  <title>my_relationship</title>
  <tags></tags>
  <type>File Geodatabase Relationship Class</type>
  <typeKeywords>
    <typekeyword>Data</typekeyword>
    <typekeyword>Dataset</typekeyword>
    <typekeyword>Vector Data</typekeyword>
    <typekeyword>Feature Data</typekeyword>
    <typekeyword>File Geodatabase</typekeyword>
    <typekeyword>GDB</typekeyword>
    <typekeyword>Relationship Class</typekeyword>
  </typeKeywords>
  <url></url>
  <datalastModifiedTime></datalastModifiedTime>
  <extent>
    <xmin></xmin>
    <ymin></ymin>
    <xmax></xmax>
    <ymax></ymax>
  </extent>
  <minScale>0</minScale>
  <maxScale>0</maxScale>
  <spatialReference></spatialReference>
  <accessInformation></accessInformation>
  <licenseInfo></licenseInfo>
  <typeID>fgdb_relationship</typeID>
  <isContainer>false</isContainer>
  <browseDialogOnly>false</browseDialogOnly>
  <propNames></propNames>
  <propValues></propValues>
</ESRI_ItemInformation>
"""
    # check item relationships have been created
    item_relationships_lyr = ds.GetLayerByName("GDB_ItemRelationships")

    f = item_relationships_lyr.GetFeature(3)
    assert f["OriginID"] == origin_table_uuid
    assert f["DestID"] == relationship_uuid
    assert f["Type"] == "{725BADAB-3452-491B-A795-55F32D67229C}"

    f = item_relationships_lyr.GetFeature(4)
    assert f["OriginID"] == dest_table_uuid
    assert f["DestID"] == relationship_uuid
    assert f["Type"] == "{725BADAB-3452-491B-A795-55F32D67229C}"

    f = item_relationships_lyr.GetFeature(5)
    assert f["OriginID"] == root_dataset_uuid
    assert f["DestID"] == relationship_uuid
    assert f["Type"] == "{DC78F1AB-34E4-43AC-BA47-1C4EABD0E7C7}"

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {"my_relationship"}

    # one to many
    lyr = ds.CreateLayer("origin_table_1_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table_1_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    ds = gdal.OpenEx(dirname, gdal.GA_Update)

    # should be rejected -- duplicate name
    assert not ds.AddRelationship(relationship)

    relationship = gdal.Relationship(
        "my_one_to_many_relationship",
        "origin_table_1_to_many",
        "dest_table_1_to_many",
        gdal.GRC_ONE_TO_MANY,
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetType(gdal.GRT_COMPOSITE)
    relationship.SetForwardPathLabel("fwd label")
    relationship.SetBackwardPathLabel("backward label")
    assert ds.AddRelationship(relationship)

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
    }
    retrieved_rel = ds.GetRelationship("my_one_to_many_relationship")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_COMPOSITE
    assert retrieved_rel.GetLeftTableName() == "origin_table_1_to_many"
    assert retrieved_rel.GetRightTableName() == "dest_table_1_to_many"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetForwardPathLabel() == "fwd label"
    assert retrieved_rel.GetBackwardPathLabel() == "backward label"
    assert retrieved_rel.GetRelatedTableType() == "features"

    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(8)
    assert f["Name"] == "my_one_to_many_relationship"
    assert (
        f["Definition"]
        == """<DERelationshipClassInfo xsi:type="typens:DERelationshipClassInfo" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:typens="http://www.esri.com/schemas/ArcGIS/10.1">
  <CatalogPath>\\my_one_to_many_relationship</CatalogPath>
  <Name>my_one_to_many_relationship</Name>
  <ChildrenExpanded>false</ChildrenExpanded>
  <DatasetType>esriDTRelationshipClass</DatasetType>
  <DSID>8</DSID>
  <Versioned>false</Versioned>
  <CanVersion>false</CanVersion>
  <ConfigurationKeyword></ConfigurationKeyword>
  <RequiredGeodatabaseClientVersion>10.0</RequiredGeodatabaseClientVersion>
  <HasOID>false</HasOID>
  <GPFieldInfoExs xsi:type="typens:ArrayOfGPFieldInfoEx" />
  <OIDFieldName></OIDFieldName>
  <CLSID></CLSID>
  <EXTCLSID></EXTCLSID>
  <RelationshipClassNames xsi:type="typens:Names" />
  <AliasName></AliasName>
  <ModelName></ModelName>
  <HasGlobalID>false</HasGlobalID>
  <GlobalIDFieldName></GlobalIDFieldName>
  <RasterFieldName></RasterFieldName>
  <ExtensionProperties xsi:type="typens:PropertySet">
    <PropertyArray xsi:type="typens:ArrayOfPropertySetProperty" />
  </ExtensionProperties>
  <ControllerMemberships xsi:type="typens:ArrayOfControllerMembership" />
  <EditorTrackingEnabled>false</EditorTrackingEnabled>
  <CreatorFieldName></CreatorFieldName>
  <CreatedAtFieldName></CreatedAtFieldName>
  <EditorFieldName></EditorFieldName>
  <EditedAtFieldName></EditedAtFieldName>
  <IsTimeInUTC>true</IsTimeInUTC>
  <Cardinality>esriRelCardinalityOneToMany</Cardinality>
  <Notification>esriRelNotificationNone</Notification>
  <IsAttributed>false</IsAttributed>
  <IsComposite>true</IsComposite>
  <OriginClassNames xsi:type="typens:Names">
    <Name>origin_table_1_to_many</Name>
  </OriginClassNames>
  <DestinationClassNames xsi:type="typens:Names">
    <Name>dest_table_1_to_many</Name>
  </DestinationClassNames>
  <KeyType>esriRelKeyTypeSingle</KeyType>
  <ClassKey>esriRelClassKeyUndefined</ClassKey>
  <ForwardPathLabel>fwd label</ForwardPathLabel>
  <BackwardPathLabel>backward label</BackwardPathLabel>
  <IsReflexive>false</IsReflexive>
  <OriginClassKeys xsi:type="typens:ArrayOfRelationshipClassKey">
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>o_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginPrimary</KeyRole>
    </RelationshipClassKey>
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>dest_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginForeign</KeyRole>
    </RelationshipClassKey>
  </OriginClassKeys>
  <RelationshipRules xsi:type="typens:ArrayOfRelationshipRule" />
  <IsAttachmentRelationship>false</IsAttachmentRelationship>
  <ChangeTracked>false</ChangeTracked>
  <ReplicaTracked>false</ReplicaTracked>
</DERelationshipClassInfo>\n"""
    )
    assert f["DatasetSubtype1"] == 2
    assert f["DatasetSubtype2"] == 0

    # many to many relationship
    lyr = ds.CreateLayer("origin_table_many_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table_many_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("mapping_table_many_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("many_to_many", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("RID", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("origin_fk", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("destination_fk", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    ds = gdal.OpenEx(dirname, gdal.GA_Update)

    relationship = gdal.Relationship(
        "many_to_many",
        "origin_table_many_to_many",
        "dest_table_many_to_many",
        gdal.GRC_MANY_TO_MANY,
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetMappingTableName("mapping_table_many_to_many")
    relationship.SetLeftMappingTableFields(["origin_fk"])
    relationship.SetRightMappingTableFields(["destination_fk"])

    # this should be rejected -- the mapping table name MUST match the relationship name
    assert not ds.AddRelationship(relationship)

    relationship.SetMappingTableName("many_to_many")
    assert ds.AddRelationship(relationship)

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
    }
    retrieved_rel = ds.GetRelationship("many_to_many")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table_many_to_many"
    assert retrieved_rel.GetRightTableName() == "dest_table_many_to_many"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetMappingTableName() == "many_to_many"
    assert retrieved_rel.GetLeftMappingTableFields() == ["origin_fk"]
    assert retrieved_rel.GetRightMappingTableFields() == ["destination_fk"]

    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(13)
    assert f["Name"] == "many_to_many"
    assert (
        f["Definition"]
        == """<DERelationshipClassInfo xsi:type="typens:DERelationshipClassInfo" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:typens="http://www.esri.com/schemas/ArcGIS/10.1">
  <CatalogPath>\\many_to_many</CatalogPath>
  <Name>many_to_many</Name>
  <ChildrenExpanded>false</ChildrenExpanded>
  <DatasetType>esriDTRelationshipClass</DatasetType>
  <DSID>13</DSID>
  <Versioned>false</Versioned>
  <CanVersion>false</CanVersion>
  <ConfigurationKeyword></ConfigurationKeyword>
  <RequiredGeodatabaseClientVersion>10.0</RequiredGeodatabaseClientVersion>
  <HasOID>false</HasOID>
  <GPFieldInfoExs xsi:type="typens:ArrayOfGPFieldInfoEx">
    <GPFieldInfoEx xsi:type="typens:GPFieldInfoEx">
      <Name>OBJECTID</Name>
    </GPFieldInfoEx>
    <GPFieldInfoEx xsi:type="typens:GPFieldInfoEx">
      <Name>origin_fk</Name>
    </GPFieldInfoEx>
    <GPFieldInfoEx xsi:type="typens:GPFieldInfoEx">
      <Name>destination_fk</Name>
    </GPFieldInfoEx>
  </GPFieldInfoExs>
  <OIDFieldName>OBJECTID</OIDFieldName>
  <CLSID></CLSID>
  <EXTCLSID></EXTCLSID>
  <RelationshipClassNames xsi:type="typens:Names" />
  <AliasName></AliasName>
  <ModelName></ModelName>
  <HasGlobalID>false</HasGlobalID>
  <GlobalIDFieldName></GlobalIDFieldName>
  <RasterFieldName></RasterFieldName>
  <ExtensionProperties xsi:type="typens:PropertySet">
    <PropertyArray xsi:type="typens:ArrayOfPropertySetProperty" />
  </ExtensionProperties>
  <ControllerMemberships xsi:type="typens:ArrayOfControllerMembership" />
  <EditorTrackingEnabled>false</EditorTrackingEnabled>
  <CreatorFieldName></CreatorFieldName>
  <CreatedAtFieldName></CreatedAtFieldName>
  <EditorFieldName></EditorFieldName>
  <EditedAtFieldName></EditedAtFieldName>
  <IsTimeInUTC>true</IsTimeInUTC>
  <Cardinality>esriRelCardinalityManyToMany</Cardinality>
  <Notification>esriRelNotificationNone</Notification>
  <IsAttributed>false</IsAttributed>
  <IsComposite>false</IsComposite>
  <OriginClassNames xsi:type="typens:Names">
    <Name>origin_table_many_to_many</Name>
  </OriginClassNames>
  <DestinationClassNames xsi:type="typens:Names">
    <Name>dest_table_many_to_many</Name>
  </DestinationClassNames>
  <KeyType>esriRelKeyTypeSingle</KeyType>
  <ClassKey>esriRelClassKeyUndefined</ClassKey>
  <ForwardPathLabel></ForwardPathLabel>
  <BackwardPathLabel></BackwardPathLabel>
  <IsReflexive>false</IsReflexive>
  <OriginClassKeys xsi:type="typens:ArrayOfRelationshipClassKey">
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>o_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginPrimary</KeyRole>
    </RelationshipClassKey>
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>origin_fk</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleOriginForeign</KeyRole>
    </RelationshipClassKey>
  </OriginClassKeys>
  <DestinationClassKeys xsi:type="typens:ArrayOfRelationshipClassKey">
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>dest_pkey</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleDestinationPrimary</KeyRole>
    </RelationshipClassKey>
    <RelationshipClassKey xsi:type="typens:RelationshipClassKey">
      <ObjectKeyName>destination_fk</ObjectKeyName>
      <ClassKeyName></ClassKeyName>
      <KeyRole>esriRelKeyRoleDestinationForeign</KeyRole>
    </RelationshipClassKey>
  </DestinationClassKeys>
  <RelationshipRules xsi:type="typens:ArrayOfRelationshipRule" />
  <IsAttachmentRelationship>false</IsAttachmentRelationship>
  <ChangeTracked>false</ChangeTracked>
  <ReplicaTracked>false</ReplicaTracked>
</DERelationshipClassInfo>\n"""
    )
    assert f["DatasetSubtype1"] == 3
    assert f["DatasetSubtype2"] == 0

    # many to many relationship, auto create mapping table
    lyr = ds.CreateLayer("origin_table_many_to_many2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table_many_to_many2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    ds = gdal.OpenEx(dirname, gdal.GA_Update)

    relationship = gdal.Relationship(
        "many_to_many_auto",
        "origin_table_many_to_many2",
        "dest_table_many_to_many2",
        gdal.GRC_MANY_TO_MANY,
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])

    assert ds.AddRelationship(relationship)

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
        "many_to_many_auto",
    }
    retrieved_rel = ds.GetRelationship("many_to_many_auto")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table_many_to_many2"
    assert retrieved_rel.GetRightTableName() == "dest_table_many_to_many2"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetMappingTableName() == "many_to_many_auto"
    assert retrieved_rel.GetLeftMappingTableFields() == ["origin_fk"]
    assert retrieved_rel.GetRightMappingTableFields() == ["destination_fk"]
    # make sure mapping table was created
    mapping_table = ds.GetLayerByName("many_to_many_auto")
    assert mapping_table is not None
    lyr_defn = mapping_table.GetLayerDefn()
    assert mapping_table.GetFIDColumn() == "RID"
    assert lyr_defn.GetFieldIndex("origin_fk") >= 0
    assert lyr_defn.GetFieldIndex("destination_fk") >= 0

    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(16)
    relationship_uuid = f["UUID"]
    assert f["Name"] == "many_to_many_auto"
    assert f["Type"] == "{B606A7E1-FA5B-439C-849C-6E9C2481537B}"

    # delete relationship
    assert not ds.DeleteRelationship("i dont exist")
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
        "many_to_many_auto",
    }

    assert ds.DeleteRelationship("many_to_many_auto")
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
    }
    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
    }

    # make sure we are correctly cleaned up
    items_lyr = ds.GetLayerByName("GDB_Items")
    for f in items_lyr:
        assert f["UUID"] != relationship_uuid

    # check item relationships have been created
    item_relationships_lyr = ds.GetLayerByName("GDB_ItemRelationships")
    for f in item_relationships_lyr:
        assert f["OriginID"] != relationship_uuid
        assert f["DestID"] != relationship_uuid

    # update relationship
    relationship = gdal.Relationship(
        "i dont exist",
        "origin_table_1_to_many",
        "dest_table_1_to_many",
        gdal.GRC_ONE_TO_MANY,
    )
    assert not ds.UpdateRelationship(relationship)

    relationship = gdal.Relationship(
        "my_one_to_many_relationship",
        "origin_table_1_to_many",
        "dest_table_1_to_many",
        gdal.GRC_ONE_TO_MANY,
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetType(gdal.GRT_COMPOSITE)
    relationship.SetForwardPathLabel("my new fwd label")
    relationship.SetBackwardPathLabel("my new backward label")
    assert ds.UpdateRelationship(relationship)

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
    }
    retrieved_rel = ds.GetRelationship("my_one_to_many_relationship")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_COMPOSITE
    assert retrieved_rel.GetLeftTableName() == "origin_table_1_to_many"
    assert retrieved_rel.GetRightTableName() == "dest_table_1_to_many"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetForwardPathLabel() == "my new fwd label"
    assert retrieved_rel.GetBackwardPathLabel() == "my new backward label"
    assert retrieved_rel.GetRelatedTableType() == "features"

    # change relationship tables
    lyr = ds.CreateLayer("new_origin_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("new_o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("new_dest_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("new_dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    relationship = gdal.Relationship(
        "my_one_to_many_relationship",
        "new_origin_table",
        "new_dest_table",
        gdal.GRC_ONE_TO_MANY,
    )
    relationship.SetLeftTableFields(["new_o_pkey"])
    relationship.SetRightTableFields(["new_dest_pkey"])
    assert ds.UpdateRelationship(relationship)

    ds = gdal.OpenEx(dirname, gdal.GA_Update)
    assert set(ds.GetRelationshipNames()) == {
        "my_relationship",
        "my_one_to_many_relationship",
        "many_to_many",
    }
    retrieved_rel = ds.GetRelationship("my_one_to_many_relationship")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "new_origin_table"
    assert retrieved_rel.GetRightTableName() == "new_dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["new_o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["new_dest_pkey"]

    # make sure GDB_ItemRelationships table has been updated
    items_lyr = ds.GetLayerByName("GDB_Items")
    f = items_lyr.GetFeature(8)
    relationship_uuid = f["UUID"]
    assert f["Name"] == "my_one_to_many_relationship"
    assert f["Type"] == "{B606A7E1-FA5B-439C-849C-6E9C2481537B}"

    f = items_lyr.GetFeature(18)
    assert f["Name"] == "new_origin_table"
    origin_table_uuid = f["UUID"]

    f = items_lyr.GetFeature(19)
    assert f["Name"] == "new_dest_table"
    dest_table_uuid = f["UUID"]

    item_relationships_lyr = ds.GetLayerByName("GDB_ItemRelationships")

    assert (
        len(
            [
                f
                for f in item_relationships_lyr
                if f["OriginID"] == origin_table_uuid
                and f["DestID"] == relationship_uuid
                and f["Type"] == "{725BADAB-3452-491B-A795-55F32D67229C}"
            ]
        )
        == 1
    )
    assert (
        len(
            [
                f
                for f in item_relationships_lyr
                if f["OriginID"] == dest_table_uuid
                and f["DestID"] == relationship_uuid
                and f["Type"] == "{725BADAB-3452-491B-A795-55F32D67229C}"
            ]
        )
        == 1
    )
    assert (
        len(
            [
                f
                for f in item_relationships_lyr
                if f["OriginID"] == root_dataset_uuid
                and f["DestID"] == relationship_uuid
                and f["Type"] == "{DC78F1AB-34E4-43AC-BA47-1C4EABD0E7C7}"
            ]
        )
        == 1
    )


###############################################################################
# Test emulated transactions


def test_ogr_openfilegdb_write_emulated_transactions(tmp_path):

    dirname = tmp_path / "test_ogr_openfilegdb_write_emulated_transactions.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    gdal.Mkdir(dirname / ".ogrtransaction_backup", 0o755)
    with gdal.quiet_errors():
        assert ds.StartTransaction(True) == ogr.OGRERR_FAILURE
    gdal.Rmdir(dirname / ".ogrtransaction_backup")

    assert ds.TestCapability(ogr.ODsCEmulatedTransactions)
    assert ds.StartTransaction(True) == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is not None

    assert ds.CommitTransaction() == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    with gdal.quiet_errors():
        assert ds.StartTransaction(True) != ogr.OGRERR_NONE
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    with gdal.quiet_errors():
        assert ds.CommitTransaction() != ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    with gdal.quiet_errors():
        assert ds.RollbackTransaction() != ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbNone)
    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is not None
    assert lyr is not None
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / ".ogrtransaction_backup") is None

    # It is in a ghost state after rollback
    assert lyr.GetFeatureCount() == 0

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE

    # Implicit rollback
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    gdal.Rmdir(dirname / ".ogrtransaction_backup")
    with gdal.quiet_errors():
        assert ds.RollbackTransaction() == ogr.OGRERR_FAILURE
        ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.TestCapability(ogr.ODsCEmulatedTransactions)
    assert ds.GetLayerCount() == 0
    assert gdal.VSIStatL(dirname / "a00000009.gdbtable") is None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE

    assert ds.CreateLayer("foo", geom_type=ogr.wkbNone) is not None
    assert gdal.VSIStatL(dirname / "a00000009.gdbtable") is not None

    assert ds.DeleteLayer(0) == ogr.OGRERR_NONE
    assert gdal.VSIStatL(dirname / "a00000009.gdbtable") is None

    assert ds.CreateLayer("foo2", geom_type=ogr.wkbNone) is not None
    assert gdal.VSIStatL(dirname / "a0000000a.gdbtable") is not None

    assert ds.CommitTransaction() == ogr.OGRERR_NONE

    assert gdal.VSIStatL(dirname / "a0000000a.gdbtable") is not None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    assert ds.DeleteLayer(0) == ogr.OGRERR_NONE
    assert gdal.VSIStatL(dirname / "a0000000a.gdbtable") is None
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE
    assert gdal.VSIStatL(dirname / "a0000000a.gdbtable") is not None
    ds = None

    gdal.Mkdir(dirname / ".ogrtransaction_backup", 0o755)
    with gdal.quiet_errors():
        # Cannot open in update mode with an existing backup directory
        assert ogr.Open(dirname, update=1) is None

        # Emit warning in read-only mode when opening with an existing backup directory
        gdal.ErrorReset()
        assert ogr.Open(dirname) is not None
        assert "A previous backup directory" in gdal.GetLastErrorMsg()
    gdal.Rmdir(dirname / ".ogrtransaction_backup")

    # Transaction not supported in read-only mode
    ds = ogr.Open(dirname)
    assert ds.TestCapability(ogr.ODsCEmulatedTransactions) == 0
    with gdal.quiet_errors():
        assert ds.StartTransaction(True) == ogr.OGRERR_FAILURE
    ds = None

    ds = ogr.Open(dirname, update=1)
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("foo2")

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    assert ds.CommitTransaction() == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayerByName("foo2")
    assert lyr.GetFeatureCount() == 1

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 2
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1

    # Test that StartTransaction() / RollbackTransaction() doesn't destroy
    # unmodified layers! (https://github.com/OSGeo/gdal/issues/5952)
    assert ds.StartTransaction(True) == ogr.OGRERR_NONE
    assert ds.RollbackTransaction() == ogr.OGRERR_NONE

    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayerByName("foo2")
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################


def test_ogr_openfilegdb_write_emulated_transactions_delete_field_before_geom(
    tmp_vsimem,
):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    with gdaltest.config_option("OPENFILEGDB_CREATE_FIELD_BEFORE_GEOMETRY", "YES"):
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    assert lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_before_geom", "to be deleted")
    f.SetField("str", "foo")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    assert ds.StartTransaction(True) == ogr.OGRERR_NONE

    assert (
        lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("field_before_geom"))
        == ogr.OGRERR_NONE
    )

    assert ds.RollbackTransaction() == ogr.OGRERR_NONE

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetField("field_before_geom") == "to be deleted"
    assert f.GetField("str") == "foo"
    assert f.GetGeometryRef() is not None

    ds = None


###############################################################################
# Test renaming a layer


@pytest.mark.parametrize("options", [[], ["FEATURE_DATASET=fd1"]])
def test_ogr_openfilegdb_write_rename_layer(tmp_path, options):

    dirname = tmp_path / "rename.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("other_layer", geom_type=ogr.wkbNone)
    lyr.SyncToDisk()

    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbPoint, options=options)
    assert lyr.TestCapability(ogr.OLCRename) == 1

    assert lyr.Rename("bar") == ogr.OGRERR_NONE
    assert lyr.GetDescription() == "bar"
    assert lyr.GetLayerDefn().GetName() == "bar"

    # Too long layer name
    with gdal.quiet_errors():
        assert lyr.Rename("x" * 200) != ogr.OGRERR_NONE

    with gdal.quiet_errors():
        assert lyr.Rename("bar") != ogr.OGRERR_NONE

    with gdal.quiet_errors():
        assert lyr.Rename("other_layer") != ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    ds = ogr.Open(dirname, update=1)

    # Check system tables
    system_catolog_lyr = ds.GetLayerByName("GDB_SystemCatalog")
    f = system_catolog_lyr.GetFeature(10)
    assert f["Name"] == "bar"

    items_lyr = ds.GetLayerByName("GDB_Items")
    if options == []:
        f = items_lyr.GetFeature(4)
        assert f["Path"] == "\\bar"
        assert "<CatalogPath>\\bar</CatalogPath>" in f["Definition"]
    else:
        f = items_lyr.GetFeature(5)
        assert f["Path"] == "\\fd1\\bar"
        assert "<CatalogPath>\\fd1\\bar</CatalogPath>" in f["Definition"]
    assert f["Name"] == "bar"
    assert f["PhysicalName"] == "BAR"
    assert "<Name>bar</Name>" in f["Definition"]

    # Second renaming, after dataset reopening
    lyr = ds.GetLayerByName("bar")
    assert lyr.Rename("baz") == ogr.OGRERR_NONE
    assert lyr.GetDescription() == "baz"
    assert lyr.GetLayerDefn().GetName() == "baz"

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

    ds = None

    ds = ogr.Open(dirname)

    # Check system tables
    system_catolog_lyr = ds.GetLayerByName("GDB_SystemCatalog")
    f = system_catolog_lyr.GetFeature(10)
    assert f["Name"] == "baz"

    items_lyr = ds.GetLayerByName("GDB_Items")
    if options == []:
        f = items_lyr.GetFeature(4)
        assert f["Path"] == "\\baz"
        assert "<CatalogPath>\\baz</CatalogPath>" in f["Definition"]
    else:
        f = items_lyr.GetFeature(5)
        assert f["Path"] == "\\fd1\\baz"
        assert "<CatalogPath>\\fd1\\baz</CatalogPath>" in f["Definition"]
    assert f["Name"] == "baz"
    assert f["PhysicalName"] == "BAZ"
    assert "<Name>baz</Name>" in f["Definition"]

    lyr = ds.GetLayerByName("baz")
    assert lyr is not None, [
        ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())
    ]

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

    ds = None


###############################################################################
# Test field name laundering (#4458)


def test_ogr_openfilegdb_field_name_laundering(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    with gdal.quiet_errors():
        lyr.CreateField(ogr.FieldDefn("FROM", ogr.OFTInteger))  # reserved keyword
        lyr.CreateField(
            ogr.FieldDefn("1NUMBER", ogr.OFTInteger)
        )  # starting with a number
        lyr.CreateField(
            ogr.FieldDefn("WITH SPACE AND !$*!- special characters", ogr.OFTInteger)
        )  # unallowed characters
        lyr.CreateField(ogr.FieldDefn("é" * 64, ogr.OFTInteger))  # OK
        lyr.CreateField(
            ogr.FieldDefn(
                "A123456789012345678901234567890123456789012345678901234567890123",
                ogr.OFTInteger,
            )
        )  # 64 characters : ok
        lyr.CreateField(
            ogr.FieldDefn(
                "A1234567890123456789012345678901234567890123456789012345678901234",
                ogr.OFTInteger,
            )
        )  # 65 characters : nok
        lyr.CreateField(
            ogr.FieldDefn(
                "A12345678901234567890123456789012345678901234567890123456789012345",
                ogr.OFTInteger,
            )
        )  # 66 characters : nok

    lyr_defn = lyr.GetLayerDefn()
    expected_names = [
        "FROM_",
        "_1NUMBER",
        "WITH_SPACE_AND_______special_characters",
        "é" * 64,
        "A123456789012345678901234567890123456789012345678901234567890123",
        "A1234567890123456789012345678901234567890123456789012345678901_1",
        "A1234567890123456789012345678901234567890123456789012345678901_2",
    ]
    for i in range(5):
        assert lyr_defn.GetFieldIndex(expected_names[i]) == i, (
            "did not find %s" % expected_names[i]
        )

    ds = None


###############################################################################
# Test layer name laundering (#4466)


def test_ogr_openfilegdb_layer_name_laundering(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    _160char = "A123456789" * 16

    in_names = [
        "FROM",  # reserved keyword
        "1NUMBER",  # starting with a number
        "WITH SPACE AND !$*!- special characters",  # banned characters
        "sde_foo",  # reserved prefixes
        _160char,  # OK
        _160char + "A",  # too long
        _160char + "B",  # still too long
    ]

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    with gdal.quiet_errors():
        for in_name in in_names:
            ds.CreateLayer(in_name, geom_type=ogr.wkbPoint)

    expected_names = [
        "FROM_",
        "_1NUMBER",
        "WITH_SPACE_AND_______special_characters",
        "_sde_foo",
        _160char,
        _160char[0:158] + "_1",
        _160char[0:158] + "_2",
    ]
    for i, exp_name in enumerate(expected_names):
        assert ds.GetLayerByIndex(i).GetName() == exp_name, "did not find %s" % exp_name

    ds = None


###############################################################################
# Test creating layer with documentation


def test_ogr_openfilegdb_layer_documentation(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["DOCUMENTATION=<my_doc/>"])
    ds = None

    ds = ogr.Open(dirname)
    sql_lyr = ds.ExecuteSQL("GetLayerMetadata test")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == "<my_doc/>"
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit(tmp_vsimem):

    dirname = (
        tmp_vsimem
        / "test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit.gdb"
    )

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer(
        "line",
        srs=srs,
        geom_type=ogr.wkbLineString,
        options=["CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(0 0,2 0)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("COMPOUNDCURVE((0 0,2 0))"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("MULTILINESTRING((0 0,2 0),(10 0,15 0))")
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("MULTICURVE((0 0,2 0),(10 0,15 0))")
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer(
        "area",
        srs=srs,
        geom_type=ogr.wkbPolygon,
        options=["CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "POLYGON((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2)),((10 0,10 1,11 1,11 0,10 0)))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "MULTISURFACE(((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2)),((10 0,10 1,11 1,11 0,10 0)))"
        )
    )
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(dirname, update=1)

    lyr = ds.GetLayerByName("line")
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex("Shape_Length") >= 0
    assert lyr_defn.GetFieldIndex("Shape_Area") < 0
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == 2
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == 2
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == 2 + 5
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == 2 + 5
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] is None

    lyr = ds.GetLayerByName("area")
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex("Shape_Length") >= 0
    assert lyr_defn.GetFieldIndex("Shape_Area") >= 0
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Area")).GetDefault()
        == "FILEGEODATABASE_SHAPE_AREA"
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == pytest.approx(6.4)
    assert f["Shape_Area"] == pytest.approx(0.64)
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == pytest.approx(6.4)
    assert f["Shape_Area"] == pytest.approx(0.64)
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == pytest.approx(6.4 + 4)
    assert f["Shape_Area"] == pytest.approx(0.64 + 1)
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] == pytest.approx(6.4 + 4)
    assert f["Shape_Area"] == pytest.approx(0.64 + 1)
    f = lyr.GetNextFeature()
    assert f["Shape_Length"] is None
    assert f["Shape_Area"] is None

    # Rename Shape_Length and Shape_Area fields (not sure the FileGDB SDK likes it)
    iShapeLength = lyr_defn.GetFieldIndex("Shape_Length")
    fld_defn = ogr.FieldDefn("Shape_Length_renamed", ogr.OFTReal)
    assert (
        lyr.AlterFieldDefn(iShapeLength, fld_defn, ogr.ALTER_NAME_FLAG)
        == ogr.OGRERR_NONE
    )

    iShapeArea = lyr_defn.GetFieldIndex("Shape_Area")
    fld_defn = ogr.FieldDefn("Shape_Area_renamed", ogr.OFTReal)
    assert (
        lyr.AlterFieldDefn(iShapeArea, fld_defn, ogr.ALTER_NAME_FLAG) == ogr.OGRERR_NONE
    )

    ds = ogr.Open(dirname, update=1)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition area")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<AreaFieldName>Shape_Area_renamed</AreaFieldName>" in xml
    assert "<LengthFieldName>Shape_Length_renamed</LengthFieldName>" in xml

    lyr = ds.GetLayerByName("area")
    lyr_defn = lyr.GetLayerDefn()

    # Delete Shape_Length and Shape_Area fields
    assert (
        lyr.DeleteField(lyr_defn.GetFieldIndex("Shape_Length_renamed"))
        == ogr.OGRERR_NONE
    )
    assert (
        lyr.DeleteField(lyr_defn.GetFieldIndex("Shape_Area_renamed")) == ogr.OGRERR_NONE
    )

    f = ogr.Feature(lyr_defn)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(dirname)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition area")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<AreaFieldName />" in xml
    assert "<LengthFieldName />" in xml

    ds = None


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit(tmp_vsimem):

    dirname = (
        tmp_vsimem
        / "test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit.gdb"
    )

    gdal.VectorTranslate(
        dirname,
        "data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb",
        options="-f OpenFileGDB -fid 1",
    )

    ds = ogr.Open(dirname)
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

    ds = None


###############################################################################
# Test AlterGeomFieldDefn()


def test_ogr_openfilegdb_write_alter_geom_field_defn(tmp_vsimem):

    dirname = tmp_vsimem / "test_ogr_openfilegdb_alter_geom_field_defn.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

    # Change name
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbLineString)
    assert (
        lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_NAME_FLAG)
        == ogr.OGRERR_NONE
    )
    assert lyr.GetGeometryColumn() == "shape_renamed"
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<Name>shape_renamed</Name>" in xml
    assert "WKID" in xml

    assert lyr.GetGeometryColumn() == "shape_renamed"
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"

    # Set SRS to None
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbLineString)
    fld_defn.SetSpatialRef(None)
    assert (
        lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG)
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is None
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is None

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "WKID" not in xml

    # Set SRS to EPSG:4326
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbLineString)
    fld_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG)
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is not None
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is not None

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<WKID>4326</WKID>" in xml

    srs4269 = osr.SpatialReference()
    srs4269.ImportFromEPSG(4269)

    # Set SRS to EPSG:4269
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbLineString)
    fld_defn.SetSpatialRef(srs4269)
    assert (
        lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG)
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is not None
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"
    ds = None

    ds = ogr.Open(dirname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is not None
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition test")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    xml = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert "<WKID>4269</WKID>" in xml

    # Changing geometry type not supported
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbPolygon)
    with gdal.quiet_errors():
        assert (
            lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_TYPE_FLAG)
            != ogr.OGRERR_NONE
        )

    # Changing nullable state not supported
    fld_defn = ogr.GeomFieldDefn("shape_renamed", ogr.wkbPolygon)
    fld_defn.SetNullable(False)
    with gdal.quiet_errors():
        assert (
            lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG)
            != ogr.OGRERR_NONE
        )

    ds = None


###############################################################################
# Test CreateField() with name = OBJECTID
# Cf https://github.com/qgis/QGIS/issues/51435


@pytest.mark.parametrize("field_type", [ogr.OFTInteger, ogr.OFTInteger64, ogr.OFTReal])
def test_ogr_openfilegdb_write_create_OBJECTID(tmp_vsimem, field_type):

    dirname = tmp_vsimem / "test_ogr_openfilegdb_write_create_OBJECTID.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    assert (
        lyr.CreateField(ogr.FieldDefn("unused_before", ogr.OFTString))
        == ogr.OGRERR_NONE
    )
    assert (
        lyr.CreateField(ogr.FieldDefn(lyr.GetFIDColumn(), field_type))
        == ogr.OGRERR_NONE
    )
    assert (
        lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger)) == ogr.OGRERR_NONE
    )
    assert lyr.GetLayerDefn().GetFieldCount() == 3

    # No FID, but OBJECTID
    f = ogr.Feature(lyr.GetLayerDefn())
    f[lyr.GetFIDColumn()] = 10
    f["int_field"] = 2
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 10
    f = None

    field_idx = lyr.GetLayerDefn().GetFieldIndex("unused_before")
    assert lyr.DeleteField(field_idx) == ogr.OGRERR_NONE

    assert (
        lyr.CreateField(ogr.FieldDefn("int_field2", ogr.OFTInteger)) == ogr.OGRERR_NONE
    )

    # FID and OBJECTID, both equal
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(11)
    f[lyr.GetFIDColumn()] = 11
    f["int_field"] = 3
    f["int_field2"] = 30
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 11

    f["int_field"] = 4
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE

    # FID and OBJECTID, different ==> error
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(12)
    f[lyr.GetFIDColumn()] = 13
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10
    assert f[lyr.GetFIDColumn()] == 10
    assert f["int_field"] == 2
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 11
    assert f[lyr.GetFIDColumn()] == 11
    assert f["int_field"] == 4
    assert f["int_field2"] == 30

    # Can't delete or alter OBJECTID field
    field_idx = lyr.GetLayerDefn().GetFieldIndex(lyr.GetFIDColumn())
    with gdal.quiet_errors():
        assert lyr.DeleteField(field_idx) == ogr.OGRERR_FAILURE
        assert (
            lyr.AlterFieldDefn(
                field_idx,
                lyr.GetLayerDefn().GetFieldDefn(field_idx),
                ogr.ALTER_ALL_FLAG,
            )
            == ogr.OGRERR_FAILURE
        )

    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10
    assert f["int_field"] == 2
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

    f = lyr.GetNextFeature()
    assert f.GetFID() == 11
    assert f["int_field"] == 4
    assert f["int_field2"] == 30

    ds = None


###############################################################################
# Test driver Delete() method


def test_ogr_openfilegdb_write_delete(tmp_path):

    dirname = tmp_path / "test_ogr_openfilegdb_write_delete.gdb"
    if gdal.VSIStatL(dirname) is not None:
        gdal.RmdirRecursive(dirname)
    drv = ogr.GetDriverByName("OpenFileGDB")
    ds = drv.CreateDataSource(dirname)
    ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = None
    assert gdal.VSIStatL(dirname) is not None
    assert drv.DeleteDataSource(dirname) == gdal.CE_None
    assert gdal.VSIStatL(dirname) is None


###############################################################################
# Test writing a CompoundCRS


@pytest.mark.parametrize(
    "write_wkid,write_vcswkid", [(True, True), (True, False), (False, False)]
)
@pytest.mark.require_proj(7, 2)
def test_ogr_openfilegdb_write_compound_crs(tmp_vsimem, write_wkid, write_vcswkid):

    dirname = tmp_vsimem / "test_ogr_openfilegdb_write_compound_crs.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""COMPOUNDCRS["WGS_1984_Complex_UTM_Zone_22N + MSL height",
PROJCRS["WGS_1984_Complex_UTM_Zone_22N",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["Degree",0.0174532925199433]]],
    CONVERSION["UTM zone 22N",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["Degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",-51,
            ANGLEUNIT["Degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8807]]],
    CS[Cartesian,2],
        AXIS["(E)",east,
            ORDER[1],
            LENGTHUNIT["metre",1]],
        AXIS["(N)",north,
            ORDER[2],
            LENGTHUNIT["metre",1]],
    USAGE[
        SCOPE["Not known."],
        AREA["Between 54°W and 48°W, northern hemisphere between equator and 84°N, onshore and offshore."],
        BBOX[0,-54,84,-48]],
    ID["ESRI",102572]],
VERTCRS["MSL height",
    VDATUM["Mean Sea Level"],
    CS[vertical,1],
        AXIS["gravity-related height (H)",up,
            LENGTHUNIT["metre",1]],
    USAGE[
        SCOPE["Hydrography, drilling."],
        AREA["World."],
        BBOX[-90,-180,90,180]],
    ID["EPSG",5714]]]
    """)
    d = {
        "OPENFILEGDB_WRITE_WKID": None if write_wkid else "FALSE",
        "OPENFILEGDB_WRITE_VCSWKID": None if write_vcswkid else "FALSE",
    }
    with gdaltest.config_options(d):
        ds.CreateLayer("test", geom_type=ogr.wkbPoint, srs=srs)
        ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    got_srs = lyr.GetSpatialRef()
    assert got_srs.IsSame(srs)


###############################################################################
# Test writing empty geometries


@pytest.mark.parametrize(
    "geom_type",
    [
        ogr.wkbPoint,
        ogr.wkbPoint25D,
        ogr.wkbPointM,
        ogr.wkbPointZM,
        ogr.wkbMultiLineString,
        ogr.wkbMultiLineString25D,
        ogr.wkbMultiLineStringM,
        ogr.wkbMultiLineStringZM,
        ogr.wkbMultiPolygon,
        ogr.wkbMultiPolygon25D,
        ogr.wkbMultiPolygonM,
        ogr.wkbMultiPolygonZM,
    ],
)
def test_ogr_openfilegdb_write_empty_geoms(tmp_vsimem, geom_type):

    dirname = tmp_vsimem / "test_ogr_openfilegdb_write_empty_geoms.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    lyr = ds.CreateLayer("test", geom_type=geom_type)
    f = ogr.Feature(lyr.GetLayerDefn())
    g = ogr.Geometry(geom_type)
    f.SetGeometry(g)
    with gdaltest.config_option("OGR_OPENFILEGDB_WRITE_EMPTY_GEOMETRY", "YES"):
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == geom_type
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == geom_type
    assert g.IsEmpty()


###############################################################################
# Test creating layer with alias name


def test_ogr_openfilegdb_layer_alias_name(tmp_vsimem):

    dirname = tmp_vsimem / "out.gdb"

    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(dirname)
    ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["LAYER_ALIAS=my_alias"])
    ds = None

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem("ALIAS_NAME") == "my_alias"
    ds = None


###############################################################################
# Test creating layer with a Integer64 field (ArcGIS Pro >= 3.2)


def test_ogr_openfilegdb_write_int64(tmp_vsimem):

    filename = str(tmp_vsimem / "out.gdb")
    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(filename)
    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbNone,
        options=["TARGET_ARCGIS_VERSION=ARCGIS_PRO_3_2_OR_LATER"],
    )
    fld_defn = ogr.FieldDefn("int64", ogr.OFTInteger64)
    fld_defn.SetDefault("1234567890123")
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["int64"] = -1234567890123
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["int64"] = 1234567890123
    lyr.CreateFeature(f)
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetType() == ogr.OFTInteger64
    assert fld_defn.GetDefault() == "1234567890123"
    f = lyr.GetNextFeature()
    assert f["int64"] == -1234567890123

    ds = ogr.Open(filename, update=1)
    ds.ExecuteSQL("CREATE INDEX idx_int64 ON test(int64)")
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

    lyr.SetAttributeFilter("int64 > 0")
    f = lyr.GetNextFeature()
    assert f["int64"] == 1234567890123

    lyr.SetAttributeFilter("int64 < 0")
    f = lyr.GetNextFeature()
    assert f["int64"] == -1234567890123

    with ds.ExecuteSQL("SELECT MIN(int64), MAX(int64) FROM test") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["MIN_int64"] == -1234567890123
        assert f["MAX_int64"] == 1234567890123


###############################################################################
# Test creating layer with DateOnly, TimeOnly, TimestampOffset fields (ArcGIS Pro >= 3.2)


def test_ogr_openfilegdb_write_new_datetime_types(tmp_vsimem):

    filename = str(tmp_vsimem / "out.gdb")
    with gdal.quiet_errors():
        gdal.ErrorReset()
        ds = gdal.VectorTranslate(
            filename,
            "data/filegdb/arcgis_pro_32_types.gdb",
            layers=["date_types", "date_types_high_precision"],
            layerCreationOptions=["TARGET_ARCGIS_VERSION=ARCGIS_PRO_3_2_OR_LATER"],
        )
        assert gdal.GetLastErrorMsg() == ""

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

    lyr = ds.GetLayerByName("date_types_high_precision")
    lyr_defn = lyr.GetLayerDefn()

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

    ds.ExecuteSQL("CREATE INDEX idx_date_only ON date_types(date_only)")
    ds.ExecuteSQL("CREATE INDEX idx_time_only ON date_types(time_only)")
    ds.ExecuteSQL("CREATE INDEX idx_tsoffset ON date_types(timestamp_offset)")
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

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
        assert f["MIN_timestamp_offset"] == "1900/12/31 14:01:01"
        assert f["MAX_timestamp_offset"] == "2023/12/30 14:01:01"


###############################################################################
# Test updating an existing feature with one whose m_nRowBlobLength is
# larger than m_nHeaderBufferMaxSize


def test_ogr_openfilegdb_write_update_feature_larger(tmp_vsimem):

    filename = str(tmp_vsimem / "out.gdb")
    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", srs, ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    g = ogr.Geometry(ogr.wkbLineString)
    g.SetPoint_2D(10, 0, 0)
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = ogr.Geometry(ogr.wkbLineString)
    g.SetPoint_2D(999, 0, 0)
    f.SetGeometry(g)
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 1000


###############################################################################
# Test geometry coordinate precision support


def test_ogr_openfilegdb_write_geom_coord_precision(tmp_vsimem):

    filename = str(tmp_vsimem / "test.gdb")
    ds = gdal.GetDriverByName("OpenFileGDB").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbPointZM)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-5, 1e-3, 1e-2)
    geom_fld.SetCoordinatePrecision(prec)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-2
    assert prec.GetFormats() == ["FileGeodatabase"]
    opts = prec.GetFormatSpecificOptions("FileGeodatabase")
    for key in opts:
        opts[key] = float(opts[key])
    assert opts == {
        "MOrigin": -100000.0,
        "MScale": 100.0,
        "MTolerance": 0.001,
        "XOrigin": -2147483647.0,
        "XYScale": 100000.0,
        "XYTolerance": 1e-06,
        "YOrigin": -2147483647.0,
        "ZOrigin": -100000.0,
        "ZScale": 1000.0,
        "ZTolerance": 0.0001,
    }
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT(1.23456789 2.34567891 9.87654321 -9.87654321)")
    )
    lyr.CreateFeature(f)
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-2
    assert prec.GetFormats() == ["FileGeodatabase"]
    opts = prec.GetFormatSpecificOptions("FileGeodatabase")
    for key in opts:
        try:
            opts[key] = float(opts[key])
        except ValueError:
            pass
    assert opts == {
        "MOrigin": -100000.0,
        "MScale": 100.0,
        "MTolerance": 0.001,
        "XOrigin": -2147483647.0,
        "XYScale": 100000.0,
        "XYTolerance": 1e-06,
        "YOrigin": -2147483647.0,
        "ZOrigin": -100000.0,
        "ZScale": 1000.0,
        "ZTolerance": 0.0001,
        "HighPrecision": "true",
    }
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetX(0) == pytest.approx(1.23456789, abs=1e-5)
    assert g.GetX(0) != pytest.approx(1.23456789, abs=1e-8)
    assert g.GetY(0) == pytest.approx(2.34567891, abs=1e-5)
    assert g.GetZ(0) == pytest.approx(9.87654321, abs=1e-3)
    assert g.GetZ(0) != pytest.approx(9.87654321, abs=1e-8)
    assert g.GetM(0) == pytest.approx(-9.87654321, abs=1e-2)
    assert g.GetM(0) != pytest.approx(-9.87654321, abs=1e-8)
    ds.Close()

    j = gdal.VectorInfo(filename, format="json")
    j_geom_field = j["layers"][0]["geometryFields"][0]
    assert j_geom_field["xyCoordinateResolution"] == 1e-5
    assert j_geom_field["zCoordinateResolution"] == 1e-3
    assert j_geom_field["mCoordinateResolution"] == 1e-2
    assert j_geom_field["coordinatePrecisionFormatSpecificOptions"] == {
        "FileGeodatabase": {
            "XOrigin": -2147483647,
            "YOrigin": -2147483647,
            "XYScale": 100000,
            "ZOrigin": -100000,
            "ZScale": 1000,
            "MOrigin": -100000,
            "MScale": 100,
            "XYTolerance": 1e-06,
            "ZTolerance": 0.0001,
            "MTolerance": 0.001,
            "HighPrecision": "true",
        }
    }

    filename2 = str(tmp_vsimem / "test2.gdb")
    gdal.VectorTranslate(filename2, filename, format="OpenFileGDB")

    ds = ogr.Open(filename2)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    opts = prec.GetFormatSpecificOptions("FileGeodatabase")
    for key in opts:
        try:
            opts[key] = float(opts[key])
        except ValueError:
            pass
    assert opts == {
        "MOrigin": -100000.0,
        "MScale": 100.0,
        "MTolerance": 0.001,
        "XOrigin": -2147483647.0,
        "XYScale": 100000.0,
        "XYTolerance": 1e-06,
        "YOrigin": -2147483647.0,
        "ZOrigin": -100000.0,
        "ZScale": 1000.0,
        "ZTolerance": 0.0001,
        "HighPrecision": "true",
    }


###############################################################################
# Test repairing a corrupted header
# Scenario similar to https://github.com/qgis/QGIS/issues/57536


def test_ogr_openfilegdb_repair_corrupted_header(tmp_vsimem):

    filename = str(tmp_vsimem / "out.gdb")
    ds = ogr.GetDriverByName("OpenFileGDB").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", srs, ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    g = ogr.Geometry(ogr.wkbLineString)
    g.SetPoint_2D(10, 0, 0)
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None

    # Corrupt m_nHeaderBufferMaxSize field
    corrupted_filename = filename + "/a00000004.gdbtable"
    f = gdal.VSIFOpenL(corrupted_filename, "r+b")
    assert f
    gdal.VSIFSeekL(f, 8, 0)
    gdal.VSIFWriteL(b"\x00" * 4, 4, 1, f)
    gdal.VSIFCloseL(f)

    with gdal.config_option(
        "OGR_OPENFILEGDB_ERROR_ON_INCONSISTENT_BUFFER_MAX_SIZE", "NO"
    ), gdal.quiet_errors():
        ds = ogr.Open(filename)
    assert (
        gdal.GetLastErrorMsg()
        == f"A corruption in the header of {corrupted_filename} has been detected. It would need to be repaired to be properly read by other software, either by using ogr2ogr to generate a new dataset, or by opening this dataset in update mode and reading all its records."
    )
    assert ds.GetLayerCount() == 1

    with gdal.config_option(
        "OGR_OPENFILEGDB_ERROR_ON_INCONSISTENT_BUFFER_MAX_SIZE", "NO"
    ), gdal.quiet_errors():
        ds = ogr.Open(filename, update=1)
    assert (
        gdal.GetLastErrorMsg()
        == f"A corruption in the header of {corrupted_filename} has been detected. It is going to be repaired to be properly read by other software."
    )
    assert ds.GetLayerCount() == 1

    with gdal.config_option(
        "OGR_OPENFILEGDB_ERROR_ON_INCONSISTENT_BUFFER_MAX_SIZE", "NO"
    ), gdal.quiet_errors():
        ds = ogr.Open(filename)
    assert gdal.GetLastErrorMsg() == ""
    assert ds.GetLayerCount() == 1


###############################################################################
# Test writing special value OGRUnsetMarker = -21121 in a int32 field


def test_ogr_openfilegdb_write_OGRUnsetMarker(tmp_vsimem):

    filename = str(tmp_vsimem / "out.gdb")
    with ogr.GetDriverByName("OpenFileGDB").CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("i32", ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["i32"] = -21121
        lyr.CreateFeature(f)
    with ogr.Open(filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["i32"] == -21121


###############################################################################
# Verify that we can generate an output that is byte-identical to the expected golden file.


@pytest.mark.parametrize(
    "src_directory",
    [
        # Generated with:
        # ogr2ogr autotest/ogr/data/openfilegdb/polygon_golden.gdb '{"type":"Feature","properties":{"foo":"bar"},"geometry":{"type":"Polygon","coordinates":[[[0,0],[0,1],[1,0],[0,0]]]}}' --config OPENFILEGDB_CREATOR GDAL --config OPENFILEGDB_REPRODUCIBLE_UUID YES -f openfilegdb
        "data/openfilegdb/polygon_golden.gdb",
    ],
)
def test_ogr_openfilegdb_write_check_golden_file(tmp_path, src_directory):

    out_directory = str(tmp_path / "test.gdb")
    with gdaltest.config_options(
        {"OPENFILEGDB_CREATOR": "GDAL", "OPENFILEGDB_REPRODUCIBLE_UUID": "YES"}
    ):
        gdal.VectorTranslate(out_directory, src_directory, format="OpenFileGDB")
    for filename in os.listdir(src_directory):
        src_filename = os.path.join(src_directory, filename)
        out_filename = os.path.join(out_directory, filename)

        assert os.stat(src_filename).st_size == os.stat(out_filename).st_size, filename
        assert (
            open(src_filename, "rb").read() == open(out_filename, "rb").read()
        ), filename


###############################################################################
# Test 'gdal driver openfilegdb repack'


@gdaltest.enable_exceptions()
def test_ogropenfilegdb_write_gdal_driver_openfilegdb_repack(tmp_path):

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]
    assert alg.GetName() == "driver"

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["openfilegdb"]
    assert alg.GetName() == "openfilegdb"

    out_directory = str(tmp_path / "test.gdb")
    gdal.VectorTranslate(
        out_directory, "data/openfilegdb/polygon_golden.gdb", format="OpenFileGDB"
    )

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["openfilegdb"]["repack"]
    assert alg.GetName() == "repack"
    alg["dataset"] = "data/poly.shp"
    with pytest.raises(Exception, match="is not a FileGeoDatabase"):
        alg.Run()

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["openfilegdb"]["repack"]
    assert alg.GetName() == "repack"
    alg["dataset"] = out_directory
    assert alg.Run()

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["openfilegdb"]["repack"]
    assert alg.GetName() == "repack"
    alg["dataset"] = out_directory
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
