#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MySQL driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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


import re

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("MySQL")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


# E. Rouault : this is almost a copy & paste from ogr_pg.py

#
# To create the required MySQL instance do something like:
#
#  $ mysql -u root -p
#     mysql> CREATE DATABASE autotest;
#     mysql> GRANT ALL ON autotest.* TO 'THE_USER_THAT_RUNS_AUTOTEST'@'localhost';
#

###############################################################################
# Module-scoped database connection to verify connection parameters and
# support fixtures that describe database capabilities. This fixture should
# generally not be used directly by tests.


@pytest.fixture(scope="module")
def mysql_autotest_ds():

    val = gdal.GetConfigOption("OGR_MYSQL_CONNECTION_STRING", None)
    if val is not None:
        mysql_connection_string = val
    else:
        mysql_connection_string = "MYSQL:autotest"

    try:
        ds = ogr.Open(mysql_connection_string, update=1)
    except RuntimeError:
        ds = None

    if ds is None:
        if val:
            pytest.skip(
                f"MySQL database is not available using supplied connection string {mysql_connection_string}"
            )
        else:
            pytest.skip(
                f"OGR_MYSQL_CONNECTION_STRING not specified; database is not available using default connection string {mysql_connection_string}"
            )

    yield ds


@pytest.fixture(scope="module")
def mysql_version(mysql_autotest_ds):

    with mysql_autotest_ds.ExecuteSQL("SELECT VERSION()") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        print("Version: " + f.GetField(0))

        return f.GetField(0)


@pytest.fixture()
def mysql_is_8_or_later(mysql_version):

    return int(mysql_version.split(".")[0]) >= 8 and "MariaDB" not in mysql_version


###############################################################################
# Test-scoped database connection that sets the active database to a temporary
# database that will be destroyed at the end of the test.


@pytest.fixture()
def mysql_ds(mysql_autotest_ds, request):

    schema = request.node.name

    mysql_autotest_ds.ExecuteSQL(f"CREATE DATABASE {schema}")

    test_connection_string = re.sub(
        r"(?<=mysql:)[\w]+",
        schema,
        mysql_autotest_ds.GetDescription(),
        flags=re.IGNORECASE,
    )

    ds = ogr.Open(test_connection_string, update=1)

    assert ds is not None

    yield ds

    mysql_autotest_ds.ExecuteSQL(f"DROP DATABASE {schema}")


###############################################################################
# Create table from data/poly.shp


@pytest.fixture()
def tpoly(mysql_ds):

    shp_ds = ogr.Open("data/poly.shp")
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    mysql_lyr = mysql_ds.CreateLayer("tpoly", srs=shp_lyr.GetSpatialRef(), options=[])

    assert mysql_lyr is not None

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        mysql_lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
            ("INT64", ogr.OFTInteger64),
        ],
    )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=mysql_lyr.GetLayerDefn())

    for feat in shp_lyr:

        dst_feat.SetFrom(feat)
        dst_feat.SetField("INT64", 1234567890123)
        mysql_lyr.CreateFeature(dst_feat)

    assert (
        mysql_lyr.GetFeatureCount() == shp_lyr.GetFeatureCount()
    ), "not matching feature count"

    assert mysql_lyr.GetSpatialRef().GetAuthorityCode(
        None
    ) == shp_lyr.GetSpatialRef().GetAuthorityCode(None), "not matching spatial ref"


###############################################################################
# Test reading a layer extent


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_19(mysql_ds):

    layer = mysql_ds.GetLayerByName("tpoly")
    if layer is None:
        pytest.fail("did not get tpoly layer")
    assert layer.GetDataset().GetDescription() == mysql_ds.GetDescription()

    extent = layer.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail("Extents do not match")


###############################################################################
# Verify that stuff we just wrote is still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_3(mysql_ds, poly_feat):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    assert mysql_lyr.GetGeometryColumn() == "SHAPE"

    assert mysql_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(mysql_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(mysql_lyr, "eas_id", expect)

        assert mysql_lyr.GetFeatureCount() == 5

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = mysql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.001
        )

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )
        assert read_feat.GetField("INT64") == 1234567890123


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_4(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    # E. Rouault : the mySQL driver doesn't seem to like adding new features and
    # iterating over a query at the same time.
    # If trying to do so, we get the 'Commands out of sync' error.

    wkt_list = ["10", "2", "1", "4", "5", "6"]

    mysql_lyr.ResetReading()

    feature_def = mysql_lyr.GetLayerDefn()

    for item in wkt_list:
        dst_feat = ogr.Feature(feature_def)

        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        mysql_lyr.CreateFeature(dst_feat)

    # FIXME : The source wkt polygons of '4' and '6' are not closed and
    # mySQL return them as closed, so the check_feature_geometry returns FALSE
    # Checking them after closing the rings again returns TRUE.

    wkt_list = ["10", "2", "1", "5", "4", "6"]

    for item in wkt_list:
        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Read back the feature and get the geometry.

        mysql_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = mysql_lyr.GetNextFeature()

        try:
            ogrtest.check_feature_geometry(feat_read, geom)
        except AssertionError:
            print("Geometry changed. Closing rings before trying again for wkt #", item)
            print("(before):", geom.ExportToWkt())
            geom.CloseRings()
            print("(after) :", geom.ExportToWkt())
            ogrtest.check_feature_geometry(feat_read, geom)


###############################################################################
# Test ExecuteSQL() results layers without geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_5(mysql_ds):

    lyr = mysql_ds.GetLayerByName("tpoly")
    for _ in range(2):
        dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt("POINT (0 0)")
        dst_feat.SetGeometryDirectly(geom)
        lyr.CreateFeature(dst_feat)

    lyr.ResetReading()

    # E. Rouault : unlike PostgreSQL driver : None is sorted in last position
    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    with mysql_ds.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    ) as sql_lyr:

        assert sql_lyr.GetFeatureCount() == 11

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ExecuteSQL() results layers with geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_6(mysql_ds):

    with mysql_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '35043413'"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "prfedea", ["35043413"])

        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            feat_read,
            "POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))",
            max_error=1e-3,
        )

        sql_lyr.ResetReading()

        with ogrtest.spatial_filter(sql_lyr, "LINESTRING(-10 -10,0 0)"):

            assert sql_lyr.GetFeatureCount() == 0

            assert (
                sql_lyr.GetNextFeature() is None
            ), "GetNextFeature() did not return None"


###############################################################################
# Test spatial filtering.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_7(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    mysql_lyr.SetAttributeFilter(None)

    with ogrtest.spatial_filter(mysql_lyr, "LINESTRING(479505 4763195,480526 4762819)"):

        assert mysql_lyr.GetFeatureCount() == 1

        ogrtest.check_features_against_list(mysql_lyr, "eas_id", [158])

        with ogrtest.attribute_filter(mysql_lyr, "eas_id = 158"):

            assert mysql_lyr.GetFeatureCount() == 1


###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_8(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    dst_feat = ogr.Feature(feature_def=mysql_lyr.GetLayerDefn())

    dst_feat.SetField("PRFEDEA", "CrazyKey")
    dst_feat.SetField("SHORTNAME", "Crazy\"'Long")
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    mysql_lyr.CreateFeature(dst_feat)

    mysql_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = mysql_lyr.GetNextFeature()

    assert feat_read is not None, "creating crazy feature failed!"

    assert (
        feat_read.GetField("shortname") == "Crazy\"'L"
    ), "Value not properly escaped or truncated:" + feat_read.GetField("shortname")


###############################################################################
# Verify inplace update of a feature with SetFeature().


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_9(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    mysql_lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    feat = mysql_lyr.GetNextFeature()
    mysql_lyr.SetAttributeFilter(None)

    feat.SetField("SHORTNAME", "Reset")

    point = ogr.Geometry(ogr.wkbPoint25D)
    point.SetPoint(0, 5, 6)
    feat.SetGeometryDirectly(point)

    assert mysql_lyr.SetFeature(feat) == 0, "SetFeature() method failed."

    fid = feat.GetFID()

    feat = mysql_lyr.GetFeature(fid)
    assert feat is not None, "GetFeature(%d) failed." % fid

    shortname = feat.GetField("SHORTNAME")
    assert shortname[:5] == "Reset", (
        "SetFeature() did not update SHORTNAME, got %s." % shortname
    )

    ogrtest.check_feature_geometry(feat, "POINT(5 6)")

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert (
        mysql_lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."

    # Test deleting non-existing feature
    assert (
        mysql_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."


###############################################################################
# Verify that DeleteFeature() works properly.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_10(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    mysql_lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    feat = mysql_lyr.GetNextFeature()
    mysql_lyr.SetAttributeFilter(None)

    fid = feat.GetFID()

    assert mysql_lyr.DeleteFeature(fid) == 0, "DeleteFeature() method failed."

    mysql_lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    feat = mysql_lyr.GetNextFeature()
    mysql_lyr.SetAttributeFilter(None)

    assert feat is None


###############################################################################
# Test very large query.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_15(mysql_ds):

    mysql_lyr = mysql_ds.GetLayerByName("tpoly")

    expect = [169]

    query = "eas_id = 169"

    for i in range(1000):
        query = query + (" or eas_id = %d" % (i + 1000))

    with ogrtest.attribute_filter(mysql_lyr, query):
        ogrtest.check_features_against_list(mysql_lyr, "eas_id", expect)


###############################################################################
# Test very large statement.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_16(mysql_ds):

    expect = [169]

    query = "eas_id = 169"

    for ident in range(1000):
        query = query + (" or eas_id = %d" % (ident + 1000))

    statement = "select eas_id from tpoly where " + query

    with mysql_ds.ExecuteSQL(statement) as lyr:

        ogrtest.check_features_against_list(lyr, "eas_id", expect)


###############################################################################
# Test requesting a non-existent table by name (bug 1480).


def test_ogr_mysql_17(mysql_ds):

    count = mysql_ds.GetLayerCount()
    layer = mysql_ds.GetLayerByName("JunkTableName")
    assert layer is None, "got layer for non-existent table!"

    assert count == mysql_ds.GetLayerCount(), "layer count changed unexpectedly."


###############################################################################


def test_ogr_mysql_20(mysql_ds):

    layer = mysql_ds.CreateLayer("select", options=[])
    ogrtest.quick_create_layer_def(
        layer, [("desc", ogr.OFTString), ("select", ogr.OFTString)]
    )
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())

    dst_feat.SetField("desc", "desc")
    dst_feat.SetField("select", "select")
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    layer.CreateFeature(dst_feat)

    layer = mysql_ds.GetLayerByName("select")
    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat.desc == "desc"
    assert feat.select == "select"


###############################################################################
# Test inserting NULL geometries into a table with a spatial index -> must FAIL


def test_ogr_mysql_21(mysql_ds):

    layer = mysql_ds.CreateLayer(
        "tablewithspatialindex", geom_type=ogr.wkbPoint, options=[]
    )
    ogrtest.quick_create_layer_def(layer, [("name", ogr.OFTString)])
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    dst_feat.SetField("name", "name")

    # The insertion MUST fail
    with gdal.quiet_errors():
        layer.CreateFeature(dst_feat)

    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat is None


###############################################################################
# Test inserting NULL geometries into a table without a spatial index


def test_ogr_mysql_22(mysql_ds):

    layer = mysql_ds.CreateLayer(
        "tablewithoutspatialindex", geom_type=ogr.wkbPoint, options=["SPATIAL_INDEX=NO"]
    )
    ogrtest.quick_create_layer_def(layer, [("name", ogr.OFTString)])
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    dst_feat.SetField("name", "name")

    layer.CreateFeature(dst_feat)

    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat is not None


###############################################################################
# Check for right precision


def test_ogr_mysql_23(mysql_ds, mysql_is_8_or_later):

    fields = ("zero", "widthonly", "onedecimal", "twentynine", "thirtyone")
    values = (
        1,
        2,
        1.1,
        0.12345678901234567890123456789,
        0.1234567890123456789012345678901,
    )
    precision = (0, 0, 1, 29, 0)

    ######################################################
    # Create a layer with a single feature through SQL

    if mysql_is_8_or_later:
        mysql_lyr = mysql_ds.ExecuteSQL(
            "SELECT ROUND(1.1,0) AS zero, ROUND(2.0, 0) AS widthonly, ROUND(1.1,1) AS onedecimal, ROUND(0.12345678901234567890123456789,29) AS twentynine, ST_GeomFromText(CONVERT('POINT(1.0 2.0)',CHAR)) as the_geom;"
        )
    else:
        mysql_lyr = mysql_ds.ExecuteSQL(
            "SELECT ROUND(1.1,0) AS zero, ROUND(2.0, 0) AS widthonly, ROUND(1.1,1) AS onedecimal, ROUND(0.12345678901234567890123456789,29) AS twentynine, GeomFromText(CONVERT('POINT(1.0 2.0)',CHAR)) as the_geom;"
        )

    feat = mysql_lyr.GetNextFeature()
    assert feat is not None

    ######################################################
    # Check the values and the precisions
    for i in range(4):
        assert feat.GetFieldIndex(fields[i]) >= 0, "field not found"
        assert (
            feat.GetField(feat.GetFieldIndex(fields[i])) == values[i]
        ), "value not right"
        assert (
            feat.GetFieldDefnRef(feat.GetFieldIndex(fields[i])).GetPrecision()
            == precision[i]
        ), "precision not right"

    mysql_ds.ReleaseResultSet(mysql_lyr)


###############################################################################
# Run test_ogrsf


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_24(mysql_ds):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " '"
        + mysql_ds.GetDescription()
        + "' tpoly"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test 64 bit FID


def test_ogr_mysql_72(mysql_ds):

    # Regular layer with 32 bit IDs
    lyr = mysql_ds.CreateLayer("ogr_mysql_72", geom_type=ogr.wkbNone)
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is None
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, "bar")
    assert lyr.CreateFeature(f) == 0
    f = lyr.GetFeature(123456789012345)
    assert f is not None

    lyr = mysql_ds.CreateLayer(
        "ogr_mysql_72", geom_type=ogr.wkbNone, options=["FID64=YES", "OVERWRITE=YES"]
    )
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, "bar")
    assert lyr.CreateFeature(f) == 0
    assert lyr.SetFeature(f) == 0

    dsn = mysql_ds.GetDescription()
    mysql_ds = None
    # Test with normal protocol
    mysql_ds = ogr.Open(dsn, update=1)
    lyr = mysql_ds.GetLayerByName("ogr_mysql_72")
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    assert f.GetFID() == 123456789012345


###############################################################################
# Test nullable


def test_ogr_mysql_25(mysql_ds):

    lyr = mysql_ds.CreateLayer("ogr_mysql_25", geom_type=ogr.wkbPoint, options=[])
    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = None

    # Error case: missing geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(f)
    assert ret != 0
    f = None

    # Error case: missing non-nullable field
    if False:  # pylint: disable=using-constant-test
        # hum mysql seems OK with unset non-nullable fields ??
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
        with gdal.quiet_errors():
            ret = lyr.CreateFeature(f)
        assert ret != 0
        f = None

    dsn = mysql_ds.GetDescription()
    mysql_ds = None
    mysql_ds = ogr.Open(dsn, update=1)
    lyr = mysql_ds.GetLayerByName("ogr_mysql_25")
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


###############################################################################
# Test default values


def test_ogr_mysql_26(mysql_ds):

    lyr = mysql_ds.CreateLayer("ogr_mysql_26", geom_type=ogr.wkbPoint, options=[])

    field_defn = ogr.FieldDefn("field_string", ogr.OFTString)
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_string_null", ogr.OFTString)
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_int", ogr.OFTInteger)
    field_defn.SetDefault("123")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_real", ogr.OFTReal)
    field_defn.SetDefault("1.23")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_nodefault", ogr.OFTInteger)
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_datetime", ogr.OFTDateTime)
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_datetime2", ogr.OFTDateTime)
    field_defn.SetDefault("'2015/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    # field_defn = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    # field_defn.SetDefault("CURRENT_DATE")
    # lyr.CreateField(field_defn)

    # field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    # field_defn.SetDefault("CURRENT_TIME")
    # lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull("field_string_null")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = None

    dsn = mysql_ds.GetDescription()
    mysql_ds = None
    mysql_ds = ogr.Open(dsn, update=1)
    lyr = mysql_ds.GetLayerByName("ogr_mysql_26")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_string"))
        .GetDefault()
        == "'a''b'"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_int"))
        .GetDefault()
        == "123"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_real"))
        .GetDefault()
        == "1.23"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_nodefault"))
        .GetDefault()
        is None
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_datetime"))
        .GetDefault()
        == "CURRENT_TIMESTAMP"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_datetime2"))
        .GetDefault()
        == "'2015/06/30 12:34:56'"
    )
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() != "CURRENT_DATE":
    #    gdaltest.post_reason('fail')
    #    print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault())
    #    return 'fail'
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    f = lyr.GetNextFeature()
    if (
        f.GetField("field_string") != "a'b"
        or f.GetField("field_int") != 123
        or f.GetField("field_real") != 1.23
        or not f.IsFieldNull("field_string_null")
        or not f.IsFieldNull("field_nodefault")
        or not f.IsFieldSet("field_datetime")
        or f.GetField("field_datetime2") != "2015/06/30 12:34:56"
    ):
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test created table indecs


@pytest.mark.usefixtures("tpoly")
def test_ogr_mysql_27(mysql_ds, mysql_is_8_or_later):

    if not mysql_is_8_or_later:
        pytest.skip()

    layer = mysql_ds.GetLayerByName("tpoly")
    if layer is None:
        pytest.skip("did not get tpoly layer")

    sql_lyr = mysql_ds.ExecuteSQL("SHOW CREATE TABLE tpoly")

    f = sql_lyr.GetNextFeature()
    field = f.GetField(1)
    res = False
    for line in field.splitlines():
        if "geometry" in line:
            if "SRID" in line:
                res = True
            else:
                res = False
    if not res:
        print("{}".format(field))
        pytest.fail("Not found SRID definition with GEOMETORY field.")
    mysql_ds.ReleaseResultSet(sql_lyr)


###############################################################################
#


def test_ogr_mysql_longlat(mysql_ds, mysql_is_8_or_later):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:4326")
    lyr = mysql_ds.CreateLayer(
        "ogr_mysql_longlat", geom_type=ogr.wkbPoint, srs=srs, options=[]
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(150 2)")
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    lyr.SetSpatialFilterRect(149.5, 1.5, 150.5, 2.5)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, geom)

    extent = lyr.GetExtent()
    expect = (150.0, 150.0, 2.0, 2.0)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail("Extents do not match")

    if mysql_is_8_or_later:
        sql_lyr = mysql_ds.ExecuteSQL("SHOW CREATE TABLE ogr_mysql_longlat")
        f = sql_lyr.GetNextFeature()
        field = f.GetField(1)
        res = False
        for line in field.splitlines():
            if "geometry" in line:
                if "SRID" in line:
                    res = True
                else:
                    res = False
        if not res:
            print("{}".format(field))
            pytest.fail("Not found SRID definition with GEOMETORY field.")
        mysql_ds.ReleaseResultSet(sql_lyr)

    lyr.SetSpatialFilterRect(-181, -91, 181, 91)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, geom)


###############################################################################
# Test writing and reading back geometries


@pytest.mark.xfail(reason="MariaDB has a known issue MDEV-21401")
def test_ogr_mysql_28(mysql_ds):

    wkts = ogrtest.get_wkt_data_series(
        with_z=True, with_m=True, with_gc=True, with_circular=True, with_surface=False
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    for i, wkt in enumerate(wkts):
        gdaltest.num_mysql_28 = i + 1
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = mysql_ds.CreateLayer(
            "ogr_mysql_28_%d" % i, geom_type=geom.GetGeometryType(), srs=srs
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)
        f = None
        #
        layer = mysql_ds.GetLayerByName("ogr_mysql_28_%d" % i)
        if layer is None:
            pytest.fail("did not get ogr_mysql_28_%d layer" % i)
        feat = layer.GetNextFeature()
        assert feat is not None
        feat = None


@pytest.mark.xfail(reason="MySQL does not support POLYHEDRALSURFACE.")
def test_ogr_mysql_29(mysql_ds):

    wkts = ogrtest.get_wkt_data_series(
        with_z=False,
        with_m=False,
        with_gc=False,
        with_circular=False,
        with_surface=True,
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    for i, wkt in enumerate(wkts):
        gdaltest.num_mysql_29 = i + 1
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = mysql_ds.CreateLayer(
            "ogr_mysql_29_%d" % i, geom_type=geom.GetGeometryType(), srs=srs
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)
        f = None
        #
        layer = mysql_ds.GetLayerByName("ogr_mysql_29_%d" % i)
        if layer is None:
            pytest.fail("did not get ogr_mysql_29_%d layer" % i)
        feat = layer.GetNextFeature()
        assert feat is not None
        feat = None


###############################################################################
# Test registering a new SRS


def test_ogr_mysql_create_new_srs(mysql_ds):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=merc +datum=WGS84")

    lyr = mysql_ds.CreateLayer("test_new_srs", srs=srs)
    assert lyr
    lyr = mysql_ds.CreateLayer("test_new_srs2", srs=srs)
    assert lyr

    dsn = mysql_ds.GetDescription()
    mysql_ds = None
    mysql_ds = ogr.Open(dsn, update=1)

    lyr = mysql_ds.GetLayerByName("test_new_srs")
    assert lyr.GetSpatialRef().ExportToProj4().startswith("+proj=merc")
