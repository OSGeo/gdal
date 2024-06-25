#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR JOIN support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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


import ogrtest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Test a join.


def test_ogr_join_1():
    ds = ogr.Open("data")

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM poly LEFT JOIN idlink ON poly.eas_id = idlink.eas_id"
    )

    count = sql_lyr.GetFeatureCount()
    assert count == 10, (
        "Got wrong count with GetFeatureCount() - %d, expecting 10" % count
    )

    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Check the values we are actually getting back (restricting the search a bit)


def test_ogr_join_2():

    ds = ogr.Open("data")
    expect = ["_166_", "_158_", "_165_"]

    with ds.ExecuteSQL(
        "SELECT * FROM poly "
        "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
        "WHERE eas_id < 168"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "NAME", expect)


###############################################################################
# Try various naming conversions for the selected fields.


def test_ogr_join_3():

    ds = ogr.Open("data")
    expect = ["_166_", "_158_", "_165_"]

    with ds.ExecuteSQL(
        "SELECT poly.area, idlink.* FROM poly "
        "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
        "WHERE eas_id < 168"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "idlink.NAME", expect)


###############################################################################
# Verify that records for which a join can't be found work ok.


def test_ogr_join_4():

    ds = ogr.Open("data")
    expect = ["_179_", "_171_", None, None]

    with ds.ExecuteSQL(
        "SELECT poly.*, name FROM poly "
        + "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
        + "WHERE eas_id > 170"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "NAME", expect)


###############################################################################
# Verify that table aliases work


def test_ogr_join_5():

    ds = ogr.Open("data")
    expect = [179, 171, 173, 172]

    with ds.ExecuteSQL(
        "SELECT p.*, il.name FROM poly p "
        + "LEFT JOIN idlink il ON p.eas_id = il.eas_id "
        + "WHERE eas_id > 170"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "p.eas_id", expect)


###############################################################################
# Again, ordering by a primary field.


def test_ogr_join_6():

    ds = ogr.Open("data")
    expect = [171, 172, 173, 179]

    with ds.ExecuteSQL(
        "SELECT p.*, il.name FROM poly p "
        + "LEFT JOIN idlink il ON p.eas_id = il.eas_id "
        + "WHERE eas_id > 170 ORDER BY p.eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "p.eas_id", expect)


###############################################################################
# Test joining to an external datasource.


def test_ogr_join_7():

    ds = ogr.Open("data")
    expect = [171, 172, 173, 179]

    with ds.ExecuteSQL(
        "SELECT p.*, il.name FROM poly p "
        + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il.eas_id '
        + "WHERE eas_id > 170 ORDER BY p.eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "p.eas_id", expect)


###############################################################################
# Test doing two joins at once.


def test_ogr_join_8():

    ds = ogr.Open("data")
    expect = [171, None, None, 179]

    with ds.ExecuteSQL(
        "SELECT p.*, il.name, il2.eas_id FROM poly p "
        + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il.eas_id '
        + "LEFT JOIN idlink il2 ON p.eas_id = il2.eas_id "
        + "WHERE eas_id > 170 ORDER BY p.eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "il2.eas_id", expect)


###############################################################################
# Verify fix for #2788 (memory corruption on wildcard expansion in SQL request
# with join clauses)


def test_ogr_join_9():

    ds = ogr.Open("data")
    expect = [179, 171, 173, 172]

    with ds.ExecuteSQL(
        "SELECT poly.* FROM poly "
        + "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
        + "WHERE eas_id > 170"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "poly.EAS_ID", expect)


###############################################################################


def test_ogr_join_10():

    ds = ogr.Open("data")
    expect = [None, None, None, None, None, None, None, None, None, None]

    with ds.ExecuteSQL(
        "SELECT * FROM poly " + "LEFT JOIN idlink2 ON poly.eas_id = idlink2.name "
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "F3", expect)


###############################################################################
# Test join on string field


def test_ogr_join_11():

    ds = ogr.Open("data")
    expect = ["_168_", "_179_", "_171_", "_170_", "_165_", "_158_", "_166_"]

    with ds.ExecuteSQL(
        "SELECT il.*, il2.* FROM idlink il LEFT JOIN idlink2 il2 ON il.NAME = il2.NAME"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "il2.NAME", expect)


###############################################################################
# Test fix for #4112 (join between 2 datasources)


def test_ogr_join_12():
    ds = ogr.Open("data/poly.shp")

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM poly LEFT JOIN 'data/idlink.dbf'.idlink ON poly.eas_id = idlink.eas_id"
    )

    count = sql_lyr.GetFeatureCount()
    assert count == 10, (
        "Got wrong count with GetFeatureCount() - %d, expecting 10" % count
    )

    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test joining a float column with a string column (#4321)


def test_ogr_join_13():

    ds = ogr.Open("data")
    expect = [
        "_168_",
        "_179_",
        "_171_",
        None,
        None,
        None,
        "_166_",
        "_158_",
        "_165_",
        "_170_",
    ]

    with ds.ExecuteSQL(
        "SELECT * FROM poly " + "LEFT JOIN idlink2 ON poly.eas_id = idlink2.eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "name", expect)


###############################################################################
# Test joining a string column with a float column (#4321, actually addressed by #4259)


def test_ogr_join_14():

    ds = ogr.Open("data")
    expect = [168, 179, 171, 170, 165, 158, 166]

    with ds.ExecuteSQL(
        "SELECT * FROM idlink2 " + "LEFT JOIN poly ON idlink2.eas_id = poly.eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "poly.EAS_ID", expect)


###############################################################################
# Test multiple joins with expressions (#4521)


@pytest.mark.require_driver("CSV")
def test_ogr_join_15():

    ds = ogr.GetDriverByName("CSV").CreateDataSource("/vsimem/ogr_join_14")
    lyr = ds.CreateLayer("first")
    ogrtest.quick_create_layer_def(lyr, [["id"]])
    ogrtest.quick_create_feature(lyr, ["key"], None)

    lyr = ds.CreateLayer("second")
    ogrtest.quick_create_layer_def(lyr, [["col1_2"], ["id"], ["col3_2"]])
    ogrtest.quick_create_feature(lyr, ["a2", "key", "c2"], None)

    lyr = ds.CreateLayer("third")
    ogrtest.quick_create_layer_def(lyr, [["col1_3"], ["id"], ["col3_3"]])
    ogrtest.quick_create_feature(lyr, ["a3", "key", "c3"], None)

    sql_lyr = ds.ExecuteSQL(
        "SELECT concat(col3_2, ''), col3_2 FROM first JOIN second ON first.id = second.id JOIN third ON first.id = third.id"
    )
    feat = sql_lyr.GetNextFeature()
    val1 = feat.GetFieldAsString(0)
    val2 = feat.GetFieldAsString(1)
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    gdal.Unlink("/vsimem/ogr_join_14/first.csv")
    gdal.Unlink("/vsimem/ogr_join_14/second.csv")
    gdal.Unlink("/vsimem/ogr_join_14/third.csv")
    gdal.Unlink("/vsimem/ogr_join_14")

    assert val1 == "c2"

    assert val2 == "c2"


###############################################################################
# Test non-support of a secondarytable.fieldname in a where clause


def test_ogr_join_16():

    ds = ogr.Open("data")

    with pytest.raises(Exception):
        ds.ExecuteSQL(
            "SELECT * FROM poly "
            + "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
            + "WHERE idlink.name = '_165'"
        )


###############################################################################
# Test non-support of a secondarytable.fieldname in a order by clause


def test_ogr_join_17():

    ds = ogr.Open("data")

    with pytest.raises(Exception):
        ds.ExecuteSQL(
            "SELECT * FROM poly "
            + "LEFT JOIN idlink ON poly.eas_id = idlink.eas_id "
            + "ORDER BY name"
        )


###############################################################################
# Test inverted order of fields in ON


def test_ogr_join_18():

    ds = ogr.Open("data")

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM poly LEFT JOIN idlink ON idlink.eas_id = poly.eas_id"
    )

    count = sql_lyr.GetFeatureCount()
    assert count == 10, (
        "Got wrong count with GetFeatureCount() - %d, expecting 10" % count
    )

    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test unrecognized primary field


def test_ogr_join_19():

    ds = ogr.Open("data")

    with pytest.raises(Exception):
        ds.ExecuteSQL("SELECT * FROM poly LEFT JOIN idlink ON poly.foo = idlink.eas_id")


###############################################################################
# Test unrecognized secondary field


def test_ogr_join_20():

    ds = ogr.Open("data")

    with pytest.raises(Exception):
        ds.ExecuteSQL("SELECT * FROM poly LEFT JOIN idlink ON poly.eas_id = idlink.foo")


###############################################################################
# Test unexpected secondary table


def test_ogr_join_21():

    ds = ogr.Open("data")

    with pytest.raises(Exception):
        ds.ExecuteSQL(
            "SELECT p.*, il.name, il2.eas_id FROM poly p "
            + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il2.eas_id '
            + "LEFT JOIN idlink il2 ON p.eas_id = il2.eas_id"
        )


###############################################################################
# Test join with a complex expression as ON


def test_ogr_join_22():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("first")
    ogrtest.quick_create_layer_def(lyr, [["id.1"], ["id2"]])
    ogrtest.quick_create_feature(lyr, ["key1", "key2"], None)

    lyr = ds.CreateLayer("second")
    ogrtest.quick_create_layer_def(lyr, [["id.1"], ["id2"], ["val"]])
    ogrtest.quick_create_feature(lyr, ["key1", "keyX", "1"], None)
    ogrtest.quick_create_feature(lyr, ["key1", "key2", "2"], None)
    ogrtest.quick_create_feature(lyr, ["key1", "keyY", "3"], None)

    sql_lyr = ds.ExecuteSQL(
        'SELECT val FROM first JOIN second ON first."id.1" = second."id.1" AND first.id2 = second.id2'
    )
    feat = sql_lyr.GetNextFeature()
    val = feat.GetFieldAsString(0)
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    assert val == "2"


###############################################################################
# Test join with NULL keys


def test_ogr_join_23():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("first")
    ogrtest.quick_create_layer_def(lyr, [["f"]])
    ogrtest.quick_create_feature(lyr, [None], None)
    ogrtest.quick_create_feature(lyr, ["key1"], None)

    lyr = ds.CreateLayer("second")
    ogrtest.quick_create_layer_def(lyr, [["f"]])
    ogrtest.quick_create_feature(lyr, ["key1"], None)
    ogrtest.quick_create_feature(lyr, [None], None)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM first JOIN second ON first.f = second.f")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull("second.f"):
        feat.DumpReadable()
        pytest.fail()
    feat = sql_lyr.GetNextFeature()
    if feat["f"] != "key1" or feat["second.f"] != "key1":
        feat.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test join on special fields (FID)


def test_ogr_join_on_special_field():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr1 = ds.CreateLayer("lyr1", options=["FID=fid1"])
    lyr1.CreateField(ogr.FieldDefn("a"))
    lyr2 = ds.CreateLayer("lyr2", options=["FID=fid2"])
    lyr2.CreateField(ogr.FieldDefn("b"))
    f = ogr.Feature(lyr1.GetLayerDefn())
    f.SetFID(1)
    f["a"] = "a1"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,0 0))"))
    f.SetStyleString("dummy")
    lyr1.CreateFeature(f)
    f = ogr.Feature(lyr1.GetLayerDefn())
    f.SetFID(2)
    f["a"] = "a2"
    lyr1.CreateFeature(f)
    f = ogr.Feature(lyr2.GetLayerDefn())
    f.SetFID(1)
    f["b"] = "b1"
    lyr2.CreateFeature(f)
    f = ogr.Feature(lyr2.GetLayerDefn())
    f.SetFID(2)
    f["b"] = "b2"
    lyr2.CreateFeature(f)

    with ds.ExecuteSQL(
        "SELECT a, b FROM lyr1 LEFT JOIN lyr2 ON lyr1.fid1 = lyr2.fid2"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a1"
        assert f["b"] == "b1"
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a2"
        assert f["b"] == "b2"
        assert sql_lyr.GetNextFeature() is None

    # Kind of dummy, but testing Real special field ...
    with ds.ExecuteSQL(
        "SELECT a, b FROM lyr1 LEFT JOIN lyr2 ON lyr1.OGR_GEOM_AREA = 0.5"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a1"
        assert f["b"] == "b1"
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a2"
        assert f["b"] is None
        assert sql_lyr.GetNextFeature() is None

    # Kind of dummy, but testing String special field ...
    with ds.ExecuteSQL(
        "SELECT a, b FROM lyr1 LEFT JOIN lyr2 ON lyr1.OGR_STYLE = 'dummy'"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a1"
        assert f["b"] == "b1"
        f = sql_lyr.GetNextFeature()
        assert f["a"] == "a2"
        assert f["b"] is None
        assert sql_lyr.GetNextFeature() is None
