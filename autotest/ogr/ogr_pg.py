#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PostGIS driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
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

import functools
import os
import re
import sys
import threading
import time

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("PostgreSQL")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Return true if 'layer_name' is one of the reported layers of pg_ds


def ogr_pg_check_layer_in_list(ds, layer_name):

    for i in range(0, ds.GetLayerCount()):
        name = ds.GetLayer(i).GetName()
        if name == layer_name:
            return True
    return False


def ogr_pg_table_exists(ds, schema, table):

    with ds.ExecuteSQL(
        f"SELECT table_name FROM information_schema.tables WHERE table_name = 'tpoly' AND table_schema='{schema}'"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        return f is not None


def current_schema(ds):
    with ds.ExecuteSQL("SELECT current_schema()") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        return f["current_schema"]


def reconnect(ds, update=True, open_options=None):

    dsn = ds.GetDescription()

    ds = None

    flags = gdal.OF_VECTOR
    if update:
        flags |= gdal.OF_UPDATE

    if open_options is None:
        open_options = {}

    return gdal.OpenEx(dsn, flags, open_options=open_options)


def clean_identifier(x):
    return (
        x.replace("[", "_").replace("]", "").replace("-", "_").replace("=", "_").lower()
    )


#
# To create the required PostGIS instance do something like:
#
#  $ createdb autotest
#  $ createlang plpgsql autotest
#  $ psql autotest -c 'create extension postgis'
#

###############################################################################
# Module-scoped database connection to verify connection parameters and
# support fixtures that describe database capabilities. This fixture should
# generally not be used directly by tests.


@pytest.fixture(scope="module")
def pg_autotest_ds():

    val = gdal.GetConfigOption("OGR_PG_CONNECTION_STRING", None)
    if val is not None:
        pg_connection_string = val
    else:
        pg_connection_string = "dbname=autotest"

    try:
        ds = ogr.Open("PG:" + pg_connection_string, update=1)
    except RuntimeError:
        ds = None

    if ds is None:
        if val is None:
            pytest.skip(
                f"OGR_PG_CONNECTION_STRING not specified; Postgres is not available using default connection string {pg_connection_string}"
            )
        else:
            pytest.skip(
                f"Postgres is not available using supplied OGR_PG_CONNECTION_STRING {pg_connection_string}"
            )

    return ds


###############################################################################
# Test-scoped database connection that sets the active schema to a temporary
# schema that will be destroyed at the end of the test.
# By default, tests using this fixture will run both with both
# PG_USE_POSTGIS=YES and PG_USE_POSTGIS=NO. To indicate that a test should only
# run with a value of YES or NO, the `only_with_postgis` and `only_without_postgis`
# decorators can be used.


@pytest.fixture()
def pg_ds(request, pg_autotest_ds, use_postgis):

    schema = clean_identifier(request.node.name)
    pg_autotest_ds.ExecuteSQL(f"DROP SCHEMA IF EXISTS {schema} CASCADE")
    pg_autotest_ds.ExecuteSQL(f"CREATE SCHEMA {schema}")

    dsn = pg_autotest_ds.GetDescription() + " schemas=" + schema
    ds = ogr.Open(dsn)

    assert ds is not None

    yield ds

    for lyr in ds:
        lyr.ResetReading()  # prevent blocking of DROP SCHEMA below

    ds.ExecuteSQL(f"DROP SCHEMA {schema} CASCADE")


@pytest.fixture(scope="module", params=[True, False], ids=["postgis", "no-postgis"])
def use_postgis(request, pg_has_postgis):

    postgis = request.param

    if postgis and not pg_has_postgis:
        pytest.skip("Test requires PostGIS")

    with gdal.config_option("PG_USE_POSTGIS", "YES" if postgis else "NO"):
        yield postgis


def only_with_postgis(func):
    @pytest.mark.parametrize("use_postgis", [True], ids=["postgis"], indirect=True)
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)

    return wrapper


def only_without_postgis(func):
    @pytest.mark.parametrize("use_postgis", [False], ids=["no-postgis"], indirect=True)
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)

    return wrapper


###############################################################################
# Fixture providing a second temporary schema to be used by a single test.
# This is only needed for tests that write to multiple schemas.


@pytest.fixture()
def tmp_schema(pg_ds, request):

    schema = clean_identifier(request.node.name) + "_tmp"

    pg_ds.ExecuteSQL(f'CREATE SCHEMA "{schema}"')
    yield schema

    pg_ds.ExecuteSQL(f'DROP SCHEMA "{schema}" CASCADE')


###############################################################################
# Fixture that temporarily clears the spatial_ref_sys table, for tests that
# must manipulate its contents.


@pytest.fixture()
def empty_spatial_ref_sys(pg_ds):

    pg_ds.ExecuteSQL(
        "CREATE TABLE spatial_ref_sys_bak AS SELECT * FROM spatial_ref_sys"
    )

    pg_ds.ExecuteSQL("TRUNCATE spatial_ref_sys")

    yield

    pg_ds.ExecuteSQL("TRUNCATE spatial_ref_sys")
    pg_ds.ExecuteSQL("INSERT INTO spatial_ref_sys SELECT * FROM spatial_ref_sys_bak")
    pg_ds.ExecuteSQL("DROP TABLE spatial_ref_sys_bak")


###############################################################################
# Fixtures describing database capabilities
###############################################################################


@pytest.fixture(scope="module")
def pg_version(pg_autotest_ds):
    with pg_autotest_ds.ExecuteSQL("SELECT version()") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        v = feat.GetFieldAsString("version")

    # return of version() is something like "PostgreSQL 12.0[rcX|betaX] ...otherstuff..."

    tokens = v.split(" ")
    assert len(tokens) >= 2
    # First token is "PostgreSQL" (or some enterprise DB alternative name)
    v = tokens[1]
    pos = v.find("beta")
    if pos > 0:
        v = v[0:pos]
    else:
        pos = v.find("rc")
        if pos > 0:
            v = v[0:pos]

    return tuple([int(x) for x in v.split(".")])


@pytest.fixture(scope="module")
def pg_postgis_version(pg_autotest_ds):
    sql_lyr = pg_autotest_ds.ExecuteSQL("SELECT postgis_version()")
    if sql_lyr is not None:
        feat = sql_lyr.GetNextFeature()
        version_str = feat.GetFieldAsString("postgis_version")

        pos = version_str.find(" ")
        if pos > 0:
            version_str = version_str[0:pos]

        pg_autotest_ds.ReleaseResultSet(sql_lyr)

        return tuple([int(x) for x in version_str.split(".")])


@pytest.fixture(scope="module")
def pg_postgis_schema(pg_autotest_ds):
    with pg_autotest_ds.ExecuteSQL(
        "SELECT table_schema FROM information_schema.tables WHERE table_name = 'geometry_columns'"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        if feat is not None:
            return feat["table_schema"]


# Indicate whether the database has PostGIS installed.
# This does not mean that GDAL is using PostGIS; for that, see
# the use_postgis fixture.
@pytest.fixture(scope="module")
def pg_has_postgis(pg_postgis_version):

    return pg_postgis_version is not None


@pytest.fixture(scope="module")
def pg_quote_with_E(pg_autotest_ds):
    with gdal.quiet_errors():
        sql_lyr = pg_autotest_ds.ExecuteSQL("SHOW standard_conforming_strings")
        if sql_lyr is None:
            return False

    pg_autotest_ds.ReleaseResultSet(sql_lyr)

    return True


###############################################################################
# Create table from data/poly.shp


@pytest.fixture()
def tpoly(pg_ds, poly_feat):

    ######################################################
    # Create Layer
    pg_lyr = pg_ds.CreateLayer("tpoly", options=["DIM=3"])

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        pg_lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
            ("REALLIST", ogr.OFTRealList),
        ],
    )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())

    for feat in poly_feat:

        dst_feat.SetFrom(feat)
        pg_lyr.CreateFeature(dst_feat)

    pg_lyr.ResetReading()
    pg_lyr = None
    pg_ds = None

    return


testgeoms = (
    ("POINT (10 20 5 5)", "POINT ZM (10 20 5 5)"),
    (
        "LINESTRING (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)",
        "LINESTRING ZM (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)",
    ),
    (
        "POLYGON ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))",
        "POLYGON ZM ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))",
    ),
    ("MULTIPOINT (10 20 5 5,30 30 7 7)", "MULTIPOINT ZM ((10 20 5 5),(30 30 7 7))"),
    (
        "MULTILINESTRING ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))",
        "MULTILINESTRING ZM ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))",
    ),
    (
        "MULTIPOLYGON(((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))",
        "MULTIPOLYGON ZM (((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))",
    ),
    (
        "GEOMETRYCOLLECTION(POINT(2 3 11 101),LINESTRING(2 3 12 102,3 4 13 103))",
        "GEOMETRYCOLLECTION ZM (POINT ZM (2 3 11 101),LINESTRING ZM (2 3 12 102,3 4 13 103))",
    ),
    (
        "TRIANGLE ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))",
        "TRIANGLE ZM ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))",
    ),
    (
        "TIN (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))",
        "TIN ZM (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))",
    ),
    (
        "POLYHEDRALSURFACE (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))",
        "POLYHEDRALSURFACE ZM (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))",
    ),
)


@pytest.fixture()
def testgeom(pg_ds):

    schema = current_schema(pg_ds)

    pg_ds.ExecuteSQL("CREATE TABLE testgeom (ogc_fid integer)")

    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','testgeom','wkb_geometry',-1,'GEOMETRY',4)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    for i, geom in enumerate(testgeoms):
        pg_ds.ExecuteSQL("INSERT INTO testgeom (ogc_fid,wkb_geometry) \
                                    VALUES (%d,GeomFromEWKT('%s'))" % (i, geom[0]))


###############################################################################
# Create table with all data types


@pytest.fixture()
def datatypetest(pg_ds, pg_has_postgis, pg_quote_with_E):

    ######################################################
    # Create Table
    lyr = pg_ds.CreateLayer("datatypetest")

    ######################################################
    # Setup Schema
    # ogrtest.quick_create_layer_def( lyr, None )

    ######################################################
    # add some custom date fields.
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_numeric numeric")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_numeric5 numeric(5)")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_numeric5_3 numeric(5,3)")
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_bool bool' )
    fld = ogr.FieldDefn("my_bool", ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int2 int2' )
    fld = ogr.FieldDefn("my_int2", ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_int4 int4")
    lyr.CreateField(ogr.FieldDefn("my_int8", ogr.OFTInteger64))
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float4 float4' )
    fld = ogr.FieldDefn("my_float4", ogr.OFTReal)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_float8 float8")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_real real")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_char char")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_varchar character varying")
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_varchar10 character varying(10)"
    )
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_text text")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_bytea bytea")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_time time")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_date date")
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_timestamp timestamp without time zone"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_timestamptz timestamp with time zone"
    )
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_chararray char(1)[]")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_textarray text[]")
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_varchararray character varying[]"
    )
    fld = ogr.FieldDefn("my_int2array", ogr.OFTIntegerList)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_int4array int4[]")
    lyr.CreateField(ogr.FieldDefn("my_int8array", ogr.OFTInteger64List))
    fld = ogr.FieldDefn("my_float4array", ogr.OFTRealList)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_float8array float8[]")
    pg_ds.ExecuteSQL("ALTER TABLE datatypetest ADD COLUMN my_numericarray numeric[]")
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_numeric5array numeric(5)[]"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE datatypetest ADD COLUMN my_numeric5_3array numeric(5,3)[]"
    )
    fld = ogr.FieldDefn("my_boolarray", ogr.OFTIntegerList)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    ######################################################
    # Create a populated records.

    if pg_has_postgis:
        geom_str = "GeomFromEWKT('POINT(10 20)')"
    else:
        geom_str = "'\\\\001\\\\001\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000$@\\\\000\\\\000\\\\000\\\\000\\\\000\\\\0004@'"
        if pg_quote_with_E:
            geom_str = "E" + geom_str
    sql = "INSERT INTO datatypetest ( my_numeric, my_numeric5, my_numeric5_3, my_bool, my_int2, "
    sql += "my_int4, my_int8, my_float4, my_float8, my_real, my_char, my_varchar, "
    sql += "my_varchar10, my_text, my_bytea, my_time, my_date, my_timestamp, my_timestamptz, "
    sql += "my_chararray, my_textarray, my_varchararray, my_int2array, my_int4array, "
    sql += "my_int8array, my_float4array, my_float8array, my_numericarray, my_numeric5array, my_numeric5_3array, my_boolarray, wkb_geometry) "
    sql += "VALUES ( 1.2, 12345, 0.123, 'T', 12345, 12345678, 1234567901234, 0.123, "
    sql += "0.12345678, 0.876, 'a', 'ab', 'varchar10 ', 'abc', 'xyz', '12:34:56', "
    sql += "'2000-01-01', '2000-01-01 00:00:00', '2000-01-01 00:00:00+00', "
    sql += "'{a,b}', "
    sql += "'{aa,bb}', '{cc,dd}', '{100,200}', '{100,200}', '{1234567901234}', "
    sql += (
        "'{100.1,200.1}', '{100.12,200.12}', ARRAY[100.12,200.12], ARRAY[10,20], ARRAY[10.12,20.12], '{1,0}', "
        + geom_str
        + " )"
    )
    pg_ds.ExecuteSQL(sql)


######################################################
# Check capabilities


@pytest.mark.usefixtures("tpoly")
def test_capabilities(pg_ds, use_postgis):

    pg_lyr = pg_ds.GetLayer("tpoly")
    assert pg_lyr.GetDataset().GetDescription() == pg_ds.GetDescription()

    if use_postgis:
        assert pg_lyr.TestCapability(ogr.OLCFastSpatialFilter)
        assert pg_lyr.TestCapability(ogr.OLCFastGetExtent)
    else:
        assert not pg_lyr.TestCapability(ogr.OLCFastSpatialFilter)
        assert not pg_lyr.TestCapability(ogr.OLCFastGetExtent)

    assert pg_lyr.TestCapability(ogr.OLCRandomRead)
    assert pg_lyr.TestCapability(ogr.OLCFastFeatureCount)
    assert pg_lyr.TestCapability(ogr.OLCFastSetNextByIndex)
    try:
        ogr.OLCStringsAsUTF8
        assert pg_lyr.TestCapability(ogr.OLCStringsAsUTF8)
    except Exception:
        pass
    assert pg_lyr.TestCapability(ogr.OLCSequentialWrite)
    assert pg_lyr.TestCapability(ogr.OLCCreateField)
    assert pg_lyr.TestCapability(ogr.OLCRandomWrite)
    assert pg_lyr.TestCapability(ogr.OLCTransactions)


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_delete(pg_ds):

    schema = current_schema(pg_ds)

    assert ogr_pg_table_exists(pg_ds, schema, "tpoly")

    pg_ds.ExecuteSQL("DELLAYER:tpoly")

    assert not ogr_pg_table_exists(pg_ds, schema, "tpoly")


@only_without_postgis
def test_ogr_pg_connection_string_format(pg_ds):

    for pattern in (" = ", "= ", " =", "  =    "):
        dsn_mod = re.sub(r"(?<=[\w])=(?=[\w])", pattern, pg_ds.GetDescription())

        ds = ogr.Open(dsn_mod)

        assert ds is not None


###############################################################################
# Test reading a layer extent


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_19(pg_ds, use_postgis):

    layer = pg_ds.GetLayerByName("tpoly")
    assert layer is not None, "did not get tpoly layer"

    extent = layer.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail("Extents do not match")

    estimated_extent = layer.GetExtent(force=0)
    if use_postgis:
        # Better testing needed ?
        assert estimated_extent != (0, 0, 0, 0)
    else:
        # The OGRLayer default implementation in force = 0 returns error
        assert estimated_extent == (0, 0, 0, 0)


###############################################################################
# Test reading a SQL result layer extent


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_19_2(pg_ds):

    sql_lyr = pg_ds.ExecuteSQL("select * from tpoly")

    extent = sql_lyr.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    assert max(minx, maxx, miny, maxy) <= 0.0001, "Extents do not match"

    pg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Verify that stuff we just wrote is still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_3(pg_ds, poly_feat):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    assert pg_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(pg_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(pg_lyr, "eas_id", expect)

        assert pg_lyr.GetFeatureCount() == 5

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        orig_geom = orig_feat.GetGeometryRef().Clone()
        orig_geom = ogr.ForceTo(orig_geom, ogr.wkbPolygon25D)
        read_feat = pg_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(read_feat, orig_geom, max_error=0.001)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_4(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())
    wkt_list = ["10", "2", "1", "3d_1", "4", "5", "6"]

    for item in wkt_list:

        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        dst_feat.SetFID(-1)
        pg_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        pg_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = pg_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, geom)

    pg_lyr.ResetReading()  # to close implicit transaction


###############################################################################
# Test ExecuteSQL() results layers without geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_5(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")
    for _ in range(2):
        dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())
        pg_lyr.CreateFeature(dst_feat)

    expect = [None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158]

    with pg_ds.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    ) as sql_lyr:

        assert sql_lyr.GetFeatureCount() == 11

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ExecuteSQL() results layers with geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_6(pg_ds):

    with pg_ds.ExecuteSQL("select * from tpoly where prfedea = '35043413'") as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "prfedea", ["35043413"])

        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        geom = feat_read.GetGeometryRef()
        geom = ogr.ForceTo(geom, ogr.wkbPolygon)

        ogrtest.check_feature_geometry(
            geom,
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
def test_ogr_pg_7(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    pg_lyr.SetAttributeFilter(None)

    with ogrtest.spatial_filter(pg_lyr, "LINESTRING(479505 4763195,480526 4762819)"):

        assert pg_lyr.GetFeatureCount() == 1

        ogrtest.check_features_against_list(pg_lyr, "eas_id", [158])

        with ogrtest.attribute_filter(pg_lyr, "eas_id = 158"):

            assert pg_lyr.GetFeatureCount() == 1


###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_8(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())

    dst_feat.SetField("PRFEDEA", "CrazyKey")
    dst_feat.SetField("SHORTNAME", "Crazy\"'Long")
    pg_lyr.CreateFeature(dst_feat)

    pg_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = pg_lyr.GetNextFeature()

    assert feat_read is not None, "creating crazy feature failed!"

    assert (
        feat_read.GetField("shortname") == "Crazy\"'L"
    ), "Value not properly escaped or truncated:" + feat_read.GetField("shortname")


###############################################################################
# Verify inplace update of a feature with SetFeature().


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_9(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    pg_lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    feat = pg_lyr.GetNextFeature()
    pg_lyr.SetAttributeFilter(None)

    feat.SetField("SHORTNAME", "Reset")

    point = ogr.Geometry(ogr.wkbPoint25D)
    point.SetPoint(0, 5, 6, 7)
    feat.SetGeometryDirectly(point)

    assert pg_lyr.SetFeature(feat) == 0, "SetFeature() method failed."

    fid = feat.GetFID()

    feat = pg_lyr.GetFeature(fid)
    assert feat is not None, "GetFeature(%d) failed." % fid

    shortname = feat.GetField("SHORTNAME")
    assert shortname[:5] == "Reset", (
        "SetFeature() did not update SHORTNAME, got %s." % shortname
    )

    ogrtest.check_feature_geometry(feat, "POINT(5 6 7)")

    feat.SetGeometryDirectly(None)

    assert pg_lyr.SetFeature(feat) == 0, "SetFeature() method failed."

    feat = pg_lyr.GetFeature(fid)
    assert (
        feat.GetGeometryRef() is None
    ), "Geometry update failed. null geometry expected"

    feat.SetFieldNull("SHORTNAME")
    pg_lyr.SetFeature(feat)
    feat = pg_lyr.GetFeature(fid)
    assert feat.IsFieldNull("SHORTNAME"), "SHORTNAME update failed. null value expected"

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert (
        pg_lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."


###############################################################################
# Verify inplace update of a feature with UpdateFeature().


def test_ogr_pg_update_feature(pg_ds):

    lyr = pg_ds.CreateLayer("test_ogr_pg_update_feature")
    assert lyr.TestCapability(ogr.OLCUpdateFeature) == 1

    lyr.CreateField(ogr.FieldDefn("test_str", ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat["test_str"] = "foo"
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(feat) == ogr.OGRERR_NONE
    fid = feat.GetFID()

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(fid)
    feat.SetField("test_str", "bar")

    assert (
        lyr.UpdateFeature(
            feat, [lyr.GetLayerDefn().GetFieldIndex("test_str")], [], False
        )
        == ogr.OGRERR_NONE
    )

    feat = lyr.GetFeature(fid)
    assert feat is not None
    assert feat["test_str"] == "bar"
    assert feat.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(fid)

    assert lyr.UpdateFeature(feat, [], [0], False) == ogr.OGRERR_NONE

    feat = lyr.GetFeature(fid)
    assert feat is not None
    assert feat["test_str"] == "bar"
    assert feat.GetGeometryRef() is None

    feat = ogr.Feature(lyr.GetLayerDefn())
    with gdal.quiet_errors():
        assert lyr.UpdateFeature(feat, [], [0], False) != ogr.OGRERR_NONE

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(12345678)

    assert lyr.UpdateFeature(feat, [], [0], False) == ogr.OGRERR_NON_EXISTING_FEATURE


###############################################################################
# Verify that DeleteFeature() works properly.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_10(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    pg_lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    feat = pg_lyr.GetNextFeature()
    pg_lyr.SetAttributeFilter(None)

    fid = feat.GetFID()

    assert pg_lyr.DeleteFeature(fid) == 0, "DeleteFeature() method failed."

    pg_lyr.SetAttributeFilter("PRFEDEA = '35041413'")
    feat = pg_lyr.GetNextFeature()
    pg_lyr.SetAttributeFilter(None)

    assert feat is None, "DeleteFeature() seems to have had no effect."

    # Test deleting non-existing feature
    assert (
        pg_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."


###############################################################################
# Create table from data/poly.shp in INSERT mode.


def test_ogr_pg_11(pg_ds, use_postgis, poly_feat):

    with gdal.config_option("PG_USE_COPY", "NO"):

        ######################################################
        # Create Layer
        pgc_lyr = pg_ds.CreateLayer("tpolycopy", options=["DIM=3"])

        ######################################################
        # Setup Schema
        ogrtest.quick_create_layer_def(
            pgc_lyr,
            [
                ("AREA", ogr.OFTReal),
                ("EAS_ID", ogr.OFTInteger),
                ("PRFEDEA", ogr.OFTString),
                ("SHORTNAME", ogr.OFTString, 8),
            ],
        )

        ######################################################
        # Copy in poly.shp

        dst_feat = ogr.Feature(feature_def=pgc_lyr.GetLayerDefn())

        for feat in poly_feat:

            dst_feat.SetFrom(feat)
            pgc_lyr.CreateFeature(dst_feat)

    ###############################################################################
    # Verify that stuff we just wrote is still OK.

    pgc_lyr.ResetReading()
    pgc_lyr.SetAttributeFilter(None)

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        orig_geom = orig_feat.GetGeometryRef().Clone()
        if use_postgis:
            orig_geom = ogr.ForceTo(orig_geom, ogr.wkbPolygon25D)
        read_feat = pgc_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(read_feat, orig_geom, max_error=0.001)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )

    pgc_lyr.ResetReading()  # to close implicit transaction


###############################################################################
# Create a table with some date fields.


@pytest.fixture()
def datetest(pg_ds):

    ######################################################
    # Create Table
    lyr = pg_ds.CreateLayer("datetest")

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        lyr,
        [
            ("ogrdate", ogr.OFTDate),
            ("ogrtime", ogr.OFTTime),
            ("ogrdatetime", ogr.OFTDateTime),
        ],
    )

    ######################################################
    # add some custom date fields.
    pg_ds.ExecuteSQL("ALTER TABLE datetest ADD COLUMN tsz timestamp with time zone")
    pg_ds.ExecuteSQL("ALTER TABLE datetest ADD COLUMN ts timestamp without time zone")
    pg_ds.ExecuteSQL("ALTER TABLE datetest ADD COLUMN dt date")
    pg_ds.ExecuteSQL("ALTER TABLE datetest ADD COLUMN tm time")

    ######################################################
    # Create a populated records.
    pg_ds.ExecuteSQL(
        "INSERT INTO datetest ( ogrdate, ogrtime, ogrdatetime, tsz, ts, dt, tm) VALUES ( '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05' )"
    )

    ###############################################################################
    # Verify that stuff we just wrote is still OK.
    # Fetch in several timezones to test our timezone processing.


@pytest.mark.usefixtures("datetest")
def test_ogr_pg_14(pg_ds):

    ds = reconnect(pg_ds, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName("datetest")

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("ogrdatetime") == "2005/10/12 15:41:33+00"
    assert feat.GetFieldAsString("ogrdate") == "2005/10/12"
    assert feat.GetFieldAsString("ogrtime") == "10:41:33"
    assert feat.GetFieldAsString("tsz") == "2005/10/12 15:41:33+00"
    assert feat.GetFieldAsString("ts") == "2005/10/12 10:41:33"
    assert feat.GetFieldAsString("dt") == "2005/10/12"
    assert feat.GetFieldAsString("tm") == "10:41:33"

    sql_lyr = ds.ExecuteSQL(
        "select * from pg_timezone_names where name = 'Canada/Newfoundland'"
    )
    if sql_lyr is None:
        has_tz = True
    else:
        has_tz = sql_lyr.GetFeatureCount() != 0
        ds.ReleaseResultSet(sql_lyr)

    if has_tz:
        ds.ExecuteSQL('set timezone to "Canada/Newfoundland"')

        lyr.ResetReading()

        feat = lyr.GetNextFeature()

        assert feat.GetFieldAsString("ogrdatetime") == "2005/10/12 13:11:33-0230"
        assert feat.GetFieldAsString("tsz") == "2005/10/12 13:11:33-0230"
        assert feat.GetFieldAsString("ts") == "2005/10/12 10:41:33"
        assert feat.GetFieldAsString("dt") == "2005/10/12"
        assert feat.GetFieldAsString("tm") == "10:41:33"

    ds.ExecuteSQL('set timezone to "+5"')

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("ogrdatetime") == "2005/10/12 20:41:33+05"
    assert feat.GetFieldAsString("tsz") == "2005/10/12 20:41:33+05"


###############################################################################
# Test very large query.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_15(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")

    expect = [169]

    query = "eas_id = 169"

    for ident in range(1000):
        query = query + (" or eas_id = %d" % (ident + 1000))

    with ogrtest.attribute_filter(pg_lyr, query):
        ogrtest.check_features_against_list(pg_lyr, "eas_id", expect)


###############################################################################
# Test very large statement.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_16(pg_ds):

    expect = [169]

    query = "eas_id = 169"

    for ident in range(1000):
        query = query + (" or eas_id = %d" % (ident + 1000))

    statement = "select eas_id from tpoly where " + query

    with pg_ds.ExecuteSQL(statement) as lyr:

        ogrtest.check_features_against_list(lyr, "eas_id", expect)


###############################################################################
# Test requesting a non-existent table by name (bug 1480).


def test_ogr_pg_17(pg_ds):

    count = pg_ds.GetLayerCount()
    try:
        layer = pg_ds.GetLayerByName("JunkTableName")
    except Exception:
        layer = None

    assert layer is None, "got layer for non-existent table!"

    assert count == pg_ds.GetLayerCount(), "layer count changed unexpectedly."


###############################################################################
# Test getting a layer by name that was not previously a layer.


@only_with_postgis
def test_ogr_pg_18(pg_ds, pg_postgis_schema):

    count = pg_ds.GetLayerCount()
    layer = pg_ds.GetLayerByName(f"{pg_postgis_schema}.geometry_columns")
    assert layer is not None, "did not get geometry_columns layer"

    assert count + 1 == pg_ds.GetLayerCount(), "layer count unexpectedly unchanged."


###############################################################################
# Test reading 4-dim geometry in EWKT format


@only_with_postgis
@pytest.mark.usefixtures("testgeom")
def test_ogr_pg_20(pg_ds):

    pg_ds = reconnect(pg_ds, update=1)

    layer = pg_ds.GetLayerByName("testgeom")
    assert layer is not None, "did not get testgeom layer"

    # Test updating the geometries
    for i in range(len(testgeoms)):
        feat = layer.GetFeature(i)
        assert layer.SetFeature(feat) == gdal.CE_None

    # Test we get them back as expected
    for i, geoms in enumerate(testgeoms):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        assert geom is not None, "did not get geometry, expected %s" % geoms[1]
        wkt = geom.ExportToIsoWkt()
        feat = None

        assert wkt == geoms[1], "WKT do not match: expected %s, got %s" % (
            geoms[1],
            wkt,
        )

    layer = None


###############################################################################
# Test reading 4-dimension geometries in EWKB format


@only_with_postgis
@pytest.mark.usefixtures("testgeom")
def test_ogr_pg_21(pg_ds):

    layer = pg_ds.ExecuteSQL("SELECT wkb_geometry FROM testgeom")
    assert layer is not None, "did not get testgeom layer"

    for feat in layer:
        geom = feat.GetGeometryRef()
        if (
            ogr.GT_HasZ(geom.GetGeometryType()) == 0
            or ogr.GT_HasM(geom.GetGeometryType()) == 0
        ):
            feat = None
            pg_ds.ReleaseResultSet(layer)
            layer = None
            pytest.fail("expected feature with type >3000")

    pg_ds.ReleaseResultSet(layer)
    layer = None


###############################################################################
# Check if the sub geometries of TIN and POLYHEDRALSURFACE are valid


@only_with_postgis
@pytest.mark.usefixtures("testgeom")
def test_ogr_pg_21_subgeoms(pg_ds):

    subgeom_PS = [
        "POLYGON ZM ((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0))",
        "POLYGON ZM ((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0))",
        "POLYGON ZM ((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0))",
        "POLYGON ZM ((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0))",
        "POLYGON ZM ((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0))",
        "POLYGON ZM ((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0))",
    ]

    subgeom_TIN = [
        "TRIANGLE ZM ((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0))",
        "TRIANGLE ZM ((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0))",
    ]

    layer = pg_ds.GetLayerByName("testgeom")
    for i in range(8, 10):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        assert geom is not None, "did not get the expected geometry"
        if geom.GetGeometryName() == "POLYHEDRALSURFACE":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                assert (
                    subgeom_wkt == subgeom_PS[j]
                ), "did not get the expected subgeometry, expected %s" % (subgeom_PS[j])
        if geom.GetGeometryName() == "TIN":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                assert (
                    subgeom_wkt == subgeom_TIN[j]
                ), "did not get the expected subgeometry, expected %s" % (
                    subgeom_TIN[j]
                )
        feat = None


###############################################################################
# Check if the 3d geometries of TIN, Triangle and POLYHEDRALSURFACE are valid


@only_with_postgis
def test_ogr_pg_21_3d_geometries(pg_ds):

    schema = current_schema(pg_ds)

    pg_ds.ExecuteSQL("CREATE TABLE zgeoms (field_no integer)")
    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','zgeoms','wkb_geometry',-1,'GEOMETRY',3)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)
    wkt_list = [
        "POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))",
        "TIN (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))",
        "TRIANGLE ((48 36 84,32 54 64,86 11 54,48 36 84))",
    ]

    wkt_expected = [
        "POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))",
        "TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))",
        "TRIANGLE Z ((48 36 84,32 54 64,86 11 54,48 36 84))",
    ]

    for i in range(0, 3):
        pg_ds.ExecuteSQL(
            "INSERT INTO zgeoms (field_no, wkb_geometry) VALUES (%d,GeomFromEWKT('%s'))"
            % (i, wkt_list[i])
        )

    dsn = pg_ds.GetDescription()
    pg_ds = None
    pg_ds = ogr.Open(dsn, update=1)
    assert pg_ds is not None, "Cannot open the dataset"

    layer = pg_ds.GetLayerByName("zgeoms")
    assert layer is not None, "No layer received"

    for i in range(0, 3):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()

        wkt = geom.ExportToIsoWkt()

        assert wkt == wkt_expected[i], "Unexpected WKT, expected %s and got %s" % (
            wkt_expected[i],
            wkt,
        )


###############################################################################
# Create table from data/poly.shp under specified SCHEMA
# This test checks if schema support and schema name quoting works well.


def test_ogr_pg_22(pg_ds, tmp_schema, poly_feat):

    layer_name = tmp_schema + ".tpoly"

    ######################################################
    # Create Layer
    pg_lyr = pg_ds.CreateLayer(layer_name, options={"DIM": 3, "SCHEMA": tmp_schema})

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        pg_lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
        ],
    )

    ######################################################
    # Copy 3 features from the poly.shp

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())

    # Insert 3 features only
    for ident in range(0, 3):
        feat = poly_feat[ident]
        dst_feat.SetFrom(feat)
        pg_lyr.CreateFeature(dst_feat)

    # Test if test layer under custom schema is listed

    found = ogr_pg_check_layer_in_list(pg_ds, layer_name)

    assert found is not False, "layer from schema '" + tmp_schema + "' not listed"


###############################################################################


def check_value_23(layer_defn, feat):

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5"))
    assert (
        field_defn.GetWidth() == 5
        and field_defn.GetPrecision() == 0
        and field_defn.GetType() == ogr.OFTInteger
    ), "Wrong field defn for my_numeric5 : %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5_3"))
    assert (
        field_defn.GetWidth() == 5
        and field_defn.GetPrecision() == 3
        and field_defn.GetType() == ogr.OFTReal
    ), "Wrong field defn for my_numeric5_3 : %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_varchar10"))
    assert (
        field_defn.GetWidth() == 10 and field_defn.GetPrecision() == 0
    ), "Wrong field defn for my_varchar10 : %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_bool"))
    assert (
        field_defn.GetWidth() == 1
        and field_defn.GetType() == ogr.OFTInteger
        and field_defn.GetSubType() == ogr.OFSTBoolean
    ), "Wrong field defn for my_bool : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_boolarray"))
    assert (
        field_defn.GetType() == ogr.OFTIntegerList
        and field_defn.GetSubType() == ogr.OFSTBoolean
    ), "Wrong field defn for my_boolarray : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2"))
    assert (
        field_defn.GetType() == ogr.OFTInteger
        and field_defn.GetSubType() == ogr.OFSTInt16
    ), "Wrong field defn for my_int2 : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4"))
    assert (
        field_defn.GetType() == ogr.OFTReal
        and field_defn.GetSubType() == ogr.OFSTFloat32
    ), "Wrong field defn for my_float4 : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2array"))
    assert (
        field_defn.GetType() == ogr.OFTIntegerList
        and field_defn.GetSubType() == ogr.OFSTInt16
    ), "Wrong field defn for my_int2array : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4array"))
    assert (
        field_defn.GetType() == ogr.OFTRealList
        and field_defn.GetSubType() == ogr.OFSTFloat32
    ), "Wrong field defn for my_float4array : %d, %d, %d, %d" % (
        field_defn.GetWidth(),
        field_defn.GetPrecision(),
        field_defn.GetType(),
        field_defn.GetSubType(),
    )

    if (
        feat.my_numeric != pytest.approx(1.2, abs=1e-8)
        or feat.my_numeric5 != 12345
        or feat.my_numeric5_3 != 0.123
        or feat.my_bool != 1
        or feat.my_int2 != 12345
        or feat.my_int4 != 12345678
        or feat.my_int8 != 1234567901234
        or feat.my_float4 != pytest.approx(0.123, abs=1e-8)
        or feat.my_float8 != 0.12345678
        or feat.my_real != pytest.approx(0.876, abs=1e-6)
        or feat.my_char != "a"
        or feat.my_varchar != "ab"
        or feat.my_varchar10 != "varchar10 "
        or feat.my_text != "abc"
        or feat.GetFieldAsString("my_bytea") != "78797A"
        or feat.GetFieldAsString("my_time") != "12:34:56"
        or feat.GetFieldAsString("my_date") != "2000/01/01"
        or (
            feat.GetFieldAsString("my_timestamp") != "2000/01/01 00:00:00"
            and feat.GetFieldAsString("my_timestamp") != "2000/01/01 00:00:00+00"
        )
        or (
            layer_defn.GetFieldIndex("my_timestamptz") >= 0
            and feat.GetFieldAsString("my_timestamptz") != "2000/01/01 00:00:00+00"
        )
        or feat.GetFieldAsString("my_chararray") != "(2:a,b)"
        or feat.GetFieldAsString("my_textarray") != "(2:aa,bb)"
        or feat.GetFieldAsString("my_varchararray") != "(2:cc,dd)"
        or feat.GetFieldAsString("my_int2array") != "(2:100,200)"
        or feat.GetFieldAsString("my_int4array") != "(2:100,200)"
        or feat.my_int8array != [1234567901234]
        or feat.GetFieldAsString("my_boolarray") != "(2:1,0)"
        or feat.my_float4array[0] != pytest.approx(100.1, abs=1e-6)
        or feat.my_float8array[0] != pytest.approx(100.12, abs=1e-8)
        or feat.my_numericarray[0] != pytest.approx(100.12, abs=1e-8)
        or feat.my_numeric5array[0] != pytest.approx(10, abs=1e-8)
        or feat.my_numeric5_3array[0] != pytest.approx(10.12, abs=1e-8)
    ):
        feat.DumpReadable()
        pytest.fail("Wrong values")

    geom = feat.GetGeometryRef()
    assert geom is not None, "geom is none"

    wkt = geom.ExportToWkt()
    assert wkt == "POINT (10 20)", "Wrong WKT :" + wkt


###############################################################################
# Test with PG: connection


@pytest.mark.usefixtures("datatypetest")
def test_ogr_pg_24(pg_ds):

    ds = reconnect(pg_ds, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName("datatypetest")

    feat = lyr.GetNextFeature()
    check_value_23(lyr.GetLayerDefn(), feat)


###############################################################################
# Test with PG: connection and SELECT query


@pytest.mark.usefixtures("datatypetest")
def test_ogr_pg_25(pg_ds):

    ds = reconnect(pg_ds, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    sql_lyr = ds.ExecuteSQL("select * from datatypetest")

    feat = sql_lyr.GetNextFeature()
    check_value_23(sql_lyr.GetLayerDefn(), feat)

    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Duplicate all data types in INSERT mode


@pytest.mark.usefixtures("datatypetest")
@pytest.mark.parametrize("use_copy", ("YES", "NO"), ids=lambda x: f"PG_USE_COPY={x}")
def test_ogr_pg_28(use_copy, pg_ds):

    with gdal.config_option("PG_USE_COPY", use_copy):

        ds = reconnect(pg_ds, update=1)

        ds.ExecuteSQL('set timezone to "UTC"')

        src_lyr = ds.GetLayerByName("datatypetest")

        dst_lyr = ds.CreateLayer("datatypetest2")

        src_lyr.ResetReading()

        for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
            field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
            dst_lyr.CreateField(field_defn)

        dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())

        feat = src_lyr.GetNextFeature()
        assert feat is not None

        dst_feat.SetFrom(feat)
        assert dst_lyr.CreateFeature(dst_feat) == 0, "CreateFeature failed."

    ds = reconnect(pg_ds, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName("datatypetest2")

    # my_timestamp has now a time zone...
    feat = lyr.GetNextFeature()
    check_value_23(lyr.GetLayerDefn(), feat)

    geom = feat.GetGeometryRef()
    wkt = geom.ExportToWkt()
    assert wkt == "POINT (10 20)", "Wrong WKT :" + wkt


###############################################################################
# Test the tables= connection string option


@pytest.mark.usefixtures("tpoly", "testgeom")
def test_ogr_pg_31(pg_ds):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = pg_ds.CreateLayer(
        "test_for_tables_equal_param",
        geom_type=ogr.wkbPoint,
        srs=srs,
        options=["OVERWRITE=YES"],
    )
    lyr.StartTransaction()
    for i in range(501):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
        lyr.CreateFeature(f)
    lyr.CommitTransaction()

    ds = ogr.Open(pg_ds.GetDescription() + " tables=tpoly,testgeom", update=1)

    assert ds is not None and ds.GetLayerCount() == 2

    sql_lyr = ds.ExecuteSQL("SELECT * FROM test_for_tables_equal_param")
    i = 0
    while True:
        f = sql_lyr.GetNextFeature()
        if f is None:
            break
        i = i + 1
    ds.ReleaseResultSet(sql_lyr)
    assert i == 501


###############################################################################
# Test the TABLES open option


def test_ogr_pg_tables_open_option(pg_ds):

    pg_ds.CreateLayer(
        "test (with parenthesis and \\)",
        geom_type=ogr.wkbPoint,
    )
    pg_ds.CreateLayer(
        "test with, comma",
        geom_type=ogr.wkbPoint,
    )
    pg_ds.FlushCache()

    with gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR,
        open_options=["TABLES=test \\(with parenthesis and \\\\)"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetName() == "test (with parenthesis and \\)"

    with gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR,
        open_options=["TABLES=test \\(with parenthesis and \\\\)(geometry)"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetName() == "test (with parenthesis and \\)"

    with gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR,
        open_options=['TABLES="test with, comma"'],
    ) as ds:
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetName() == "test with, comma"


###############################################################################
# Test approximate srtext (#2123, #3508)


@only_with_postgis
@pytest.mark.usefixtures("empty_spatial_ref_sys")
def test_ogr_pg_32(pg_ds):

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    pg_lyr = pg_ds.CreateLayer("testsrtext", srs=srs)

    sql_lyr = pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail("did not get expected count after step (1)")
    pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create second layer with very approximative EPSG:4326

    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]'
    )
    pg_lyr = pg_ds.CreateLayer("testsrtext2", srs=srs)

    # Must still be 1
    sql_lyr = pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail("did not get expected count after step (2)")
    pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create third layer with very approximative EPSG:4326 but without authority

    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        """GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]"""
    )
    pg_lyr = pg_ds.CreateLayer("testsrtext3", srs=srs)

    # Must still be 1
    sql_lyr = pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail("did not get expected count after step (3)")
    pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create Layer with EPSG:26632

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26632)

    pg_lyr = pg_ds.CreateLayer("testsrtext4", geom_type=ogr.wkbPoint, srs=srs)
    feat = ogr.Feature(pg_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    pg_lyr.CreateFeature(feat)
    feat = None
    sr = pg_lyr.GetSpatialRef()
    assert sr.ExportToWkt().find("26632") != -1, "did not get expected SRS"

    sql_lyr = pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 2 now
    if feat.count != 2:
        feat.DumpReadable()
        pytest.fail("did not get expected count after step (4)")
    pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Test GetSpatialRef() on SQL layer (#4644)

    sql_lyr = pg_ds.ExecuteSQL("SELECT * FROM testsrtext4")
    sr = sql_lyr.GetSpatialRef()
    assert sr.ExportToWkt().find("26632") != -1, "did not get expected SRS"
    pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Test getting SRS and geom type without requiring to fetch the layer defn

    for i in range(2):
        # sys.stderr.write('BEFORE OPEN\n')
        ds = reconnect(pg_ds, update=1)
        # sys.stderr.write('AFTER Open\n')
        lyr = ds.GetLayerByName("testsrtext4")
        # sys.stderr.write('AFTER GetLayerByName\n')
        if i == 0:
            sr = lyr.GetSpatialRef()
            # sys.stderr.write('AFTER GetSpatialRef\n')
            geom_type = lyr.GetGeomType()
            # sys.stderr.write('AFTER GetGeomType\n')
        else:
            geom_type = lyr.GetGeomType()
            # sys.stderr.write('AFTER GetGeomType\n')
            sr = lyr.GetSpatialRef()
            # sys.stderr.write('AFTER GetSpatialRef\n')

        assert sr.ExportToWkt().find("26632") != -1, "did not get expected SRS"
        assert geom_type == ogr.wkbPoint, "did not get expected geom type"

        ds = None

    ######################################################
    # Create Layer with non EPSG SRS

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=vandg")

    pg_lyr = pg_ds.CreateLayer("testsrtext5", srs=srs)

    sql_lyr = pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 3 now
    if feat.count != 3:
        feat.DumpReadable()
        pytest.fail("did not get expected count after step (5)")
    pg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test encoding as UTF8


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_33(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")
    assert pg_lyr is not None, "did not get tpoly layer"

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())
    # eacute in UTF8 : 0xc3 0xa9
    dst_feat.SetField("SHORTNAME", "\xc3\xa9")
    pg_lyr.CreateFeature(dst_feat)


###############################################################################
# Test encoding as Latin1


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_34(pg_ds):

    # We only test that on Linux since setting os.environ['XXX']
    # is not guaranteed to have effects on system not supporting putenv
    if sys.platform.startswith("linux"):
        os.environ["PGCLIENTENCODING"] = "LATIN1"
        pg_ds = reconnect(pg_ds)
        del os.environ["PGCLIENTENCODING"]

        # For some unknown reasons, some servers don't like forcing LATIN1
        # but prefer LATIN9 instead...
        if pg_ds is None:
            os.environ["PGCLIENTENCODING"] = "LATIN9"
            pg_ds = reconnect(pg_ds)
            del os.environ["PGCLIENTENCODING"]
    else:
        pg_ds.ExecuteSQL("SET client_encoding = LATIN1")

    pg_lyr = pg_ds.GetLayerByName("tpoly")
    assert pg_lyr is not None, "did not get tpoly layer"

    dst_feat = ogr.Feature(feature_def=pg_lyr.GetLayerDefn())
    # eacute in Latin1 : 0xe9
    dst_feat.SetField("SHORTNAME", "\xe9")
    pg_lyr.CreateFeature(dst_feat)


###############################################################################
# Test for buffer overflows


def test_ogr_pg_35(pg_ds):

    with gdal.quiet_errors():
        try:
            pg_lyr = pg_ds.CreateLayer("testoverflows")
            ogrtest.quick_create_layer_def(pg_lyr, [("0123456789" * 1000, ogr.OFTReal)])
            # To trigger actual layer creation
            pg_lyr.ResetReading()
        except Exception:
            pass

    with gdal.quiet_errors():
        try:
            pg_lyr = pg_ds.CreateLayer(
                "testoverflows",
                options=["OVERWRITE=YES", "GEOMETRY_NAME=" + ("0123456789" * 1000)],
            )
            # To trigger actual layer creation
            pg_lyr.ResetReading()
        except Exception:
            pass


###############################################################################
# Test support for inherited tables : tables inherited from a Postgis Table


def test_ogr_pg_36(pg_ds, use_postgis, tmp_schema):

    if use_postgis:
        lyr = pg_ds.CreateLayer("table36_base", geom_type=ogr.wkbPoint)
    else:
        lyr = pg_ds.CreateLayer("table36_base")

    pg_ds.ExecuteSQL(
        "CREATE TABLE table36_inherited ( col1 CHAR(1) ) INHERITS ( table36_base )"
    )
    pg_ds.ExecuteSQL(
        "CREATE TABLE table36_inherited2 ( col2 CHAR(1) ) INHERITS ( table36_inherited )"
    )

    # Test fix for #3636 when 2 inherited tables with same name exist in 2 different schemas
    if use_postgis:
        # lyr = gdaltest.pg_ds.CreateLayer( 'table36_base', geom_type = ogr.wkbLineString, options = ['SCHEMA=AutoTest-schema'] )
        lyr = pg_ds.CreateLayer(
            f"{tmp_schema}.table36_base", geom_type=ogr.wkbLineString
        )
    else:
        lyr = pg_ds.CreateLayer("table36_base", options={"SCHEMA": tmp_schema})

    pg_ds.ExecuteSQL(
        f'CREATE TABLE "{tmp_schema}"."table36_inherited" ( col3 CHAR(1) ) INHERITS ( "{tmp_schema}".table36_base )'
    )
    pg_ds.ExecuteSQL(
        f'CREATE TABLE "{tmp_schema}"."table36_inherited2" ( col4 CHAR(1) ) INHERITS ( "{tmp_schema}".table36_inherited )'
    )

    ds = reconnect(pg_ds, update=1)

    found = ogr_pg_check_layer_in_list(ds, "table36_inherited")
    assert found is not False, "layer table36_inherited not listed"

    found = ogr_pg_check_layer_in_list(ds, "table36_inherited2")
    assert found is not False, "layer table36_inherited2 not listed"

    lyr = ds.GetLayerByName("table36_inherited2")
    assert lyr is not None
    if use_postgis:
        assert (
            lyr.GetGeomType() == ogr.wkbPoint
        ), "wrong geometry type for layer table36_inherited2"

    lyr = ds.GetLayerByName(f"{tmp_schema}.table36_inherited2")
    assert lyr is not None

    if use_postgis:
        assert (
            lyr.GetGeomType() == ogr.wkbLineString
        ), f"wrong geometry type for layer {tmp_schema}.table36_inherited2"

    ds = ogr.Open(pg_ds.GetDescription() + " TABLES=table36_base", update=1)

    found = ogr_pg_check_layer_in_list(ds, "table36_inherited")
    assert found is not True, "layer table36_inherited is listed but it should not"

    lyr = ds.GetLayerByName("table36_inherited")
    assert lyr is not None

    if use_postgis:
        assert lyr.GetGeomType() == ogr.wkbPoint


###############################################################################
# Test support for inherited tables : Postgis table inherited from a non-Postgis table


@only_with_postgis
def test_ogr_pg_37(pg_ds):

    schema = current_schema(pg_ds)

    pg_ds.ExecuteSQL("CREATE TABLE table37_base ( col1 CHAR(1) )")
    pg_ds.ExecuteSQL(
        "CREATE TABLE table37_inherited ( col2 CHAR(1) ) INHERITS ( table37_base )"
    )
    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','table37_inherited','wkb_geometry',-1,'POINT',2)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    ds = reconnect(pg_ds, update=1)

    found = ogr_pg_check_layer_in_list(ds, "table37_inherited")
    assert found is not False, "layer table37_inherited not listed"

    lyr = ds.GetLayerByName("table37_inherited")
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint


###############################################################################
# Test support for multiple geometry columns (RFC 41)


@only_with_postgis
def test_ogr_pg_38(pg_ds):

    schema = current_schema(pg_ds)

    pg_ds.ExecuteSQL("CREATE TABLE table37_base ( col1 CHAR(1) )")
    pg_ds.ExecuteSQL(
        "CREATE TABLE table37_inherited ( col2 CHAR(1) ) INHERITS ( table37_base )"
    )
    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','table37_inherited','wkb_geometry',-1,'POINT',2)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','table37_base','pointBase',-1,'POINT',2)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','table37_inherited','point25D',-1,'POINT',3)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    ds = reconnect(pg_ds, update=1)

    # Check for the layer with the wkb_geometry column
    found = ogr_pg_check_layer_in_list(ds, "table37_inherited")
    assert found is not False, "layer table37_inherited not listed"

    lyr = ds.GetLayerByName("table37_inherited")
    assert lyr is not None
    gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(
        lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry")
    )
    assert gfld_defn is not None
    assert gfld_defn.GetType() == ogr.wkbPoint
    if lyr.GetLayerDefn().GetGeomFieldCount() != 3:
        for i in range(lyr.GetLayerDefn().GetGeomFieldCount()):
            print(lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName())
        assert lyr.GetLayerDefn().GetGeomFieldCount() == 3

    # Explicit query to 'table37_inherited(wkb_geometry)' should also work
    lyr = ds.GetLayerByName("table37_inherited(wkb_geometry)")
    assert lyr is not None

    lyr = ds.GetLayerByName("table37_inherited(pointBase)")
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert lyr.GetGeometryColumn() == "pointBase", "wrong geometry column name"

    lyr = ds.GetLayerByName("table37_inherited(point25D)")
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint25D
    assert lyr.GetGeometryColumn() == "point25D", "wrong geometry column name"

    # Check for the layer with the new 'point25D' geometry column when tables= is specified
    ds = ogr.Open(
        pg_ds.GetDescription() + " tables=table37_inherited(point25D)",
        update=1,
    )

    lyr = ds.GetLayerByName("table37_inherited(point25D)")
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint25D
    assert lyr.GetGeometryColumn() == "point25D", "wrong geometry column name"


###############################################################################
# Test support for named views


@only_without_postgis
@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_39(pg_ds):

    pg_ds.ExecuteSQL("CREATE VIEW testview AS SELECT * FROM tpoly")
    ds = reconnect(pg_ds, update=1)
    found = ogr_pg_check_layer_in_list(ds, "testview")
    assert found is not False, "layer testview not listed"


@only_with_postgis
def test_ogr_pg_39_bis(pg_ds, pg_has_postgis):

    schema = current_schema(pg_ds)

    # Create some tables
    pg_ds.CreateLayer("base", geom_type=ogr.wkbPoint)

    pg_ds.ExecuteSQL("CREATE TABLE inherited (col1 char(1)) INHERITS (base)")
    sql_lyr = pg_ds.ExecuteSQL(
        f"SELECT AddGeometryColumn('{schema}','inherited','point25D',-1,'POINT',3)"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    # Create a view
    pg_ds.ExecuteSQL("CREATE VIEW testview AS SELECT * FROM inherited")
    pg_ds.ExecuteSQL(
        "INSERT INTO inherited (col1, wkb_geometry) VALUES ( 'a', GeomFromEWKT('POINT (0 1)') )"
    )

    # Check for the view layer
    ds = reconnect(pg_ds, update=1)
    found = ogr_pg_check_layer_in_list(ds, "testview")
    assert found is not False, "layer testview not listed"

    lyr = ds.GetLayerByName("testview")
    assert lyr is not None

    gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(
        lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry")
    )
    assert gfld_defn is not None
    assert gfld_defn.GetType() == ogr.wkbPoint

    feat = lyr.GetNextFeature()
    assert feat is not None, "no feature"

    assert (
        feat.GetGeomFieldRef("wkb_geometry") is not None
        and feat.GetGeomFieldRef("wkb_geometry").ExportToWkt() == "POINT (0 1)"
    ), ("bad geometry %s" % feat.GetGeometryRef().ExportToWkt())

    # Test another geometry column

    pg_ds.ExecuteSQL(
        "UPDATE inherited SET \"point25D\" = GeomFromEWKT('POINT (0 1 2)') "
    )

    # Check for the view layer
    ds = reconnect(pg_ds, update=1)
    found = ogr_pg_check_layer_in_list(ds, "testview")
    assert found is not False, "layer testview not listed"

    lyr = ds.GetLayerByName("testview(point25D)")
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    assert lyr.GetGeometryColumn() == "point25D", "wrong geometry column name"

    feat = lyr.GetNextFeature()
    assert feat is not None, "no feature"

    assert (
        feat.GetGeometryRef() is not None
        and feat.GetGeometryRef().ExportToWkt() == "POINT (0 1 2)"
    ), ("bad geometry %s" % feat.GetGeometryRef().ExportToWkt())


###############################################################################
# Test GetFeature() with an invalid id


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_40(pg_ds):

    layer = pg_ds.GetLayerByName("tpoly")
    with gdal.quiet_errors():
        assert layer.GetFeature(0) is None


###############################################################################
# Test active_schema= option


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_41(pg_ds, tmp_schema):

    pg_ds.ExecuteSQL(f"CREATE TABLE {tmp_schema}.tpoly AS SELECT * FROM tpoly LIMIT 3")

    schema = current_schema(pg_ds)

    # strip a schemas= component from the connection string, which
    # would interfere with active_schema=
    dsn_clean = re.sub("schemas=[^\\s]*", "", pg_ds.GetDescription())

    ds = ogr.Open(
        f"{dsn_clean} active_schema={tmp_schema}",
        update=1,
    )

    # tpoly without schema refers to the active schema, that is to say tmp_schema
    found = ogr_pg_check_layer_in_list(ds, "tpoly")
    assert found is not False, "layer tpoly not listed"

    layer = ds.GetLayerByName("tpoly")
    assert layer.GetFeatureCount() == 3, "wrong feature count"

    found = ogr_pg_check_layer_in_list(ds, f"{tmp_schema}.tpoly")
    assert found is not True, f"layer {tmp_schema}.tpoly is listed, but should not"

    layer = ds.GetLayerByName(f"{tmp_schema}.tpoly")
    assert layer.GetFeatureCount() == 3, "wrong feature count"

    found = ogr_pg_check_layer_in_list(ds, f"{schema}.tpoly")
    assert found is not False, "layer tpoly not listed"

    layer = ds.GetLayerByName(f"{schema}.tpoly")
    assert layer.GetFeatureCount() == 10, "wrong feature count"

    ds.CreateLayer("test41")

    found = ogr_pg_check_layer_in_list(ds, "test41")
    assert found is not False, "layer test41 not listed"

    layer = ds.GetLayerByName("test41")
    assert layer.GetFeatureCount() == 0, "wrong feature count"

    layer = ds.GetLayerByName(f"{tmp_schema}.test41")
    assert layer.GetFeatureCount() == 0, "wrong feature count"


###############################################################################
# Test schemas= option


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_42(pg_ds, tmp_schema):

    pg_ds.ExecuteSQL(f"CREATE TABLE {tmp_schema}.tpoly AS SELECT * FROM tpoly LIMIT 3")
    schema = current_schema(pg_ds)

    # strip a schemas= component from the connection string, which
    # would interfere with active_schema=
    dsn_clean = re.sub("schemas=[^\\s]*", "", pg_ds.GetDescription())

    pg_ds = None
    ds = ogr.Open(dsn_clean + f" schemas={tmp_schema}", update=1)

    # tpoly without schema refers to the active schema, that is to say tmp_schema
    found = ogr_pg_check_layer_in_list(ds, "tpoly")
    assert found is not False, "layer tpoly not listed"

    layer = ds.GetLayerByName("tpoly")
    assert layer.GetFeatureCount() == 3, "wrong feature count"

    found = ogr_pg_check_layer_in_list(ds, f"{tmp_schema}.tpoly")
    assert found is not True, f"layer {tmp_schema}.tpoly is listed, but should not"

    layer = ds.GetLayerByName(f"{tmp_schema}.tpoly")
    assert layer.GetFeatureCount() == 3, "wrong feature count"

    found = ogr_pg_check_layer_in_list(ds, f"{schema}.tpoly")
    assert found is not True, f"layer {schema}.tpoly is listed, but should not"

    layer = ds.GetLayerByName(f"{schema}.tpoly")
    assert layer.GetFeatureCount() == 10, "wrong feature count"

    ds.CreateLayer("test42")

    found = ogr_pg_check_layer_in_list(ds, "test42")
    assert found is not False, "layer test41 not listed"

    layer = ds.GetLayerByName("test42")
    assert layer.GetFeatureCount() == 0, "wrong feature count"

    layer = ds.GetLayerByName(f"{tmp_schema}.test42")
    assert layer.GetFeatureCount() == 0, "wrong feature count"


###############################################################################
# Test schemas= option


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_43(pg_ds, tmp_schema):

    schema = current_schema(pg_ds)

    pg_ds.ExecuteSQL(f"CREATE TABLE {tmp_schema}.tpoly AS SELECT * FROM tpoly LIMIT 3")

    # strip a schemas= component from the connection string, which
    # would interfere with active_schema=
    dsn_clean = re.sub("schemas=[^\\s]*", "", pg_ds.GetDescription())

    pg_ds = None
    ds = ogr.Open(
        dsn_clean
        + f" application_name='foo\\\\ \\'bar' schemas='{schema},{tmp_schema}' active_schema={schema}",
        update=1,
    )

    # tpoly without schema refers to the active schema, that is to say "test_ogr_pg_43"
    found = ogr_pg_check_layer_in_list(ds, "tpoly")
    assert found is not False, "layer tpoly not listed"

    layer = ds.GetLayerByName("tpoly")
    assert layer.GetFeatureCount() == 10, "wrong feature count"

    found = ogr_pg_check_layer_in_list(ds, f"{tmp_schema}.tpoly")
    assert found is not False, f"layer {tmp_schema}.tpoly not listed"

    layer = ds.GetLayerByName(f"{tmp_schema}.tpoly")
    assert layer.GetFeatureCount() == 3, "wrong feature count"


###############################################################################
# Test for table and column names that need quoting (#2945)


def test_ogr_pg_44(pg_ds):

    pg_lyr = pg_ds.CreateLayer(
        "select", options=["OVERWRITE=YES", "GEOMETRY_NAME=where", "DIM=3"]
    )
    ogrtest.quick_create_layer_def(pg_lyr, [("from", ogr.OFTReal)])
    feat = ogr.Feature(pg_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0.5 0.5 1)"))
    pg_lyr.CreateFeature(feat)

    pg_ds.ExecuteSQL('ALTER TABLE "select" RENAME COLUMN "ogc_fid" to "AND"')
    with gdal.quiet_errors():
        pg_ds.ExecuteSQL("DELLAYER:from")

    ds = reconnect(pg_ds, update=1)
    layer = ds.GetLayerByName("select")
    geom = ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))")
    layer.SetSpatialFilter(geom)

    assert layer.GetFeatureCount() == 1
    feat = layer.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5 1)"

    feat = layer.GetFeature(1)
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5 1)"

    sql_lyr = ds.ExecuteSQL('SELECT * FROM "select"')
    geom = ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))")
    sql_lyr.SetSpatialFilter(geom)

    assert sql_lyr.GetFeatureCount() == 1
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5 1)"
    ds.ReleaseResultSet(sql_lyr)

    # Test layer renaming
    assert layer.TestCapability(ogr.OLCRename) == 1
    assert layer.Rename("from") == ogr.OGRERR_NONE
    assert layer.GetDescription() == "from"
    assert layer.GetLayerDefn().GetName() == "from"
    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5 1)"
    with gdal.quiet_errors():
        assert layer.Rename("from") != ogr.OGRERR_NONE
    assert layer.Rename("select") == ogr.OGRERR_NONE


###############################################################################
# Test SetNextByIndex (#3117)


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_45(pg_ds):

    lyr = pg_ds.GetLayerByName("tpoly")

    assert lyr.TestCapability(
        ogr.OLCFastSetNextByIndex
    ), "OLCFastSetNextByIndex returned false"

    nb_feat = lyr.GetFeatureCount()
    tab_feat = [None for i in range(nb_feat)]
    for i in range(nb_feat):
        tab_feat[i] = lyr.GetNextFeature()

    lyr.SetNextByIndex(2)
    feat = lyr.GetNextFeature()
    assert (
        feat.GetFID() == tab_feat[2].GetFID()
    ), "SetNextByIndex(2) did not return expected feature"

    feat = lyr.GetNextFeature()
    assert feat.GetFID() == tab_feat[3].GetFID(), "did not get expected feature"


###############################################################################
# Test that we can read more than 500 features and update each one
# with SetFeature()


def test_ogr_pg_46(pg_ds):

    nFeatures = 1000

    # Create a table with nFeatures records
    lyr = pg_ds.CreateLayer("bigtable")
    field_defn = ogr.FieldDefn("field1", ogr.OFTInteger)
    lyr.CreateField(field_defn)

    feature_defn = lyr.GetLayerDefn()

    lyr.StartTransaction()
    for i in range(nFeatures):
        feat = ogr.Feature(feature_defn)
        feat.SetField(0, i)
        lyr.CreateFeature(feat)
    lyr.CommitTransaction()

    # Check that we can read more than 500 features and update each one
    # with SetFeature()
    count = 0
    sqllyr = pg_ds.ExecuteSQL("SELECT * FROM bigtable ORDER BY OGC_FID ASC")
    feat = sqllyr.GetNextFeature()
    while feat is not None:
        expected_val = count
        assert (
            feat.GetFieldAsInteger(0) == expected_val
        ), "expected value was %d. Got %d" % (expected_val, feat.GetFieldAsInteger(0))
        feat.SetField(0, -count)
        lyr.SetFeature(feat)

        count = count + 1

        feat = sqllyr.GetNextFeature()

    assert count == nFeatures, "did not get expected %d features" % nFeatures

    # Check that 1 feature has been correctly updated
    sqllyr.SetNextByIndex(1)
    feat = sqllyr.GetNextFeature()
    expected_val = -1
    assert (
        feat.GetFieldAsInteger(0) == expected_val
    ), "expected value was %d. Got %d" % (expected_val, feat.GetFieldAsInteger(0))

    pg_ds.ReleaseResultSet(sqllyr)


###############################################################################
# Test that we can handle 'geography' column type introduced in PostGIS 1.5


@only_with_postgis
@pytest.mark.usefixtures("empty_spatial_ref_sys")
def test_ogr_pg_47(pg_ds, pg_postgis_version, pg_postgis_schema):

    if pg_ds.GetLayerByName(f"{pg_postgis_schema}.geography_columns") is None:
        pytest.skip("autotest database must be created with PostGIS >= 1.5")

    # Create table with geography column
    pg_ds.ExecuteSQL(
        """INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (4326,'EPSG',4326,'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]','+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ')"""
    )

    pg_ds.ExecuteSQL(
        """INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (4269,'EPSG',4269,'GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]]','+proj=longlat +datum=NAD83 +no_defs ')"""
    )

    pg_ds = reconnect(pg_ds, update=1)

    srs = osr.SpatialReference()

    # Only geographic SRS is supported
    srs.ImportFromEPSG(32631)
    with gdal.quiet_errors():
        lyr = pg_ds.CreateLayer(
            "test_geog",
            srs=srs,
            options=["GEOM_TYPE=geography", "GEOMETRY_NAME=my_geog"],
        )
    assert lyr is None

    if pg_postgis_version[0] >= 3 or (
        pg_postgis_version[0] == 2 and pg_postgis_version[1] >= 2
    ):
        srid = 4269
    else:
        srid = 4326

    srs.ImportFromEPSG(srid)
    lyr = pg_ds.CreateLayer(
        "test_geog", srs=srs, options=["GEOM_TYPE=geography", "GEOMETRY_NAME=my_geog"]
    )
    field_defn = ogr.FieldDefn("test_string", ogr.OFTString)
    lyr.CreateField(field_defn)

    feature_defn = lyr.GetLayerDefn()

    # Create feature
    feat = ogr.Feature(feature_defn)
    feat.SetField(0, "test_string")
    geom = ogr.CreateGeometryFromWkt("POINT (3 50)")
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    # Update feature
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = ogr.CreateGeometryFromWkt("POINT (2 49)")
    feat.SetGeometry(geom)
    lyr.SetFeature(feat)

    # Re-open DB
    lyr.ResetReading()  # to close implicit transaction
    pg_ds = reconnect(pg_ds, update=1)

    # Check if the layer is listed
    found = ogr_pg_check_layer_in_list(pg_ds, "test_geog")
    assert found is not False, "layer test_geog not listed"

    # Check that the layer is recorded in the geography_columns table
    geography_columns_lyr = pg_ds.ExecuteSQL(
        "SELECT * FROM geography_columns WHERE f_table_name = 'test_geog'"
    )
    feat = geography_columns_lyr.GetNextFeature()
    assert feat.GetFieldAsString("f_geography_column") == "my_geog"
    pg_ds.ReleaseResultSet(geography_columns_lyr)

    # Get the layer by name
    lyr = pg_ds.GetLayerByName("test_geog")
    assert lyr.GetExtent() == (2.0, 2.0, 49.0, 49.0), "bad extent for test_geog"

    assert lyr.GetSpatialRef().GetAuthorityCode(None) == str(srid)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POINT (2 49)", "bad geometry for test_geog"

    # Check with result set
    sql_lyr = pg_ds.ExecuteSQL("SELECT * FROM test_geog")
    assert sql_lyr.GetExtent() == (
        2.0,
        2.0,
        49.0,
        49.0,
    ), "bad extent for SELECT * FROM test_geog"

    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt() == "POINT (2 49)"
    ), "bad geometry for SELECT * FROM test_geog"
    pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsText
    sql_lyr = pg_ds.ExecuteSQL("SELECT ST_AsText(my_geog) FROM test_geog")
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt() == "POINT (2 49)"
    ), "bad geometry for SELECT ST_AsText(my_geog) FROM test_geog"
    pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsBinary
    sql_lyr = pg_ds.ExecuteSQL("SELECT ST_AsBinary(my_geog) FROM test_geog")
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt() == "POINT (2 49)"
    ), "bad geometry for SELECT ST_AsBinary(my_geog) FROM test_geog"
    pg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test that we can read a table without any primary key (#2082)
# Test also the effect of PG_LIST_ALL_TABLES with a non spatial table in a
# PostGIS DB.


def test_ogr_pg_48(pg_ds, use_postgis):

    pg_ds.ExecuteSQL("CREATE TABLE no_pk_table (gid serial NOT NULL, other_id INTEGER)")
    pg_ds.ExecuteSQL("INSERT INTO no_pk_table (gid, other_id) VALUES (1, 10)")

    pg_ds = reconnect(pg_ds, update=1)

    found = ogr_pg_check_layer_in_list(pg_ds, "no_pk_table")
    if use_postgis:
        # Non spatial table in a PostGIS db -> not listed ...
        assert not found, "layer no_pk_table unexpectedly listed"

        # ... but should be found on explicit request
        lyr = pg_ds.GetLayer("no_pk_table")
        assert lyr is not None, "could not get no_pk_table"

        # Try again by setting PG_LIST_ALL_TABLES=YES
        with gdal.config_option("PG_LIST_ALL_TABLES", "YES"):
            pg_ds = reconnect(pg_ds, update=1)
        found = ogr_pg_check_layer_in_list(pg_ds, "no_pk_table")

        assert found is not False, "layer no_pk_table not listed"

        # Test LIST_ALL_TABLES=YES open option
        pg_ds = gdal.OpenEx(
            pg_ds.GetDescription(),
            gdal.OF_VECTOR | gdal.OF_UPDATE,
            open_options=["LIST_ALL_TABLES=YES"],
        )
        found = ogr_pg_check_layer_in_list(pg_ds, "no_pk_table")

    assert found is not False, "layer no_pk_table not listed"

    lyr = pg_ds.GetLayer("no_pk_table")
    assert lyr is not None, "could not get no_pk_table"

    sr = lyr.GetSpatialRef()
    assert sr is None, "did not get expected SRS"

    feat = lyr.GetNextFeature()
    assert feat is not None, "did not get feature"

    assert lyr.GetFIDColumn() == "", "got a non NULL FID column"

    assert feat.GetFID() == 0
    assert feat.GetFieldAsInteger("gid") == 1
    assert feat.GetFieldAsInteger("other_id") == 10


###############################################################################
# Go on with previous test but set PGSQL_OGR_FID this time


def test_ogr_pg_49(pg_ds):

    pg_ds.ExecuteSQL("CREATE TABLE no_pk_table (gid serial NOT NULL, other_id INTEGER)")
    pg_ds.ExecuteSQL("INSERT INTO no_pk_table (gid, other_id) VALUES (1, 10)")

    with gdal.config_option("PGSQL_OGR_FID", "other_id"):
        pg_ds = reconnect(pg_ds, update=1)
        lyr = pg_ds.GetLayer("no_pk_table")

    feat = lyr.GetNextFeature()
    lyr.ResetReading()  # to close implicit transaction

    assert lyr.GetFIDColumn() == "other_id", "did not get expected FID column"

    assert feat.GetFID() == 10


###############################################################################
# Write and read NaN values (#3667)
# This tests writing using COPY and INSERT


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_50(pg_ds):

    pg_lyr = pg_ds.GetLayerByName("tpoly")
    assert pg_lyr is not None, "did not get tpoly layer"

    feature_def = pg_lyr.GetLayerDefn()
    dst_feat = ogr.Feature(feature_def)

    try:
        dst_feat.SetFieldDoubleList
        bHasSetFieldDoubleList = True
    except Exception:
        bHasSetFieldDoubleList = False

    for option in ["NO", "YES"]:
        with gdal.config_option("PG_USE_COPY", option):
            pg_lyr.ResetReading()
            for value in ["NaN", "Inf", "-Inf"]:
                dst_feat.SetField("AREA", float(value))
                dst_feat.SetField("PRFEDEA", value)
                dst_feat.SetField("SHORTNAME", option)
                if bHasSetFieldDoubleList:
                    dst_feat.SetFieldDoubleList(
                        feature_def.GetFieldIndex("REALLIST"),
                        [float(value), float(value)],
                    )
                dst_feat.SetFID(-1)
                pg_lyr.CreateFeature(dst_feat)

    for option in ["NO", "YES"]:
        for value in ["NaN", "Inf", "-Inf"]:
            pg_lyr.SetAttributeFilter(
                "PRFEDEA = '" + value + "' AND SHORTNAME = '" + option + "'"
            )
            feat = pg_lyr.GetNextFeature()
            got_val = feat.GetField("AREA")
            if value == "NaN":
                if not gdaltest.isnan(got_val):
                    pg_lyr.ResetReading()  # to close implicit transaction
                    pytest.fail(
                        feat.GetFieldAsString("AREA")
                        + " returned for AREA instead of "
                        + value
                    )
            elif got_val != float(value):
                pg_lyr.ResetReading()  # to close implicit transaction
                pytest.fail(
                    feat.GetFieldAsString("AREA")
                    + " returned for AREA instead of "
                    + value
                )

            if bHasSetFieldDoubleList:
                got_val = feat.GetFieldAsDoubleList(
                    feature_def.GetFieldIndex("REALLIST")
                )
                if value == "NaN":
                    if not gdaltest.isnan(got_val[0]) or not gdaltest.isnan(got_val[1]):
                        pg_lyr.ResetReading()  # to close implicit transaction
                        pytest.fail(
                            feat.GetFieldAsString("REALLIST")
                            + " returned for REALLIST instead of "
                            + value
                        )
                elif got_val[0] != float(value) or got_val[1] != float(value):
                    pg_lyr.ResetReading()  # to close implicit transaction
                    pytest.fail(
                        feat.GetFieldAsString("REALLIST")
                        + " returned for REALLIST instead of "
                        + value
                    )


###############################################################################
# Run test_ogrsf


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_51(pg_ds):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    pg_ds.ExecuteSQL("CREATE VIEW testview AS SELECT * FROM tpoly")

    ret = gdaltest.runexternal(
        f"{test_cli_utilities.get_test_ogrsf_path()} '{pg_ds.GetDescription()}' tpoly testview"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Run test_ogrsf with -sql


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_52(pg_ds):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + ' "'
        + pg_ds.GetDescription()
        + '" -sql "SELECT * FROM tpoly"'
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test creating a layer with explicitly wkbNone geometry type.


def test_ogr_pg_53(pg_ds, use_postgis):

    lyr = pg_ds.CreateLayer(
        "no_geometry_table", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
    )
    field_defn = ogr.FieldDefn("foo")
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "bar")
    lyr.CreateFeature(feat)

    lyr.ResetReading()  # force above feature to be committed

    ds = reconnect(pg_ds)

    if use_postgis:
        assert not (
            ogr_pg_check_layer_in_list(ds, "no_geometry_table") is True
        ), "did not expected no_geometry_table to be listed at that point"

    lyr = ds.GetLayerByName("no_geometry_table")
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == "bar"

    with gdal.quiet_errors():
        lyr = ds.CreateLayer("no_geometry_table", geom_type=ogr.wkbNone)
    assert lyr is None, "layer creation should have failed"

    lyr = ds.CreateLayer(
        "no_geometry_table", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
    )
    field_defn = ogr.FieldDefn("baz")
    lyr.CreateField(field_defn)

    ds = None
    ds = reconnect(pg_ds)

    lyr = ds.CreateLayer(
        "no_geometry_table", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
    )
    field_defn = ogr.FieldDefn("bar")
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("baz")
    lyr.CreateField(field_defn)
    assert lyr is not None

    ds = None

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("no_geometry_table")
    assert lyr.GetLayerDefn().GetFieldCount() == 2


###############################################################################
# Check that we can overwrite a non-spatial geometry table (#4012)


def test_ogr_pg_53_bis(tmp_path, pg_ds):
    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    lyr = pg_ds.CreateLayer(
        "no_geometry_table", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
    )
    lyr.ResetReading()

    f = open(tmp_path / "no_geometry_table.csv", "wt")
    f.write("foo,bar\n")
    f.write('"baz","foo"\n')
    f.close()

    ret = gdaltest.runexternal(
        f"{test_cli_utilities.get_ogr2ogr_path()} -f PostgreSQL '{pg_ds.GetDescription()}' {tmp_path / 'no_geometry_table.csv'} -overwrite"
    )

    assert "ERROR" not in ret

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("no_geometry_table")
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == "baz"


###############################################################################
# Test reading AsEWKB()


@only_with_postgis
def test_ogr_pg_54(pg_ds):

    sql_lyr = pg_ds.ExecuteSQL("SELECT ST_AsEWKB(GeomFromEWKT('POINT (0 1 2)'))")
    feat = sql_lyr.GetNextFeature()
    pg_ds.ReleaseResultSet(sql_lyr)

    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POINT (0 1 2)"


###############################################################################
# Test reading geoms as Base64 encoded strings


@only_with_postgis
def test_ogr_pg_55(pg_ds):

    layer = pg_ds.CreateLayer("ogr_pg_55", options=["DIM=3"])
    feat = ogr.Feature(layer.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2 3)"))
    layer.CreateFeature(feat)
    feat = None

    layer.ResetReading()  # force above feature to be committed

    with gdal.config_option("PG_USE_BASE64", "YES"):
        ds = reconnect(pg_ds, update=1)
        layer = ds.GetLayerByName("ogr_pg_55")
        feat = layer.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (1 2 3)"
    ds = None


###############################################################################
# Test insertion of features with first field being a 0-character string in a
# non-spatial table and without FID in COPY mode (#4040)


def test_ogr_pg_56(pg_ds):

    pg_ds.ExecuteSQL(
        "CREATE TABLE ogr_pg_56 ( bar varchar, baz varchar ) WITH (OIDS=FALSE)"
    )

    with gdal.config_option("PG_USE_COPY", "YES"):

        ds = reconnect(pg_ds, update=1)
        lyr = ds.GetLayerByName("ogr_pg_56")

        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, "")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(1, "")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, "")
        feat.SetField(1, "")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, "bar")
        feat.SetField(1, "")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, "")
        feat.SetField(1, "baz")
        lyr.CreateFeature(feat)

    ds = None

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("ogr_pg_56")

    feat = lyr.GetNextFeature()
    if feat.GetField(0) is not None or feat.GetField(1) is not None:
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != "" or feat.GetField(1) is not None:
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) is not None or feat.GetField(1) != "":
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != "" or feat.GetField(1) != "":
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != "bar" or feat.GetField(1) != "":
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != "" or feat.GetField(1) != "baz":
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    ds = None


###############################################################################
# Test inserting an empty feature


def test_ogr_pg_57(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_57")
    lyr.CreateField(ogr.FieldDefn("acolumn", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    feat = None

    assert ret == 0


###############################################################################
# Test RFC35


def test_ogr_pg_58(pg_ds):

    ds = reconnect(pg_ds, update=1)

    lyr = ds.CreateLayer("ogr_pg_58")
    lyr.CreateField(ogr.FieldDefn("strcolumn", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("aintcolumn", ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("aintcolumn", 12345)
    lyr.CreateFeature(feat)
    feat = None

    assert lyr.TestCapability(ogr.OLCReorderFields) == 0
    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1
    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    fd = ogr.FieldDefn("anotherstrcolumn", ogr.OFTString)
    fd.SetWidth(5)
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("aintcolumn"), fd, ogr.ALTER_ALL_FLAG
    )

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("anotherstrcolumn") == "12345", "failed (1)"

    ds = None
    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayer("ogr_pg_58")

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("anotherstrcolumn") == "12345", "failed (2)"
    feat = None
    lyr.ResetReading()  # to close implicit transaction

    assert (
        lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("anotherstrcolumn")) == 0
    ), "failed (3)"

    ds = None
    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayer("ogr_pg_58")

    assert lyr.GetLayerDefn().GetFieldCount() == 1, "failed (4)"


###############################################################################
# Check that we can use -nln with a layer name that is recognized by GetLayerByName()
# but which is not the layer name.


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_59(pg_ds):

    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    schema = current_schema(pg_ds)

    gdaltest.runexternal(
        f"{test_cli_utilities.get_ogr2ogr_path()} -append -f PostgreSQL '{pg_ds.GetDescription()}' data/poly.shp -nln {schema}.tpoly"
    )

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("tpoly")
    fc = lyr.GetFeatureCount()
    ds = None

    assert fc == 20, "did not get expected feature count"


###############################################################################
# Test that we can insert a feature that has a FID on a table with a
# non-incrementing PK.


def test_ogr_pg_60(pg_ds):

    sql_lyr = pg_ds.ExecuteSQL(
        "CREATE TABLE ogr_pg_60(id integer,"
        "name varchar(50),primary key (id)) "
        "without oids"
    )
    pg_ds.ReleaseResultSet(sql_lyr)

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_60")
    assert lyr.GetFIDColumn() == "id", "did not get expected name for FID column"

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(100)
    feat.SetField("name", "a_name")
    lyr.CreateFeature(feat)
    assert feat.GetFID() == 100, "bad FID value"

    feat = lyr.GetFeature(100)
    assert feat is not None, "did not get feature"


###############################################################################
# Test insertion of features with FID set in COPY mode (#4495)


def test_ogr_pg_61(pg_ds):

    pg_ds.ExecuteSQL(
        "CREATE TABLE ogr_pg_61 ( id integer NOT NULL PRIMARY KEY, bar varchar )"
    )

    with gdal.config_option("PG_USE_COPY", "YES"):

        ds = reconnect(pg_ds, update=1)
        lyr = ds.GetLayerByName("ogr_pg_61")

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(10)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(20)
        feat.SetField(0, "baz")
        lyr.CreateFeature(feat)

    ds = None

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("ogr_pg_61")

    feat = lyr.GetFeature(10)
    if not feat.IsFieldNull(0):
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    feat = lyr.GetFeature(20)
    if feat.GetField(0) != "baz":
        feat.DumpReadable()
        pytest.fail("did not get expected value for feat %d" % feat.GetFID())

    ds = None


###############################################################################
# Test ExecuteSQL() and getting SRID of the request (#4699)


@only_with_postgis
@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_62(pg_ds):

    # Test on a regular request in a table
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    pg_ds.CreateLayer("testsrtext2", srs=srs)

    sql_lyr = pg_ds.ExecuteSQL("SELECT * FROM testsrtext2")
    got_srs = sql_lyr.GetSpatialRef()
    pg_ds.ReleaseResultSet(sql_lyr)
    assert not (got_srs is None or got_srs.ExportToWkt().find("32631") == -1)

    # Test a request on a table, but with a geometry column not in the table
    sql_lyr = pg_ds.ExecuteSQL(
        "SELECT eas_id, GeomFromEWKT('SRID=4326;POINT(0 1)') FROM tpoly"
    )
    got_srs = sql_lyr.GetSpatialRef()
    pg_ds.ReleaseResultSet(sql_lyr)
    assert got_srs is not None
    assert "4326" in got_srs.ExportToWkt()


###############################################################################
# Test COLUMN_TYPES layer creation option (#4788)


@only_with_postgis
def test_ogr_pg_63(pg_ds):

    lyr = pg_ds.CreateLayer(
        "ogr_pg_63",
        options=[
            "COLUMN_TYPES=foo=int8,bar=numeric(10,5),baz=hstore,baw=numeric(20,0)"
        ],
    )
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("bar", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("baw", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "123")
    feat.SetField("baw", "123456789012345")
    lyr.StartTransaction()
    lyr.CreateFeature(feat)
    lyr.CommitTransaction()
    feat = None
    lyr = None

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("ogr_pg_63")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("foo"))
        .GetType()
        == ogr.OFTInteger64
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("bar"))
        .GetType()
        == ogr.OFTReal
    )

    feat = lyr.GetNextFeature()
    assert feat.GetField("foo") == 123
    assert feat.GetField("baw") == 123456789012345


###############################################################################
# Test OGR_TRUNCATE config. option (#5091)


@only_with_postgis
@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_64(pg_ds):

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("tpoly")

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("EAS_ID", "124")
    lyr.CreateFeature(feat)

    assert lyr.GetFeatureCount() == 11

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("tpoly")

    with gdal.config_option("OGR_TRUNCATE", "YES"):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("EAS_ID", "125")
        lyr.CreateFeature(feat)

    # Just one feature because of truncation
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test RFC 41


@only_with_postgis
def test_ogr_pg_65(pg_ds):

    ds = reconnect(pg_ds, update=1)
    assert ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) != 0
    lyr = ds.CreateLayer("ogr_pg_65", geom_type=ogr.wkbNone)
    assert lyr.TestCapability(ogr.OLCCreateGeomField) != 0

    gfld_defn = ogr.GeomFieldDefn("geom1", ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    assert lyr.CreateGeomField(gfld_defn) == 0

    gfld_defn = ogr.GeomFieldDefn("geom2", ogr.wkbLineString25D)
    assert lyr.CreateGeomField(gfld_defn) == 0

    gfld_defn = ogr.GeomFieldDefn("geom3", ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    gfld_defn.SetSpatialRef(srs)
    assert lyr.CreateGeomField(gfld_defn) == 0

    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("intfield", 123)
    feat.SetGeomFieldDirectly("geom1", ogr.CreateGeometryFromWkt("POINT (1 2)"))
    feat.SetGeomFieldDirectly(
        "geom2", ogr.CreateGeometryFromWkt("LINESTRING (3 4 5,6 7 8)")
    )
    assert lyr.CreateFeature(feat) == 0
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if (
        feat.GetField("intfield") != 123
        or feat.GetGeomFieldRef("geom1").ExportToWkt() != "POINT (1 2)"
        or feat.GetGeomFieldRef("geom2").ExportToWkt() != "LINESTRING (3 4 5,6 7 8)"
    ):
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if (
        feat.GetGeomFieldRef("geom1") is not None
        or feat.GetGeomFieldRef("geom2") is not None
    ):
        feat.DumpReadable()
        pytest.fail()

    ds = None
    for i in range(2):
        ds = reconnect(pg_ds)
        if i == 0:
            lyr = ds.GetLayerByName("ogr_pg_65")
        else:
            lyr = ds.ExecuteSQL("SELECT * FROM ogr_pg_65")
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "geom1"
        assert (
            i != 0 or lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
        )
        assert (
            lyr.GetLayerDefn()
            .GetGeomFieldDefn(0)
            .GetSpatialRef()
            .ExportToWkt()
            .find("4326")
            >= 0
        )
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetName() == "geom2"
        assert (
            i != 0
            or lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString25D
        )
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is None
        assert not (
            i == 0
            and lyr.GetLayerDefn()
            .GetGeomFieldDefn(2)
            .GetSpatialRef()
            .ExportToWkt()
            .find("32631")
            < 0
        )
        feat = lyr.GetNextFeature()
        if (
            feat.GetField("intfield") != 123
            or feat.GetGeomFieldRef("geom1").ExportToWkt() != "POINT (1 2)"
            or feat.GetGeomFieldRef("geom2").ExportToWkt() != "LINESTRING (3 4 5,6 7 8)"
        ):
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if (
            feat.GetGeomFieldRef("geom1") is not None
            or feat.GetGeomFieldRef("geom2") is not None
        ):
            feat.DumpReadable()
            pytest.fail()
        if i == 1:
            ds.ReleaseResultSet(lyr)

    with gdal.config_option("PG_USE_COPY", "YES"):
        ds = reconnect(pg_ds, update=1)
        lyr = ds.GetLayerByName("ogr_pg_65")
        lyr.DeleteFeature(1)
        lyr.DeleteFeature(2)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeomFieldDirectly("geom1", ogr.CreateGeometryFromWkt("POINT (3 4)"))
        feat.SetGeomFieldDirectly(
            "geom2", ogr.CreateGeometryFromWkt("LINESTRING (4 5 6,7 8 9)")
        )
        assert lyr.CreateFeature(feat) == 0
        feat = ogr.Feature(lyr.GetLayerDefn())
        assert lyr.CreateFeature(feat) == 0

    ds = reconnect(pg_ds)
    lyr = ds.GetLayerByName("ogr_pg_65")
    feat = lyr.GetNextFeature()
    if (
        feat.GetGeomFieldRef("geom1").ExportToWkt() != "POINT (3 4)"
        or feat.GetGeomFieldRef("geom2").ExportToWkt() != "LINESTRING (4 5 6,7 8 9)"
    ):
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if (
        feat.GetGeomFieldRef("geom1") is not None
        or feat.GetGeomFieldRef("geom2") is not None
    ):
        feat.DumpReadable()
        pytest.fail()

    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is not None:
        gdaltest.runexternal(
            f"{test_cli_utilities.get_ogr2ogr_path()} -update '{pg_ds.GetDescription()}' '{pg_ds.GetDescription()}' ogr_pg_65 -nln ogr_pg_65_copied -overwrite"
        )

        ds = reconnect(pg_ds)
        lyr = ds.GetLayerByName("ogr_pg_65_copied")
        assert (
            lyr.GetLayerDefn()
            .GetGeomFieldDefn(0)
            .GetSpatialRef()
            .ExportToWkt()
            .find("4326")
            >= 0
        )
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is None
        assert (
            lyr.GetLayerDefn()
            .GetGeomFieldDefn(2)
            .GetSpatialRef()
            .ExportToWkt()
            .find("32631")
            >= 0
        )


###############################################################################
# Run test_ogrsf


@only_with_postgis
@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_66(pg_ds):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        f"{test_cli_utilities.get_test_ogrsf_path()} '{pg_ds.GetDescription()}' tpoly"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test retrieving SRID from values (#5131)


@only_with_postgis
def test_ogr_pg_67(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_67")
    lyr.ResetReading()  # to trigger layer creation
    lyr = None

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_67")
    assert lyr.GetSpatialRef() is None
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_67")
    assert lyr.GetSpatialRef() is None
    ds.ExecuteSQL("ALTER TABLE ogr_pg_67 DROP CONSTRAINT enforce_srid_wkb_geometry")
    ds.ExecuteSQL(
        "UPDATE ogr_pg_67 SET wkb_geometry = ST_GeomFromEWKT('SRID=4326;POINT(0 1)')"
    )
    ds = None

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_67")
    assert lyr.GetSpatialRef() is not None
    ds = None

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_67")
    assert lyr.GetSpatialRef() is not None
    ds = None


###############################################################################
# Test retrieving SRID from values even if we don't have select rights on geometry_columns (#5131)


@only_with_postgis
def test_ogr_pg_68(pg_ds):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = pg_ds.CreateLayer("ogr_pg_68", srs=srs)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(feat)
    lyr = None

    sql_lyr = pg_ds.ExecuteSQL("SELECT current_user")
    feat = sql_lyr.GetNextFeature()
    current_user = feat.GetField(0)
    pg_ds.ReleaseResultSet(sql_lyr)

    pg_ds.ExecuteSQL("REVOKE SELECT ON geometry_columns FROM %s" % current_user)

    ds = ogr.Open(pg_ds.GetDescription() + " tables=fake", update=1)
    lyr = ds.GetLayerByName("ogr_pg_68")
    got_srs = None
    if lyr is not None:
        got_srs = lyr.GetSpatialRef()

    sql_lyr = ds.ExecuteSQL(
        "select * from ( select 'SRID=4326;POINT(0 0)'::geometry as g ) as _xx"
    )
    got_srs2 = None
    if sql_lyr is not None:
        got_srs2 = sql_lyr.GetSpatialRef()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    pg_ds.ExecuteSQL("GRANT SELECT ON geometry_columns TO %s" % current_user)

    assert got_srs is not None

    assert got_srs2 is not None


###############################################################################
# Test deferred loading of tables (#5450)


def has_run_load_tables(ds):
    return int(ds.GetMetadataItem("bHasLoadTables", "_DEBUG_"))


@pytest.mark.usefixtures("tpoly")
def test_ogr_pg_69(pg_ds):

    pg_ds = reconnect(pg_ds)
    assert not has_run_load_tables(pg_ds)
    pg_ds.GetLayerByName("tpoly")
    assert not has_run_load_tables(pg_ds)
    sql_lyr = pg_ds.ExecuteSQL("SELECT * FROM tpoly")
    assert not has_run_load_tables(pg_ds)
    feat = sql_lyr.GetNextFeature()
    assert not has_run_load_tables(pg_ds)
    del feat
    pg_ds.ReleaseResultSet(sql_lyr)

    pg_ds.GetLayer(0)
    assert has_run_load_tables(pg_ds)

    # Test that we can find a layer with non lowercase
    pg_ds = reconnect(pg_ds)
    assert pg_ds.GetLayerByName("TPOLY") is not None


###############################################################################
# Test historical non-deferred creation of tables (#5547)


def test_ogr_pg_70(pg_ds):

    with gdal.config_option("OGR_PG_DEFERRED_CREATION", "NO"):
        lyr = pg_ds.CreateLayer("ogr_pg_70")

    ds = reconnect(pg_ds)
    lyr2 = ds.GetLayerByName("ogr_pg_70")
    assert lyr2 is not None
    ds = None

    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    ds = reconnect(pg_ds)
    lyr2 = ds.GetLayerByName("ogr_pg_70")
    assert lyr2.GetLayerDefn().GetFieldCount() == 1
    ds = None

    gfld_defn = ogr.GeomFieldDefn("geom", ogr.wkbPoint)
    lyr.CreateGeomField(gfld_defn)

    ds = reconnect(pg_ds)
    lyr2 = ds.GetLayerByName("ogr_pg_70")
    assert lyr2.GetLayerDefn().GetGeomFieldCount() == 2
    ds = None


@only_with_postgis
def test_ogr_pg_70bis(pg_ds, pg_postgis_schema):

    schema = current_schema(pg_ds)

    if pg_ds.GetLayerByName(f"{pg_postgis_schema}.geography_columns") is None:
        pytest.skip("Missing geography_columns table")

    with gdal.config_option("OGR_PG_DEFERRED_CREATION", "NO"):
        lyr = pg_ds.CreateLayer(
            "ogr_pg_70", options=["GEOM_TYPE=geography", "GEOMETRY_NAME=my_geog"]
        )
        assert lyr is not None
    lyr.ResetReading()

    ds = reconnect(pg_ds)
    lyr2 = ds.GetLayerByName("ogr_pg_70")
    assert lyr2.GetLayerDefn().GetGeomFieldCount() == 1
    geography_columns_lyr = ds.ExecuteSQL(
        f"SELECT * FROM geography_columns WHERE f_table_schema = '{schema}' AND f_table_name = 'ogr_pg_70' AND f_geography_column = 'my_geog'"
    )
    assert geography_columns_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(geography_columns_lyr)
    ds = None


###############################################################################
# Test interoperability of WKT/WKB with PostGIS.


@only_with_postgis
def test_ogr_pg_71(pg_ds):

    curve_lyr = pg_ds.CreateLayer("test_curve")
    curve_lyr2 = pg_ds.CreateLayer(
        "test_curve_3d", geom_type=ogr.wkbUnknown | ogr.wkb25DBit
    )
    # FIXME: the ResetReading() should not be necessary
    curve_lyr.ResetReading()
    curve_lyr2.ResetReading()

    for wkt in [
        "CIRCULARSTRING EMPTY",
        "CIRCULARSTRING Z EMPTY",
        "CIRCULARSTRING (0 1,2 3,4 5)",
        "CIRCULARSTRING Z (0 1 2,4 5 6,7 8 9)",
        "COMPOUNDCURVE EMPTY",
        "TRIANGLE ((0 0 0,100 0 100,0 100 100,0 0 0))",
        "COMPOUNDCURVE ((0 1,2 3,4 5))",
        "COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9))",
        "COMPOUNDCURVE ((0 1,2 3,4 5),CIRCULARSTRING (4 5,6 7,8 9))",
        "COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9),CIRCULARSTRING Z (7 8 9,10 11 12,13 14 15))",
        "CURVEPOLYGON EMPTY",
        "CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))",
        "CURVEPOLYGON Z ((0 0 2,0 1 3,1 1 4,1 0 5,0 0 2))",
        "CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))",
        "CURVEPOLYGON Z (COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 2,1 0 3,0 0 2)))",
        "MULTICURVE EMPTY",
        "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1))",
        "MULTICURVE Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1),(0 0 1,1 1 1))",
        "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))",
        "MULTISURFACE EMPTY",
        "MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))",
        "MULTISURFACE Z (((0 0 1,0 10 1,10 10 1,10 0 1,0 0 1)),CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1)))",
        "GEOMETRYCOLLECTION (CIRCULARSTRING (0 1,2 3,4 5),COMPOUNDCURVE ((0 1,2 3,4 5)),CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0)),MULTICURVE ((0 0,1 1)),MULTISURFACE (((0 0,0 10,10 10,10 0,0 0))))",
    ]:

        postgis_in_wkt = wkt

        # Test parsing PostGIS WKB
        lyr = pg_ds.ExecuteSQL("SELECT ST_GeomFromText('%s')" % postgis_in_wkt)
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        g = None
        f = None
        pg_ds.ReleaseResultSet(lyr)

        expected_wkt = wkt
        assert out_wkt == expected_wkt

        # Test parsing PostGIS WKT
        lyr = pg_ds.ExecuteSQL(
            "SELECT ST_AsText(ST_GeomFromText('%s'))" % (postgis_in_wkt)
        )
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        g = None
        f = None
        pg_ds.ReleaseResultSet(lyr)

        expected_wkt = wkt
        assert out_wkt == expected_wkt

        g = ogr.CreateGeometryFromWkt(wkt)
        if g.GetCoordinateDimension() == 2:
            active_lyr = curve_lyr
        else:
            active_lyr = curve_lyr2

        # Use our WKB export to inject into PostGIS and check that
        # PostGIS interprets it correctly by checking with ST_AsText
        f = ogr.Feature(active_lyr.GetLayerDefn())
        f.SetGeometry(g)
        ret = active_lyr.CreateFeature(f)
        assert ret == 0, wkt
        fid = f.GetFID()

        lyr = pg_ds.ExecuteSQL(
            "SELECT ST_AsText(wkb_geometry) FROM %s WHERE ogc_fid = %d"
            % (active_lyr.GetName(), fid)
        )
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        pg_ds.ReleaseResultSet(lyr)
        g = None
        f = None

        assert out_wkt == wkt


###############################################################################
# Test 64 bit FID


def test_ogr_pg_72(pg_ds):

    # Regular layer with 32 bit IDs
    lyr = pg_ds.CreateLayer("ogr_pg_72")
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is None
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, "bar")
    assert lyr.CreateFeature(f) == 0
    f = lyr.GetFeature(123456789012345)
    assert f is not None

    lyr = pg_ds.CreateLayer("ogr_pg_72", options=["FID64=YES", "OVERWRITE=YES"])
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, "bar")
    assert lyr.CreateFeature(f) == 0
    assert lyr.SetFeature(f) == 0
    # Test with binary protocol
    # gdaltest.pg_ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )
    # lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_72')
    # if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    # f = lyr.GetNextFeature()
    # if f.GetFID() != 123456789012345:
    #    gdaltest.post_reason('fail')
    #    f.DumpReadable()
    #    return 'fail'
    # gdaltest.pg_ds = None
    # Test with normal protocol
    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_72")
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    if f.GetFID() != 123456789012345:
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()  # to close implicit transaction


###############################################################################
# Test not nullable fields


@only_with_postgis
def test_ogr_pg_73(pg_ds):

    with gdal.config_option("PG_USE_COPY", "NO"):

        lyr = pg_ds.CreateLayer("ogr_pg_73", geom_type=ogr.wkbNone)
        field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
        field_defn.SetNullable(0)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
        lyr.CreateField(field_defn)
        field_defn = ogr.GeomFieldDefn("geomfield_not_nullable", ogr.wkbPoint)
        field_defn.SetNullable(0)
        lyr.CreateGeomField(field_defn)
        field_defn = ogr.GeomFieldDefn("geomfield_nullable", ogr.wkbPoint)
        lyr.CreateGeomField(field_defn)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("field_not_nullable", "not_null")
        f.SetGeomFieldDirectly(
            "geomfield_not_nullable", ogr.CreateGeometryFromWkt("POINT(0 0)")
        )
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
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
        with gdal.quiet_errors():
            ret = lyr.CreateFeature(f)
        assert ret != 0
        f = None

    lyr.ResetReading()  # force above feature to be committed

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_73")
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
    assert (
        lyr.GetLayerDefn()
        .GetGeomFieldDefn(
            lyr.GetLayerDefn().GetGeomFieldIndex("geomfield_not_nullable")
        )
        .IsNullable()
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex("geomfield_nullable"))
        .IsNullable()
        == 1
    )

    # Turn not null into nullable
    src_fd = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_not_nullable")
    )
    fd = ogr.FieldDefn("now_nullable", src_fd.GetType())
    fd.SetNullable(1)
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_not_nullable"), fd, ogr.ALTER_ALL_FLAG
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("now_nullable"))
        .IsNullable()
        == 1
    )

    # Turn nullable into not null, but remove NULL values first
    ds.ExecuteSQL(
        "UPDATE ogr_pg_73 SET field_nullable = '' WHERE field_nullable IS NULL"
    )
    src_fd = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_nullable")
    )
    fd = ogr.FieldDefn("now_nullable", src_fd.GetType())
    fd.SetName("now_not_nullable")
    fd.SetNullable(0)
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_nullable"), fd, ogr.ALTER_ALL_FLAG
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("now_not_nullable"))
        .IsNullable()
        == 0
    )

    sql_lyr = ds.ExecuteSQL("SELECT * FROM ogr_pg_73")
    assert (
        sql_lyr.GetLayerDefn()
        .GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex("now_not_nullable"))
        .IsNullable()
        == 0
    )
    assert (
        sql_lyr.GetLayerDefn()
        .GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex("now_nullable"))
        .IsNullable()
        == 1
    )
    assert (
        sql_lyr.GetLayerDefn()
        .GetGeomFieldDefn(
            sql_lyr.GetLayerDefn().GetGeomFieldIndex("geomfield_not_nullable")
        )
        .IsNullable()
        == 0
    )
    assert (
        sql_lyr.GetLayerDefn()
        .GetGeomFieldDefn(
            sql_lyr.GetLayerDefn().GetGeomFieldIndex("geomfield_nullable")
        )
        .IsNullable()
        == 1
    )
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test default values


@only_with_postgis
def test_ogr_pg_74(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_74", geom_type=ogr.wkbNone)

    field_defn = ogr.FieldDefn("field_string", ogr.OFTString)
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

    field_defn = ogr.FieldDefn("field_datetime3", ogr.OFTDateTime)
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_date", ogr.OFTDate)
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_time", ogr.OFTTime)
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull("field_string")
    f.SetField("field_int", 456)
    f.SetField("field_real", 4.56)
    f.SetField("field_datetime", "2015/06/30 12:34:56")
    f.SetField("field_datetime2", "2015/06/30 12:34:56")
    f.SetField("field_datetime3", "2015/06/30 12:34:56.123")
    f.SetField("field_date", "2015/06/30")
    f.SetField("field_time", "12:34:56")
    lyr.CreateFeature(f)
    f = None

    # Transition from COPY to INSERT
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Transition from INSERT to COPY
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_string", "b")
    f.SetField("field_int", 456)
    f.SetField("field_real", 4.56)
    f.SetField("field_datetime", "2015/06/30 12:34:56")
    f.SetField("field_datetime2", "2015/06/30 12:34:56")
    f.SetField("field_datetime3", "2015/06/30 12:34:56.123")
    f.SetField("field_date", "2015/06/30")
    f.SetField("field_time", "12:34:56")
    lyr.CreateFeature(f)
    f = None

    lyr.ResetReading()  # force above feature to be committed

    ds = reconnect(pg_ds, update=1)
    ds.ExecuteSQL('set timezone to "UTC"')
    lyr = ds.GetLayerByName("ogr_pg_74")
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
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_datetime3"))
        .GetDefault()
        == "'2015/06/30 12:34:56.123'"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_date"))
        .GetDefault()
        == "CURRENT_DATE"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_time"))
        .GetDefault()
        == "CURRENT_TIME"
    )

    f = lyr.GetNextFeature()
    if not f.IsFieldNull("field_string"):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if (
        f.GetField("field_string") != "a'b"
        or f.GetField("field_int") != 123
        or f.GetField("field_real") != 1.23
        or not f.IsFieldNull("field_nodefault")
        or not f.IsFieldSet("field_datetime")
        or f.GetField("field_datetime2") != "2015/06/30 12:34:56+00"
        or f.GetField("field_datetime3") != "2015/06/30 12:34:56.123+00"
        or not f.IsFieldSet("field_date")
        or not f.IsFieldSet("field_time")
    ):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.GetField("field_string") != "b":
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()  # to close implicit transaction

    # Change DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_string")
    )
    fd = ogr.FieldDefn("field_string", src_fd.GetType())
    fd.SetDefault("'c'")
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_string"), fd, ogr.ALTER_DEFAULT_FLAG
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_string"))
        .GetDefault()
        == "'c'"
    )

    # Drop DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_int")
    )
    fd = ogr.FieldDefn("field_int", src_fd.GetType())
    fd.SetDefault(None)
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("field_int"), fd, ogr.ALTER_DEFAULT_FLAG
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_int"))
        .GetDefault()
        is None
    )

    ds = None
    ds = reconnect(pg_ds, update=1)
    ds.ExecuteSQL('set timezone to "UTC"')
    lyr = ds.GetLayerByName("ogr_pg_74")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_string"))
        .GetDefault()
        == "'c'"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_int"))
        .GetDefault()
        is None
    )


###############################################################################
# Test creating a field with the fid name


@only_with_postgis
def test_ogr_pg_75(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_75", geom_type=ogr.wkbNone, options=["FID=myfid"])

    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("myfid", ogr.OFTString))
    assert ret != 0

    ret = lyr.CreateField(ogr.FieldDefn("myfid", ogr.OFTInteger))
    assert ret == 0
    lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str", "first string")
    feat.SetField("myfid", 10)
    feat.SetField("str2", "second string")
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 10

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str2", "second string")
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    if feat.GetFID() < 0:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetField("myfid") != feat.GetFID():
        feat.DumpReadable()
        pytest.fail()

    feat.SetField("str", "foo")
    ret = lyr.SetFeature(feat)
    assert ret == 0

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    feat.SetField("myfid", 10)
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.SetFeature(feat)
    assert ret != 0

    feat.UnsetField("myfid")
    with gdal.quiet_errors():
        ret = lyr.SetFeature(feat)
    assert ret != 0

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str", "first string")
    feat.SetField("myfid", 12)
    feat.SetField("str2", "second string")
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 12

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if (
        f.GetFID() != 10
        or f.GetField("str") != "first string"
        or f.GetField("str2") != "second string"
        or f.GetField("myfid") != 10
    ):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetFeature(f.GetFID())
    if (
        f.GetFID() != 10
        or f.GetField("str") != "first string"
        or f.GetField("str2") != "second string"
        or f.GetField("myfid") != 10
    ):
        f.DumpReadable()
        pytest.fail()
    f = None
    lyr.ResetReading()  # to close implicit transaction


###############################################################################
# Test transactions RFC 54


def ogr_pg_76_get_transaction_state(ds):
    return (
        ds.GetMetadataItem("osDebugLastTransactionCommand", "_DEBUG_"),
        int(ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_")),
        int(ds.GetMetadataItem("bSavePointActive", "_DEBUG_")),
        int(ds.GetMetadataItem("bUserTransactionActive", "_DEBUG_")),
    )


def test_ogr_pg_76(pg_ds, use_postgis):

    assert pg_ds.TestCapability(ogr.ODsCTransactions) == 1

    level = int(pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    assert level == 0

    if use_postgis:
        pg_ds.StartTransaction()
        lyr = pg_ds.CreateLayer("will_not_be_created", options=["OVERWRITE=YES"])
        lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

        sql_lyr = pg_ds.ExecuteSQL(
            "SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'"
        )
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        pg_ds.ReleaseResultSet(sql_lyr)
        assert res == 1

        pg_ds.RollbackTransaction()

        # Rollback doesn't rollback the insertion in geometry_columns if done through the AddGeometryColumn()
        sql_lyr = pg_ds.ExecuteSQL(
            "SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'"
        )
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        pg_ds.ReleaseResultSet(sql_lyr)
        assert res == 0

    with gdal.config_option("OGR_PG_CURSOR_PAGE", "1"):
        lyr1 = pg_ds.CreateLayer(
            "ogr_pg_76_lyr1", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
        )
        lyr2 = pg_ds.CreateLayer(
            "ogr_pg_76_lyr2", geom_type=ogr.wkbNone, options=["OVERWRITE=YES"]
        )
    lyr1.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    # lyr2.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))

    level = int(pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    assert level == 0

    ogr_pg_76_scenario1(pg_ds, lyr1, lyr2)
    ogr_pg_76_scenario2(pg_ds, lyr1, lyr2)
    ogr_pg_76_scenario3(pg_ds, lyr1, lyr2)
    ogr_pg_76_scenario4(pg_ds, lyr1, lyr2)


# Scenario 1 : a CreateFeature done in the middle of GetNextFeature()


def ogr_pg_76_scenario1(pg_ds, lyr1, lyr2):

    _, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (level, savepoint, usertransac) == (0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 0)

    lyr1.SetAttributeFilter("foo is NULL")
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 0)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    # Check that GetFeature() doesn't reset the cursor
    f = lyr1.GetFeature(f.GetFID())
    assert f is not None and f.GetFID() == 2

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 3
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 0, 0)

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 1, 0, 0)
    lyr2.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)
    assert lyr1.GetFeatureCount() == 4


# Scenario 2 : a CreateFeature done in the middle of GetNextFeature(), themselves between a user transaction
def ogr_pg_76_scenario2(pg_ds, lyr1, lyr2):

    assert pg_ds.StartTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 1)

    # Try to re-enter a transaction
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = pg_ds.StartTransaction()
    assert not (gdal.GetLastErrorMsg() == "" or ret == 0)
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 1, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 0, 1)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 3, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 3
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 3, 0, 1)

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 0, 1)

    lyr2.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 1, 0, 1)

    assert pg_ds.CommitTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)

    assert pg_ds.StartTransaction() == 0

    assert pg_ds.RollbackTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("ROLLBACK", 0, 0, 0)

    # Try to re-commit a transaction
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = pg_ds.CommitTransaction()
    assert not (gdal.GetLastErrorMsg() == "" or ret == 0)
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 0, 0, 0)

    # Try to rollback a non-transaction
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = pg_ds.RollbackTransaction()
    assert not (gdal.GetLastErrorMsg() == "" or ret == 0)
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 0, 0, 0)


# Scenario 3 : StartTransaction(), GetNextFeature(), CommitTransaction(), GetNextFeature()


def ogr_pg_76_scenario3(pg_ds, lyr1, lyr2):

    assert pg_ds.StartTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 0, 1)

    assert pg_ds.CommitTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr1.GetNextFeature()
    assert gdal.GetLastErrorMsg() != "" and f is None

    # Must re-issue an explicit ResetReading()
    lyr1.ResetReading()

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 0)

    lyr1.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)

    lyr2.ResetReading()


# Scenario 4 : GetNextFeature(), StartTransaction(), CreateFeature(), CommitTransaction(), GetNextFeature(), ResetReading()


def ogr_pg_76_scenario4(pg_ds, lyr1, lyr2):

    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("BEGIN", 1, 0, 0)

    assert pg_ds.StartTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == (
        "SAVEPOINT ogr_savepoint",
        2,
        1,
        1,
    )

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 3, 1, 1)

    # Check that it doesn't commit the transaction
    lyr1.SetAttributeFilter("foo is NULL")
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 2, 1, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("", 3, 1, 1)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    assert pg_ds.CommitTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == (
        "RELEASE SAVEPOINT ogr_savepoint",
        2,
        0,
        0,
    )

    lyr2.ResetReading()

    assert pg_ds.StartTransaction() == 0

    assert pg_ds.RollbackTransaction() == 0
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == (
        "ROLLBACK TO SAVEPOINT ogr_savepoint",
        1,
        0,
        0,
    )

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    lyr1.ResetReading()
    lastcmd, level, savepoint, usertransac = ogr_pg_76_get_transaction_state(pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ("COMMIT", 0, 0, 0)


###############################################################################
# Test ogr2ogr can insert multiple layers at once


def test_ogr_pg_77(pg_ds, tmp_path):
    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open(tmp_path / "ogr_pg_77_1.csv", "wt")
    f.write("id,WKT\n")
    f.write("1,POINT(1 2)\n")
    f.close()
    f = open(tmp_path / "ogr_pg_77_2.csv", "wt")
    f.write("id,WKT\n")
    f.write("2,POINT(1 2)\n")
    f.close()
    gdaltest.runexternal(
        f"{test_cli_utilities.get_ogr2ogr_path()} -f PostgreSQL '{pg_ds.GetDescription()}' {tmp_path}"
    )

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_77_1")
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == "1"
    feat.SetField(0, 10)
    lyr.SetFeature(feat)
    lyr = ds.GetLayerByName("ogr_pg_77_2")
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == "2"
    ds = None

    # Test fix for #6018
    gdaltest.runexternal(
        f"{test_cli_utilities.get_ogr2ogr_path()} -f PostgreSQL '{pg_ds.GetDescription()}' {tmp_path} -overwrite"
    )

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("ogr_pg_77_1")
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == "1"
    ds = None


###############################################################################
# Test manually added geometry constraints


@only_with_postgis
def test_ogr_pg_78(pg_ds):

    pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_78 (ID INTEGER PRIMARY KEY)")
    pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78 ADD COLUMN my_geom GEOMETRY")
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_type CHECK (geometrytype(my_geom)='POINT')"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_dim CHECK (st_ndims(my_geom)=3)"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_srid CHECK (st_srid(my_geom)=4326)"
    )

    pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_78_2 (ID INTEGER PRIMARY KEY)")
    pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78_2 ADD COLUMN my_geog GEOGRAPHY")
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_type CHECK (geometrytype(my_geog::geometry)='POINT')"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_dim CHECK (st_ndims(my_geog::geometry)=3)"
    )
    pg_ds.ExecuteSQL(
        "ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_srid CHECK (st_srid(my_geog::geometry)=4326)"
    )

    pg_ds = reconnect(pg_ds, update=1)
    lc = pg_ds.GetLayerCount()  # force discovery of all tables
    ogr_pg_78_found = False
    ogr_pg_78_2_found = False
    for i in range(lc):
        lyr = pg_ds.GetLayer(i)
        if lyr.GetName() == "ogr_pg_78":
            ogr_pg_78_found = True
            assert lyr.GetGeomType() == ogr.wkbPoint25D
            assert lyr.GetSpatialRef().ExportToWkt().find("4326") >= 0
        if lyr.GetName() == "ogr_pg_78_2":
            ogr_pg_78_2_found = True
            assert lyr.GetGeomType() == ogr.wkbPoint25D
            assert lyr.GetSpatialRef().ExportToWkt().find("4326") >= 0
    assert ogr_pg_78_found
    assert ogr_pg_78_2_found

    # Test with slow method
    with gdal.config_option("PG_USE_POSTGIS2_OPTIM", "NO"):
        pg_ds = reconnect(pg_ds, update=1)
        lc = pg_ds.GetLayerCount()  # force discovery of all tables
        ogr_pg_78_found = False
        ogr_pg_78_2_found = False
        for i in range(lc):
            lyr = pg_ds.GetLayer(i)
            if lyr.GetName() == "ogr_pg_78":
                ogr_pg_78_found = True
                if lyr.GetGeomType() != ogr.wkbPoint25D:
                    # FIXME: why does it fail suddenly on Travis ? Change of PostGIS version ?
                    # But apparently not :
                    # Last good: https://travis-ci.org/OSGeo/gdal/builds/60211881
                    # First bad: https://travis-ci.org/OSGeo/gdal/builds/60290209
                    val = gdal.GetConfigOption("TRAVIS", None)
                    if val is not None:
                        print("Fails on Travis. geom_type = %d" % lyr.GetGeomType())
                    else:
                        pytest.fail()
                if (
                    lyr.GetSpatialRef() is None
                    or lyr.GetSpatialRef().ExportToWkt().find("4326") < 0
                ):
                    val = gdal.GetConfigOption("TRAVIS", None)
                    if val is not None:
                        print(
                            "Fails on Travis. GetSpatialRef() = %s"
                            % str(lyr.GetSpatialRef())
                        )
                    else:
                        pytest.fail()
            if lyr.GetName() == "ogr_pg_78_2":
                ogr_pg_78_2_found = True
                # No logic in geography_columns to get type/coordim/srid from constraints
                # if lyr.GetGeomType() != ogr.wkbPoint25D:
                #    gdaltest.post_reason('fail')
                #    return 'fail'
                # if lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
                #    gdaltest.post_reason('fail')
                #    return 'fail'
        assert ogr_pg_78_found
        assert ogr_pg_78_2_found


###############################################################################
# Test PRELUDE_STATEMENTS and CLOSING_STATEMENTS open options


def test_ogr_pg_79(pg_ds):

    # PRELUDE_STATEMENTS starting with BEGIN (use case: pg_bouncer in transaction pooling)
    ds = gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR | gdal.OF_UPDATE,
        open_options=[
            'PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
            "CLOSING_STATEMENTS=COMMIT;",
        ],
    )
    sql_lyr = ds.ExecuteSQL("SHOW statement_timeout")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != "1h":
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    assert ret == 0
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_pg_79a(pg_ds):

    # random PRELUDE_STATEMENTS
    ds = gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR | gdal.OF_UPDATE,
        open_options=['PRELUDE_STATEMENTS=SET statement_timeout TO "1h"'],
    )
    sql_lyr = ds.ExecuteSQL("SHOW statement_timeout")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != "1h":
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    assert ret == 0
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_pg_79b(pg_ds):

    # Test wrong PRELUDE_STATEMENTS
    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            pg_ds.GetDescription(),
            gdal.OF_VECTOR | gdal.OF_UPDATE,
            open_options=[
                'PRELUDE_STATEMENTS=BEGIN;error SET LOCAL statement_timeout TO "1h";',
                "CLOSING_STATEMENTS=COMMIT;",
            ],
        )
    assert ds is None


def test_ogr_pg_79c(pg_ds):

    # Test wrong CLOSING_STATEMENTS
    ds = gdal.OpenEx(
        pg_ds.GetDescription(),
        gdal.OF_VECTOR | gdal.OF_UPDATE,
        open_options=[
            'PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
            "CLOSING_STATEMENTS=COMMIT;error",
        ],
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = None
    assert gdal.GetLastErrorMsg() != ""
    assert ds is None  # keep flake8 happy


###############################################################################
# Test retrieving an error from ExecuteSQL() (#6194)


@only_without_postgis
def test_ogr_pg_80(pg_ds):

    gdal.ErrorReset()
    with gdal.quiet_errors():
        sql_lyr = pg_ds.ExecuteSQL("SELECT FROM")
    assert gdal.GetLastErrorMsg() != ""
    assert sql_lyr is None


###############################################################################
# Test that ogr2ogr -skip properly rollbacks transactions (#6328)


@only_with_postgis
def test_ogr_pg_81(pg_ds, tmp_vsimem):

    pg_ds.ReleaseResultSet(
        pg_ds.ExecuteSQL(
            "create table ogr_pg_81_1(id varchar unique, foo varchar); SELECT AddGeometryColumn('ogr_pg_81_1','dummy',-1,'POINT',2);"
        )
    )
    pg_ds.ReleaseResultSet(
        pg_ds.ExecuteSQL(
            "create table ogr_pg_81_2(id varchar unique, foo varchar); SELECT AddGeometryColumn('ogr_pg_81_2','dummy',-1,'POINT',2);"
        )
    )

    # 0755 = 493
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_pg_81_1.csv",
        """id,foo
1,1""",
    )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_pg_81_2.csv",
        """id,foo
1,1""",
    )

    gdal.VectorTranslate(pg_ds.GetDescription(), tmp_vsimem, accessMode="append")

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_pg_81_2.csv",
        """id,foo
2,2""",
    )

    with gdal.quiet_errors():
        gdal.VectorTranslate(
            pg_ds.GetDescription(),
            tmp_vsimem,
            accessMode="append",
            skipFailures=True,
        )

    lyr = pg_ds.GetLayer("ogr_pg_81_2")
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f["id"] != "2":
        f.DumpReadable()
        pytest.fail()
    lyr.ResetReading()  # flushes implicit transaction


###############################################################################
# Test that GEOMETRY_NAME works even when the geometry column creation is
# done through CreateGeomField (#6366)
# This is important for the ogr2ogr use case when the source geometry column
# is not-nullable, and hence the CreateGeomField() interface is used.


@only_with_postgis
def test_ogr_pg_82(pg_ds):

    lyr = pg_ds.CreateLayer(
        "ogr_pg_82", geom_type=ogr.wkbNone, options=["GEOMETRY_NAME=another_name"]
    )
    lyr.CreateGeomField(ogr.GeomFieldDefn("my_geom", ogr.wkbPoint))
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "another_name"


###############################################################################
# Test ZM support


def ogr_pg_83_ids(param):
    if type(param) is int:
        return ogr.GeometryTypeToName(param).replace(" ", "").replace("(any)", "")
    if type(param) is list:
        return "_".join(param) if param else "NONE"

    return param[0 : param.find("(")].replace(" ", "")


@only_with_postgis
@pytest.mark.parametrize(
    "geom_type,options,wkt,expected_wkt",
    [
        [ogr.wkbUnknown, [], "POINT ZM (1 2 3 4)", "POINT (1 2)"],
        [ogr.wkbUnknown, ["DIM=XYZM"], "POINT ZM (1 2 3 4)", "POINT ZM (1 2 3 4)"],
        [ogr.wkbUnknown, ["DIM=XYZ"], "POINT ZM (1 2 3 4)", "POINT Z (1 2 3)"],
        [ogr.wkbUnknown, ["DIM=XYM"], "POINT M (1 2 4)", "POINT M (1 2 4)"],
        [ogr.wkbPointZM, [], "POINT ZM (1 2 3 4)", "POINT ZM (1 2 3 4)"],
        [ogr.wkbPoint25D, [], "POINT ZM (1 2 3 4)", "POINT Z (1 2 3)"],
        [ogr.wkbPointM, [], "POINT ZM (1 2 3 4)", "POINT M (1 2 4)"],
        [
            ogr.wkbUnknown,
            ["GEOM_TYPE=geography", "DIM=XYM"],
            "POINT ZM (1 2 3 4)",
            "POINT M (1 2 4)",
        ],
    ],
    ids=ogr_pg_83_ids,
)
def test_ogr_pg_83(pg_ds, geom_type, options, wkt, expected_wkt):

    lyr = pg_ds.CreateLayer("ogr_pg_83", geom_type=geom_type, options=options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    lyr.CreateFeature(f)
    f = None
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    got_wkt = ""
    if f is not None:
        geom = f.GetGeometryRef()
        if geom is not None:
            got_wkt = geom.ExportToIsoWkt()
    assert got_wkt == expected_wkt, (geom_type, options, wkt, expected_wkt, got_wkt)
    lyr.ResetReading()  # flushes implicit transaction

    if "GEOM_TYPE=geography" in options:
        return
    # Cannot do AddGeometryColumn( 'GEOMETRYM', 3 ) with PostGIS >= 2, and doesn't accept inserting a XYM geometry
    if geom_type == ogr.wkbUnknown and options == ["DIM=XYM"]:
        return

    lyr = pg_ds.CreateLayer(
        "ogr_pg_83", geom_type=ogr.wkbNone, options=options + ["OVERWRITE=YES"]
    )
    # To force table creation to happen now so that following
    # CreateGeomField() is done through a AddGeometryColumn() call
    lyr.ResetReading()
    lyr.GetNextFeature()
    lyr.CreateGeomField(ogr.GeomFieldDefn("my_geom", geom_type))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    lyr.CreateFeature(f)
    f = None
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    got_wkt = ""
    if f is not None:
        geom = f.GetGeometryRef()
        if geom is not None:
            got_wkt = geom.ExportToIsoWkt()
    assert got_wkt == expected_wkt, (geom_type, options, wkt, expected_wkt, got_wkt)
    lyr.ResetReading()  # flushes implicit transaction


###############################################################################
# Test description


def test_ogr_pg_84(pg_ds):

    lyr = pg_ds.CreateLayer(
        "ogr_pg_84",
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES", "DESCRIPTION=foo"],
    )
    # Test that SetMetadata() and SetMetadataItem() are without effect
    lyr.SetMetadata({"DESCRIPTION": "bar"})
    lyr.SetMetadataItem("DESCRIPTION", "baz")
    assert lyr.GetMetadataItem("DESCRIPTION") == "foo"
    assert lyr.GetMetadata_List() == ["DESCRIPTION=foo"], lyr.GetMetadata()
    lyr.ResetReading()

    pg_ds = reconnect(pg_ds, update=1)
    pg_ds.GetLayerCount()  # load all layers
    lyr = pg_ds.GetLayerByName("ogr_pg_84")
    assert lyr.GetMetadataItem("DESCRIPTION") == "foo"
    assert lyr.GetMetadata_List() == ["DESCRIPTION=foo"], lyr.GetMetadata()
    # Set with SetMetadata()
    lyr.SetMetadata(["DESCRIPTION=bar"])

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_84")  # load just this layer
    assert lyr.GetMetadataItem("DESCRIPTION") == "bar"
    assert lyr.GetMetadataDomainList() is not None
    # Set with SetMetadataItem()
    lyr.SetMetadataItem("DESCRIPTION", "baz")

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_84")
    assert lyr.GetMetadataDomainList() is not None
    assert lyr.GetMetadataItem("DESCRIPTION") == "baz"
    # Unset with SetMetadataItem()
    lyr.SetMetadataItem("DESCRIPTION", None)

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_84")  # load just this layer
    assert lyr.GetMetadataDomainList() is None
    assert lyr.GetMetadataItem("DESCRIPTION") is None

    pg_ds = reconnect(pg_ds, update=1)
    pg_ds.GetLayerCount()  # load all layers
    lyr = pg_ds.GetLayerByName("ogr_pg_84")  # load just this layer
    assert lyr.GetMetadataItem("DESCRIPTION") is None


###############################################################################
# Test metadata


@only_without_postgis
@pytest.mark.parametrize("run_number", [1, 2])
def test_ogr_pg_metadata(pg_ds, run_number):

    pg_ds = reconnect(pg_ds, update=1)

    if run_number == 1:
        pg_ds.ExecuteSQL(
            "DROP EVENT TRIGGER IF EXISTS ogr_system_tables_event_trigger_for_metadata"
        )
        pg_ds.ExecuteSQL("DROP SCHEMA ogr_system_tables CASCADE")

    pg_ds.StartTransaction()
    lyr = pg_ds.CreateLayer(
        "test_ogr_pg_metadata", geom_type=ogr.wkbPoint, options=["OVERWRITE=YES"]
    )
    lyr.SetMetadata({"foo": "bar"})
    lyr.SetMetadataItem("bar", "baz")
    lyr.SetMetadataItem("DESCRIPTION", "my_desc")
    pg_ds.CommitTransaction()

    pg_ds = reconnect(pg_ds, update=1)

    with gdal.config_option("OGR_PG_ENABLE_METADATA", "NO"):
        lyr = pg_ds.GetLayerByName("test_ogr_pg_metadata")
        assert lyr.GetMetadata_Dict() == {"DESCRIPTION": "my_desc"}

    pg_ds = reconnect(pg_ds, update=1)
    with pg_ds.ExecuteSQL(
        "SELECT * FROM ogr_system_tables.metadata WHERE table_name = 'test_ogr_pg_metadata'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1
    lyr = pg_ds.GetLayerByName("test_ogr_pg_metadata")
    assert lyr.GetMetadata_Dict() == {
        "DESCRIPTION": "my_desc",
        "foo": "bar",
        "bar": "baz",
    }
    lyr.SetMetadata(None)

    pg_ds = reconnect(pg_ds, update=1)
    with pg_ds.ExecuteSQL(
        "SELECT * FROM ogr_system_tables.metadata WHERE table_name = 'test_ogr_pg_metadata'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0
    lyr = pg_ds.GetLayerByName("test_ogr_pg_metadata")
    assert lyr.GetMetadata_Dict() == {}


###############################################################################
# Test reading/writing metadata with a user with limited rights


@only_without_postgis
def test_ogr_pg_metadata_restricted_user(pg_ds):

    lyr = pg_ds.CreateLayer(
        "test_ogr_pg_metadata_restricted_user",
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES"],
    )
    lyr.SetMetadata({"foo": "bar"})

    pg_ds = reconnect(pg_ds, update=1)

    try:
        pg_ds.ExecuteSQL("CREATE ROLE test_ogr_pg_metadata_restricted_user")
        with pg_ds.ExecuteSQL("SELECT current_schema()") as lyr:
            f = lyr.GetNextFeature()
            current_schema = f.GetField(0)
        pg_ds.ExecuteSQL(
            f"GRANT ALL PRIVILEGES ON SCHEMA {current_schema} TO test_ogr_pg_metadata_restricted_user"
        )
        pg_ds.ExecuteSQL("SET ROLE test_ogr_pg_metadata_restricted_user")

        lyr = pg_ds.GetLayerByName("test_ogr_pg_metadata_restricted_user")
        gdal.ErrorReset()
        with gdal.quiet_errors():
            assert lyr.GetMetadata() == {}
        assert (
            gdal.GetLastErrorMsg()
            == "Table ogr_system_tables.metadata exists but user lacks USAGE privilege on ogr_system_tables schema"
        )

        pg_ds = reconnect(pg_ds, update=1)
        pg_ds.ExecuteSQL("DROP SCHEMA ogr_system_tables CASCADE")
        pg_ds.ExecuteSQL("SET ROLE test_ogr_pg_metadata_restricted_user")

        lyr = pg_ds.CreateLayer(
            "test_ogr_pg_metadata_restricted_user_bis",
            geom_type=ogr.wkbPoint,
            options=["OVERWRITE=YES"],
        )
        with gdal.quiet_errors():
            lyr.SetMetadata({"foo": "bar"})

            gdal.ErrorReset()
            pg_ds = reconnect(pg_ds, update=1)
        assert (
            gdal.GetLastErrorMsg()
            == "User lacks super user privilege to be able to create event trigger ogr_system_tables_event_trigger_for_metadata"
        )

    finally:
        pg_ds = reconnect(pg_ds, update=1)
        pg_ds.ExecuteSQL("DELLAYER:test_ogr_pg_metadata_restricted_user")
        pg_ds.ExecuteSQL("DELLAYER:test_ogr_pg_metadata_restricted_user_bis")
        with pg_ds.ExecuteSQL("SELECT CURRENT_USER") as lyr:
            f = lyr.GetNextFeature()
            current_user = f.GetField(0)
        pg_ds.ExecuteSQL(
            f"REASSIGN OWNED BY test_ogr_pg_metadata_restricted_user TO {current_user}"
        )
        pg_ds.ExecuteSQL("DROP OWNED BY test_ogr_pg_metadata_restricted_user")
        pg_ds.ExecuteSQL("DROP ROLE test_ogr_pg_metadata_restricted_user")


###############################################################################
# Test disabling writing metadata


@only_without_postgis
def test_ogr_pg_write_metadata_disabled(pg_ds):

    with gdal.config_option("OGR_PG_ENABLE_METADATA", "NO"):

        pg_ds = reconnect(pg_ds, update=1)
        lyr = pg_ds.CreateLayer(
            "test_ogr_pg_metadata", geom_type=ogr.wkbPoint, options=["OVERWRITE=YES"]
        )
        lyr.SetMetadata({"foo": "bar"})
        lyr.SetMetadataItem("bar", "baz")

        pg_ds = reconnect(pg_ds, update=1)

    lyr = pg_ds.GetLayerByName("test_ogr_pg_metadata")
    assert lyr.GetMetadata_Dict() == {}


###############################################################################
# Test append of several layers in PG_USE_COPY mode (#6411)


@only_without_postgis
def test_ogr_pg_85(pg_ds, tmp_vsimem):

    pg_ds.CreateLayer("ogr_pg_85_1")
    lyr = pg_ds.CreateLayer("ogr_pg_85_2")
    lyr.CreateField(ogr.FieldDefn("foo"))
    pg_ds.ReleaseResultSet(
        pg_ds.ExecuteSQL("SELECT 1")
    )  # make sure the layers are well created

    with gdal.config_option("PG_USE_COPY", "YES"):
        ds = reconnect(pg_ds, update=1)
        ds.GetLayerCount()
        ds.StartTransaction()
        lyr = ds.GetLayerByName("ogr_pg_85_1")
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
        lyr = ds.GetLayerByName("ogr_pg_85_2")
        feat_defn = lyr.GetLayerDefn()
        assert feat_defn.GetFieldCount() == 1
        f = ogr.Feature(feat_defn)
        assert lyr.CreateFeature(f) == 0
        ds.CommitTransaction()
        ds = None

        # Although test real ogr2ogr scenario
        # 0755 = 493
        gdal.FileFromMemBuffer(
            tmp_vsimem / "ogr_pg_85_1.csv",
            """id,foo
    1,1""",
        )

        gdal.FileFromMemBuffer(
            tmp_vsimem / "ogr_pg_85_2.csv",
            """id,foo
    1,1""",
        )

        gdal.VectorTranslate(
            pg_ds.GetDescription(),
            tmp_vsimem,
            accessMode="append",
        )

    lyr = pg_ds.GetLayerByName("ogr_pg_85_2")
    assert lyr.GetFeatureCount() == 2


###############################################################################
# Test OFTBinary


@only_without_postgis
@pytest.mark.parametrize("use_copy", ("YES", "NO"), ids=lambda x: f"PG_USE_COPY={x}")
def test_ogr_pg_86(use_copy, pg_ds):

    with gdal.config_option("PG_USE_COPY", use_copy):

        lyr = pg_ds.CreateLayer("ogr_pg_86")
        lyr.CreateField(ogr.FieldDefn("test", ogr.OFTBinary))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["test"] = b"\x30\x20"
        lyr.CreateFeature(f)
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetField(0) == "3020"


###############################################################################
# Test sequence updating (#7032)


@only_without_postgis
def test_ogr_pg_87(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_87")
    lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    lyr.CreateFeature(f)
    lyr.ResetReading()

    # Test updating of sequence after CreateFeatureViaCopy
    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_87")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert f.GetFID() == 11

    # Test updating of sequence after CreateFeatureViaInsert
    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("ogr_pg_87")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert f.GetFID() == 12


###############################################################################
# Test JSON subtype


def test_ogr_pg_json(pg_ds):

    lyr = pg_ds.CreateLayer("ogr_pg_json")
    fld_defn = ogr.FieldDefn("test_json", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["test_json"] = '{"a": "b"}'
    lyr.CreateFeature(f)
    lyr.ResetReading()

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayer("ogr_pg_json")
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = lyr.GetNextFeature()
    if f.GetField(0) != '{"a": "b"}':
        f.DumpReadable()
        pytest.fail()

    with pg_ds.ExecuteSQL("SELECT * FROM ogr_pg_json") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON


###############################################################################
# Test generated columns


def test_ogr_pg_generated_columns(pg_ds, pg_version):

    if pg_version < (12,):
        pytest.skip("Requires PostgreSQL >= 12")

    pg_ds.ExecuteSQL(
        "CREATE TABLE test_ogr_pg_generated_columns(id SERIAL PRIMARY KEY, unused VARCHAR, foo INTEGER, bar INTEGER GENERATED ALWAYS AS (foo+1) STORED)"
    )
    pg_ds.ExecuteSQL(
        "INSERT INTO test_ogr_pg_generated_columns VALUES (DEFAULT,NULL, 10,DEFAULT)"
    )

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayer("test_ogr_pg_generated_columns")
    f = lyr.GetNextFeature()

    assert f["foo"] == 10
    assert f["bar"] == 11
    f["foo"] = 20
    assert lyr.SetFeature(f) == 0

    f = lyr.GetFeature(1)
    assert f["foo"] == 20
    assert f["bar"] == 21
    f = None

    lyr.ResetReading()
    lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("unused"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = 30
    f["bar"] = 123456  # will be ignored
    assert lyr.CreateFeature(f) == 0

    f = lyr.GetFeature(2)
    assert f["foo"] == 30
    assert f["bar"] == 31

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayer("test_ogr_pg_generated_columns")
    with gdaltest.config_option("PG_USE_COPY", "YES"):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["foo"] = 40
        f["bar"] = 123456  # will be ignored
        assert lyr.CreateFeature(f) == 0

    f = lyr.GetFeature(3)
    assert f["foo"] == 40
    assert f["bar"] == 41


###############################################################################
# Test UNIQUE constraints


def test_ogr_pg_unique(pg_ds):

    # Create table to test UNIQUE constraints
    lyr = pg_ds.CreateLayer("test_ogr_pg_unique")

    fld_defn = ogr.FieldDefn("with_unique", ogr.OFTString)
    fld_defn.SetUnique(True)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("with_unique_and_explicit_unique_idx", ogr.OFTString)
    fld_defn.SetUnique(True)
    lyr.CreateField(fld_defn)

    lyr.CreateField(ogr.FieldDefn("without_unique", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("unique_on_several_col1", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("unique_on_several_col2", ogr.OFTString))
    pg_ds.ExecuteSQL(
        "CREATE UNIQUE INDEX unique_idx_with_unique_and_explicit_unique_idx ON test_ogr_pg_unique(with_unique_and_explicit_unique_idx)"
    )
    pg_ds.ExecuteSQL(
        "CREATE UNIQUE INDEX unique_idx_unique_constraints ON test_ogr_pg_unique(unique_on_several_col1, unique_on_several_col2)"
    )

    # Check after re-opening
    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("test_ogr_pg_unique")
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("with_unique")).IsUnique()
    assert feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex("with_unique_and_explicit_unique_idx")
    ).IsUnique()
    assert not feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex("without_unique")
    ).IsUnique()
    assert not feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex("unique_on_several_col1")
    ).IsUnique()
    assert not feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex("unique_on_several_col2")
    ).IsUnique()

    # Test AlterFieldDefn()

    # Unchanged state: no unique
    fld_defn = ogr.FieldDefn("without_unique", ogr.OFTString)
    fld_defn.SetUnique(False)
    assert (
        lyr.AlterFieldDefn(
            feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert not feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex(fld_defn.GetName())
    ).IsUnique()

    # Unchanged state: unique
    fld_defn = ogr.FieldDefn("with_unique", ogr.OFTString)
    fld_defn.SetUnique(True)
    assert (
        lyr.AlterFieldDefn(
            feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex(fld_defn.GetName())
    ).IsUnique()

    # no unique -> unique
    fld_defn = ogr.FieldDefn("without_unique", ogr.OFTString)
    fld_defn.SetUnique(True)
    assert (
        lyr.AlterFieldDefn(
            feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex(fld_defn.GetName())
    ).IsUnique()

    # unique -> no unique : unsupported
    fld_defn = ogr.FieldDefn("without_unique", ogr.OFTString)
    fld_defn.SetUnique(False)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            lyr.AlterFieldDefn(
                feat_defn.GetFieldIndex(fld_defn.GetName()),
                fld_defn,
                ogr.ALTER_UNIQUE_FLAG,
            )
            == ogr.OGRERR_NONE
        )
    assert gdal.GetLastErrorMsg() != ""
    assert feat_defn.GetFieldDefn(
        feat_defn.GetFieldIndex(fld_defn.GetName())
    ).IsUnique()

    # Check after re-opening
    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayerByName("test_ogr_pg_unique")
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex("without_unique")).IsUnique()


###############################################################################
# Test UUID datatype support


def test_ogr_pg_uuid(pg_ds):

    lyr = pg_ds.CreateLayer("test_ogr_pg_uuid")

    fd = ogr.FieldDefn("uid", ogr.OFTString)
    fd.SetSubType(ogr.OFSTUUID)

    assert lyr.CreateField(fd) == 0

    lyr.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f["uid"] = "6f9619ff-8b86-d011-b42d-00c04fc964ff"
    lyr.CreateFeature(f)
    lyr.CommitTransaction()

    test_ds = reconnect(pg_ds, update=0)
    lyr = test_ds.GetLayer("test_ogr_pg_uuid")
    fd = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fd.GetType() == ogr.OFTString
    assert fd.GetSubType() == ogr.OFSTUUID
    f = lyr.GetNextFeature()

    assert f.GetField(0) == "6f9619ff-8b86-d011-b42d-00c04fc964ff"


###############################################################################
# Test AbortSQL


def test_abort_sql(pg_ds):
    def abortAfterDelay():
        print("Aborting SQL...")
        assert pg_ds.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay)
    t.start()

    start = time.time()

    # Long running query
    sql = "SELECT pg_sleep(3)"
    pg_ds.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1

    # Same test with a GDAL dataset
    ds2 = reconnect(pg_ds)

    def abortAfterDelay2():
        print("Aborting SQL...")
        assert ds2.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay2)
    t.start()

    start = time.time()

    # Long running query
    ds2.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1


###############################################################################
# Test postgresql:// URL


def test_ogr_pg_url(pg_autotest_ds, pg_version):

    if pg_version < (9, 3):
        pytest.skip()

    params = pg_autotest_ds.GetDescription().strip("PG:").split(" ")
    url = "postgresql://?" + "&".join(params)

    ds = ogr.Open(url)
    assert ds is not None

    ds = ogr.Open("PG:" + url)
    assert ds is not None

    # Test postgresql:// with open options
    params_without_dbname = []
    open_options = ["active_schema=public"]
    for param in params:
        if param.startswith("dbname="):
            open_options.append("DBNAME=" + param[len("dbname=") :])
        elif param.startswith("port="):
            open_options.append("PORT=" + param[len("port=") :])
        else:
            params_without_dbname.append(param)

    url = "postgresql://"
    if params_without_dbname:
        url += "?" + "&".join(params_without_dbname)
    ds = gdal.OpenEx(url, gdal.OF_VECTOR, open_options=open_options)
    assert ds is not None


###############################################################################
# Test error on EndCopy()


@only_with_postgis
def test_ogr_pg_copy_error(pg_ds):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer(
        "layer_polygon_with_multipolygon", geom_type=ogr.wkbPolygon
    )
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOLYGON(((0 0,0 1,1 1,0 0)))"))
    src_lyr.CreateFeature(f)
    with gdal.ExceptionMgr():
        with pytest.raises(RuntimeError, match="COPY statement failed"):
            gdal.VectorTranslate(pg_ds.GetDescription(), src_ds)


###############################################################################
# Test gdal.VectorTranslate with GEOM_TYPE=geography and a named geometry column


@only_with_postgis
def test_ogr_pg_vector_translate_geography(pg_ds):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer(
        "test_ogr_pg_vector_translate_geography", geom_type=ogr.wkbNone
    )
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("foo", ogr.wkbPolygon))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,0 0))"))
    src_lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate(
        pg_ds.GetDescription(),
        src_ds,
        layerCreationOptions=["GEOM_TYPE=geography"],
    )
    assert out_ds
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM geography_columns WHERE "
        + "f_table_name = 'test_ogr_pg_vector_translate_geography' AND "
        + "f_geography_column = 'foo'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    out_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test AlterGeomFieldDefn()


@only_with_postgis
def test_ogr_pg_alter_geom_field_defn(pg_ds):

    srs_4326 = osr.SpatialReference()
    srs_4326.ImportFromEPSG(4326)
    lyr = pg_ds.CreateLayer(
        "ogr_pg_alter_geom_field_defn", geom_type=ogr.wkbUnknown, srs=srs_4326
    )
    assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    new_geom_field_defn = ogr.GeomFieldDefn("new_geomfield_name", ogr.wkbPoint)
    srs_4269 = osr.SpatialReference()
    srs_4269.ImportFromEPSG(4269)
    new_geom_field_defn.SetSpatialRef(srs_4269)
    new_geom_field_defn.SetNullable(False)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetGeometryColumn() == "new_geomfield_name"
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"
    assert not lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()

    test_ds = reconnect(pg_ds, update=0)
    test_lyr = test_ds.GetLayer("ogr_pg_alter_geom_field_defn")
    assert test_lyr.GetGeometryColumn() == "new_geomfield_name"
    assert test_lyr.GetGeomType() == ogr.wkbPoint
    assert test_lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"
    assert not test_lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()
    test_ds = None

    new_geom_field_defn.SetSpatialRef(None)
    new_geom_field_defn.SetNullable(True)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is None
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()

    test_ds = reconnect(pg_ds, update=0)
    test_lyr = test_ds.GetLayer("ogr_pg_alter_geom_field_defn")
    assert test_lyr.GetGeometryColumn() == "new_geomfield_name"
    assert test_lyr.GetGeomType() == ogr.wkbPoint
    assert test_lyr.GetSpatialRef() is None
    assert test_lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()
    test_ds = None

    new_geom_field_defn.SetSpatialRef(srs_4269)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"

    test_ds = reconnect(pg_ds, update=0)
    test_lyr = test_ds.GetLayer("ogr_pg_alter_geom_field_defn")
    assert test_lyr.GetGeometryColumn() == "new_geomfield_name"
    assert test_lyr.GetGeomType() == ogr.wkbPoint
    assert test_lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"
    assert test_lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()
    test_ds = None

    # Error cases
    with gdal.quiet_errors():
        # Invalid index
        assert (
            lyr.AlterGeomFieldDefn(
                -1, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

        # cannot change geometry type if data has incompatible geometries
        new_geom_field_defn = ogr.GeomFieldDefn(
            "another_new_geomfield_name", ogr.wkbLineString
        )
        assert (
            lyr.AlterGeomFieldDefn(
                0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

        new_geom_field_defn = ogr.GeomFieldDefn("new_geomfield_name", ogr.wkbPoint)
        srs_with_coord_epoch = osr.SpatialReference()
        srs_with_coord_epoch.ImportFromEPSG(4269)
        srs_with_coord_epoch.SetCoordinateEpoch(2022)
        new_geom_field_defn.SetSpatialRef(srs_with_coord_epoch)
        assert (
            lyr.AlterGeomFieldDefn(
                0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

    test_ds = reconnect(pg_ds, update=0)
    test_lyr = test_ds.GetLayer("ogr_pg_alter_geom_field_defn")
    assert test_lyr.GetGeometryColumn() == "new_geomfield_name"
    assert test_lyr.GetGeomType() == ogr.wkbPoint
    assert test_lyr.GetSpatialRef().GetAuthorityCode(None) == "4269"
    assert test_lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable()
    test_ds = None


###############################################################################


@only_with_postgis
def test_ogr_pg_get_geometry_types(pg_ds):
    """Test Layer.GetGeometryTypes()"""

    lyr = pg_ds.CreateLayer("ogr_pg_get_geometry_types_XY", geom_type=ogr.wkbUnknown)

    assert lyr.GetGeometryTypes() == {}

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 1}

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(callback=lambda x, y, z: 1) == {ogr.wkbNone: 2}

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 2, ogr.wkbPoint: 1}
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
    }
    lyr.SetAttributeFilter("TRUE")
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 2, ogr.wkbPoint: 1}
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
    }
    lyr.SetAttributeFilter(None)
    lyr.SetAttributeFilter("FALSE")
    assert lyr.GetGeometryTypes() == {}
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {}
    lyr.SetAttributeFilter(None)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }
    assert lyr.GetGeometryTypes(flags=ogr.GGT_STOP_IF_MIXED) == {
        ogr.wkbNone: 0,
        ogr.wkbPoint: 0,
        ogr.wkbPolygon: 0,
    }
    lyr.SetAttributeFilter("TRUE")
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }
    assert lyr.GetGeometryTypes(flags=ogr.GGT_STOP_IF_MIXED) == {
        ogr.wkbNone: 0,
        ogr.wkbPoint: 0,
        ogr.wkbPolygon: 0,
    }
    lyr.SetAttributeFilter(None)
    lyr.SetAttributeFilter("FALSE")
    assert lyr.GetGeometryTypes(flags=ogr.GGT_STOP_IF_MIXED) == {}
    lyr.SetAttributeFilter(None)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0,1 1)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
    }
    res = lyr.GetGeometryTypes(geom_field=0, flags=ogr.GGT_STOP_IF_MIXED)
    assert ogr.wkbNone in res
    assert len(res) == 3

    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_PG_DEBUG_GGT_CANCEL", "YES"):
            with pytest.raises(Exception):
                lyr.GetGeometryTypes(callback=lambda x, y, z: 0)

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            lyr.GetGeometryTypes(geom_field=2)

    lyr = pg_ds.CreateLayer(
        "ogr_pg_get_geometry_types_XYZ",
        geom_type=ogr.wkbUnknown,
        options=["DIM=XYZ"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z(0 0 0)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbPoint25D: 1}

    lyr = pg_ds.CreateLayer(
        "ogr_pg_get_geometry_types_XYM",
        geom_type=ogr.wkbUnknown,
        options=["DIM=XYM"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT M(0 0 0)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbPointM: 1}

    lyr = pg_ds.CreateLayer(
        "ogr_pg_get_geometry_types_XYZM",
        geom_type=ogr.wkbUnknown,
        options=["DIM=XYZM"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT ZM(0 0 0 0)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbPointZM: 1}

    lyr = pg_ds.CreateLayer(
        "ogr_pg_get_geometry_types_geomcollectionz_as_tinz",
        geom_type=ogr.wkbUnknown,
        options=["DIM=XYZ"],
    )
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {}
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION Z (TIN Z(((0 0 0,0 1 0,1 1 0,0 0 0))))"
        )
    )
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {ogr.wkbTINZ: 1}
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 1,
        ogr.wkbTINZ: 1,
    }
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z(0 0 0)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 1,
        ogr.wkbPoint25D: 1,
        ogr.wkbTINZ: 1,
    }
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("TIN Z(((0 0 0,0 1 0,1 1 0,0 0 0)))"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 1,
        ogr.wkbPoint25D: 1,
        ogr.wkbTINZ: 2,
    }
    lyr.SetAttributeFilter("TRUE")
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 1,
        ogr.wkbPoint25D: 1,
        ogr.wkbTINZ: 2,
    }
    lyr.SetAttributeFilter(None)


###############################################################################


@only_with_postgis
def test_ogr_pg_insert_single_feature_of_fid_0(pg_ds):

    lyr = pg_ds.CreateLayer(
        "ogr_pg_insert_single_feature_of_fid_0", geom_type=ogr.wkbUnknown
    )
    lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "0"
    f.SetFID(0)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert pg_ds.SyncToDisk() == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "1"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1


###############################################################################


@only_without_postgis
def test_ogr_pg_temp(pg_ds):

    layer_name = "test_ogr_pg_temp"
    lyr = pg_ds.CreateLayer(
        layer_name, geom_type=ogr.wkbPoint, options=["TEMPORARY=ON"]
    )
    lyr.CreateField(ogr.FieldDefn("foo"))
    sql_lyr = pg_ds.ExecuteSQL(
        "SELECT TRUE FROM pg_class WHERE relpersistence = 't' AND oid = '"
        + layer_name
        + "'::REGCLASS"
    )
    assert sql_lyr.GetNextFeature() is not None
    pg_ds.ReleaseResultSet(sql_lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    f["foo"] = "bar"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "wkb_geometry"


###############################################################################


@only_without_postgis
def test_ogr_pg_skip_views(pg_ds):

    view_name = "test_ogr_pg_skip_views"

    rs = pg_ds.ExecuteSQL(
        f"CREATE VIEW {view_name} AS SELECT 55 AS fid, 'POINT EMPTY'::geometry AS geom"
    )
    pg_ds.ReleaseResultSet(rs)

    pg_ds = reconnect(pg_ds, update=1)

    assert ogr_pg_check_layer_in_list(pg_ds, view_name)

    pg_ds = reconnect(pg_ds, open_options={"SKIP_VIEWS": "YES"})

    assert not ogr_pg_check_layer_in_list(pg_ds, view_name)

    with gdaltest.config_option("PG_SKIP_VIEWS", "YES"):
        pg_ds = reconnect(pg_ds, update=1)
        assert not ogr_pg_check_layer_in_list(pg_ds, view_name)


###############################################################################
# Test setting / getting / altering field comments


def test_ogr_pg_field_comment(pg_ds):

    lyr = pg_ds.CreateLayer("test_ogr_pg_field_comment")
    lyr.CreateField(ogr.FieldDefn("without_comment", ogr.OFTString))
    # Test field comment in deferred table creation
    fld_defn = ogr.FieldDefn("with_comment", ogr.OFTString)
    fld_defn.SetComment("field with comment")
    lyr.CreateField(fld_defn)

    # Force table creation
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == ogr.OGRERR_NONE
    feat = None

    # Test field comment after table creation
    fld_defn = ogr.FieldDefn("with_comment2", ogr.OFTString)
    fld_defn.SetComment("field with comment 2")
    lyr.CreateField(fld_defn)

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayer("test_ogr_pg_field_comment")
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("without_comment")
    )
    assert fld_defn.GetComment() == ""
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment")
    )
    assert fld_defn.GetComment() == "field with comment"
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment2")
    )
    assert fld_defn.GetComment() == "field with comment 2"

    fld_defn = ogr.FieldDefn("with_comment_now", ogr.OFTString)
    fld_defn.SetComment("comment added")
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("without_comment"),
            fld_defn,
            ogr.ALTER_ALL_FLAG,
        )
        == ogr.OGRERR_NONE
    )
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment_now")
    )
    assert fld_defn.GetComment() == "comment added"

    fld_defn = ogr.FieldDefn("with_comment", ogr.OFTString)
    fld_defn.SetComment("comment changed")
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("with_comment"),
            fld_defn,
            ogr.ALTER_COMMENT_FLAG,
        )
        == ogr.OGRERR_NONE
    )
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment")
    )
    assert fld_defn.GetComment() == "comment changed"

    fld_defn = ogr.FieldDefn("with_comment2", ogr.OFTString)
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("with_comment2"),
            fld_defn,
            ogr.ALTER_COMMENT_FLAG,
        )
        == ogr.OGRERR_NONE
    )
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment2")
    )
    assert fld_defn.GetComment() == ""

    pg_ds = reconnect(pg_ds, update=1)
    lyr = pg_ds.GetLayer("test_ogr_pg_field_comment")
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment_now")
    )
    assert fld_defn.GetComment() == "comment added"
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment")
    )
    assert fld_defn.GetComment() == "comment changed"
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("with_comment")
    )
    assert fld_defn.GetComment() == "comment changed"


###############################################################################
# Test long identifiers


def test_ogr_pg_long_identifiers(pg_ds):

    long_name = "test_" + ("X" * 64) + "_long_name"
    short_name = "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_3ba7c630"
    assert len(short_name) == 63
    with gdal.quiet_errors():
        lyr = pg_ds.CreateLayer(long_name)
    assert lyr.GetName() == short_name
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    long_name2 = "test_" + ("X" * 64) + "_long_name2"
    short_name2 = "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_bb4afe1c"
    assert len(short_name2) == 63
    with gdal.quiet_errors():
        lyr = pg_ds.CreateLayer(long_name2)
    assert lyr.GetName() == short_name2
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    long_name3 = "test_" + ("X" * (64 - len("test_")))
    assert len(long_name3) == 64
    short_name3 = "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_b7ebb17c"
    assert len(short_name3) == 63
    with gdal.quiet_errors():
        lyr = pg_ds.CreateLayer(long_name3)
    assert lyr.GetName() == short_name3
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    long_name4 = "test_" + ("X" * (63 - len("test_")))
    assert len(long_name4) == 63
    short_name4 = "test_" + ("x" * (63 - len("test_")))
    with gdal.quiet_errors():
        lyr = pg_ds.CreateLayer(long_name4)
    assert lyr.GetName() == short_name4
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    pg_ds = reconnect(pg_ds, update=1)

    got_lyr = pg_ds.GetLayerByName(short_name)
    assert got_lyr
    assert got_lyr.GetName() == short_name

    got_lyr = pg_ds.GetLayerByName(short_name2)
    assert got_lyr
    assert got_lyr.GetName() == short_name2

    got_lyr = pg_ds.GetLayerByName(short_name3)
    assert got_lyr
    assert got_lyr.GetName() == short_name3

    got_lyr = pg_ds.GetLayerByName(short_name4)
    assert got_lyr
    assert got_lyr.GetName() == short_name4


###############################################################################
# Test extent 3D


@only_with_postgis
def test_extent3d(pg_ds):

    # Create a 3D layer
    lyr = pg_ds.CreateLayer("test_extent3d", geom_type=ogr.wkbPoint25D)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3 4)"))
    lyr.CreateFeature(f)

    assert lyr.GetFeatureCount() == 2
    extent = lyr.GetExtent3D()
    assert extent == (0.0, 2.0, 1.0, 3.0, 2.0, 4.0)

    # Create a 2D layer
    lyr = pg_ds.CreateLayer("test_extent2d", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    lyr.CreateFeature(f)

    assert lyr.GetFeatureCount() == 2
    extent = lyr.GetExtent3D()
    assert extent == (0.0, 2.0, 1.0, 3.0, float("inf"), float("-inf"))

    # Create a geography layer
    lyr = pg_ds.CreateLayer(
        "test_extent3d_geography",
        geom_type=ogr.wkbPoint25D,
        options=["GEOM_TYPE=geography"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3 4)"))
    lyr.CreateFeature(f)

    assert lyr.GetFeatureCount() == 2
    extent = lyr.GetExtent3D()
    assert extent == (0.0, 2.0, 1.0, 3.0, 2.0, 4.0)


###############################################################################
# Test CreateLayer() and schema name with a different case


@gdaltest.enable_exceptions()
def test_ogr_pg_schema_case_createlayer(pg_ds, tmp_schema):

    tmp_schema_uppercase = tmp_schema.upper()

    with pytest.raises(Exception, match='Schema "unexisting_schema" does not exist'):
        pg_ds.CreateLayer("unexisting_schema.layer")

    lyr = pg_ds.CreateLayer(
        f"{tmp_schema_uppercase}.test_ogr_pg_schema_case_createlayer"
    )
    assert lyr
    assert lyr.GetName() == f"{tmp_schema}.test_ogr_pg_schema_case_createlayer"
    # Force deferred creation to run
    lyr.ResetReading()

    pg_ds = reconnect(pg_ds, update=1)
    assert pg_ds.GetLayerByName(
        f"{tmp_schema_uppercase}.test_ogr_pg_schema_case_createlayer"
    )

    pg_ds.ExecuteSQL(f'CREATE SCHEMA "{tmp_schema_uppercase}"')
    try:
        lyr = pg_ds.CreateLayer(f"{tmp_schema_uppercase}.another_layer")
        assert lyr
        assert lyr.GetName() == f"{tmp_schema_uppercase}.another_layer"

        tmp_schema_mixedcase = (
            tmp_schema[0 : len(tmp_schema) // 2]
            + tmp_schema_uppercase[len(tmp_schema) // 2 :]
        )
        with pytest.raises(Exception, match="Several schemas exist whose name matches"):
            pg_ds.CreateLayer(f"{tmp_schema_mixedcase}.yet_another_layer")
    finally:
        pg_ds.ExecuteSQL(f'DROP SCHEMA "{tmp_schema_uppercase}" CASCADE')


###############################################################################
# Test LAUNDER=YES


@gdaltest.enable_exceptions()
def test_ogr_pg_LAUNDER_YES(pg_ds, tmp_schema):

    eacute = b"\xc3\xa9".decode("utf-8")
    lyr = pg_ds.CreateLayer(f"{tmp_schema}.a" + eacute + "#", options=["LAUNDER=YES"])
    assert lyr.GetName() == f"{tmp_schema}.a" + eacute + "_"
    lyr.CreateField(ogr.FieldDefn("b" + eacute + "#"))
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == "b" + eacute + "_"


###############################################################################
# Test LAUNDER=NO


@gdaltest.enable_exceptions()
def test_ogr_pg_LAUNDER_NO(pg_ds, tmp_schema):

    eacute = b"\xc3\xa9".decode("utf-8")
    lyr = pg_ds.CreateLayer(f"{tmp_schema}.a" + eacute + "#", options=["LAUNDER=NO"])
    assert lyr.GetName() == f"{tmp_schema}.a" + eacute + "#"
    lyr.CreateField(ogr.FieldDefn("b" + eacute + "#"))
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == "b" + eacute + "#"


###############################################################################
# Test LAUNDER_ASCII


@gdaltest.enable_exceptions()
def test_ogr_pg_LAUNDER_ASCII(pg_ds, tmp_schema):

    eacute = b"\xc3\xa9".decode("utf-8")
    lyr = pg_ds.CreateLayer(f"{tmp_schema}.a" + eacute, options=["LAUNDER_ASCII=YES"])
    assert lyr.GetName() == f"{tmp_schema}.ae"
    lyr.CreateField(ogr.FieldDefn("b" + eacute))
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == "be"


###############################################################################
# Test ignored GEOMETRY_NAME on non-PostGIS enabled database


@only_without_postgis
def test_ogr_pg_no_postgis_GEOMETRY_NAME(pg_ds):

    with gdal.quiet_errors():
        pg_ds.CreateLayer(
            "test_ogr_pg_no_postgis_GEOMETRY_NAME",
            geom_type=ogr.wkbPoint,
            options=["GEOMETRY_NAME=foo"],
        )
        assert (
            gdal.GetLastErrorMsg()
            == "GEOMETRY_NAME=foo ignored, and set instead to 'wkb_geometry' as it is the only geometry column name recognized for non-PostGIS enabled databases."
        )


###############################################################################
# Test ignored conflicts


@only_without_postgis
def test_ogr_pg_skip_conflicts(pg_ds):
    pg_ds.ExecuteSQL(
        "CREATE TABLE test_ogr_skip_conflicts(id SERIAL PRIMARY KEY, gml_id character(16), beginnt character(20), UNIQUE(gml_id, beginnt))"
    )

    with gdal.config_option("OGR_PG_SKIP_CONFLICTS", "YES"):
        # OGR_PG_SKIP_CONFLICTS and OGR_PG_RETRIEVE_FID cannot be used at the same time
        with gdal.config_option("OGR_PG_RETRIEVE_FID", "YES"):
            pg_ds = reconnect(pg_ds, update=1)
            lyr = pg_ds.GetLayerByName("test_ogr_skip_conflicts")
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat["gml_id"] = "DERPLP0300000cG3"
            feat["beginnt"] = "2020-07-10T04:48:14Z"
            with gdal.quiet_errors():
                assert lyr.CreateFeature(feat) != ogr.OGRERR_NONE

    with gdal.config_option("OGR_PG_RETRIEVE_FID", "NO"):
        pg_ds = reconnect(pg_ds, update=1)
        lyr = pg_ds.GetLayerByName("test_ogr_skip_conflicts")

        assert lyr.GetFeatureCount() == 0

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat["gml_id"] = "DERPLP0300000cG3"
        feat["beginnt"] = "2020-07-10T04:48:14Z"
        assert lyr.CreateFeature(feat) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 1

        # Insert w/o OGR_PG_SKIP_CONFLICTS=YES succeeds, but doesn't add a feature
        with gdal.config_option("OGR_PG_SKIP_CONFLICTS", "YES"):
            pg_ds = reconnect(pg_ds, update=1)
            lyr = pg_ds.GetLayerByName("test_ogr_skip_conflicts")

            assert lyr.GetFeatureCount() == 1

            feat = ogr.Feature(lyr.GetLayerDefn())
            feat["gml_id"] = "DERPLP0300000cG3"
            feat["beginnt"] = "2020-07-10T04:48:14Z"
            assert lyr.CreateFeature(feat) == ogr.OGRERR_NONE
            assert lyr.GetFeatureCount() == 1

        # Other feature succeeds and increments the feature count
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat["gml_id"] = "DERPLP0300000cG4"
        feat["beginnt"] = "2020-07-10T04:48:14Z"
        assert lyr.CreateFeature(feat) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 2


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/10311


@only_without_postgis
@gdaltest.enable_exceptions()
def test_ogr_pg_ogr2ogr_with_multiple_dotted_table_name(pg_ds):

    tmp_schema = "tmp_schema_issue_10311"
    pg_ds.ExecuteSQL(f'CREATE SCHEMA "{tmp_schema}"')
    try:
        src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
        lyr = src_ds.CreateLayer(tmp_schema + ".table1", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("str"))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        lyr.CreateFeature(f)
        lyr = src_ds.CreateLayer(tmp_schema + ".table2", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("str"))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "bar"
        lyr.CreateFeature(f)

        gdal.VectorTranslate(pg_ds.GetDescription(), src_ds)

        pg_ds = reconnect(pg_ds)
        lyr = pg_ds.GetLayerByName(tmp_schema + ".table1")
        assert lyr.GetFeatureCount() == 1
        lyr = pg_ds.GetLayerByName(tmp_schema + ".table2")
        assert lyr.GetFeatureCount() == 1

    finally:
        pg_ds.ExecuteSQL(f'DROP SCHEMA "{tmp_schema}" CASCADE')


###############################################################################
# Test scenario of https://lists.osgeo.org/pipermail/gdal-dev/2024-October/059608.html
# and bugfix of https://github.com/OSGeo/gdal/issues/11386


@only_without_postgis
@gdaltest.enable_exceptions()
def test_ogr_pg_empty_search_path(pg_ds):

    with pg_ds.ExecuteSQL("SHOW search_path") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        old_search_path = f.GetField(0)
        old_search_path = old_search_path.replace(
            "test_ogr_pg_empty_search_path_no_postgis, ", ""
        )

    with pg_ds.ExecuteSQL("SELECT CURRENT_USER") as lyr:
        f = lyr.GetNextFeature()
        current_user = f.GetField(0)

    pg_ds.ExecuteSQL(f"ALTER ROLE {current_user} SET search_path = ''")
    try:
        ds = reconnect(pg_ds, update=1)

        with ds.ExecuteSQL("SHOW search_path") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            new_search_path = f.GetField(0)
            assert (
                new_search_path
                == 'test_ogr_pg_empty_search_path_no_postgis, "", public'
            )

    finally:
        ds.ExecuteSQL(f"ALTER ROLE {current_user} SET search_path = {old_search_path}")

    pg_ds.ExecuteSQL(f"ALTER ROLE {current_user} SET search_path = '', '$user'")
    try:
        ds = reconnect(pg_ds, update=1)

        with ds.ExecuteSQL("SHOW search_path") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            new_search_path = f.GetField(0)
            assert (
                new_search_path
                == 'test_ogr_pg_empty_search_path_no_postgis, "", "$user", public'
            )

    finally:
        ds.ExecuteSQL(f"ALTER ROLE {current_user} SET search_path = {old_search_path}")


###############################################################################
# Test appending to a layer where a field name was truncated to 63 characters.


@only_without_postgis
@gdaltest.enable_exceptions()
def test_ogr_pg_findfield(pg_ds):

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("test_very_long_field_name")
    src_lyr.CreateField(
        ogr.FieldDefn(
            "veeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeryyyyyyyyyyyyyyyyyyyyyyloooooooooooooong"
        )
    )
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField(0, "foo")
    src_lyr.CreateFeature(f)

    with gdal.quiet_errors():
        gdal.VectorTranslate(pg_ds.GetDescription(), src_ds)

    with gdal.quiet_errors():
        gdal.VectorTranslate(pg_ds.GetDescription(), src_ds, accessMode="append")

    lyr = pg_ds.GetLayerByName("test_very_long_field_name")
    assert [f.GetField(0) for f in lyr] == ["foo", None]

    with gdal.quiet_errors():
        gdal.VectorTranslate(
            pg_ds.GetDescription(),
            src_ds,
            accessMode="append",
            relaxedFieldNameMatch=True,
        )
    assert [f.GetField(0) for f in lyr] == ["foo", None, "foo"]


###############################################################################
# Test that we open the PG driver and not the PostGISRaster one with
# "gdal vector pipeline"


@gdaltest.enable_exceptions()
def test_ogr_pg_gdal_vector_pipeline(pg_ds):

    pg_ds.CreateLayer("test")

    with gdal.Run(
        "vector",
        "pipeline",
        {
            "pipeline": "read ! write",
            "input": pg_ds,
            "output_format": "stream",
            "output": "",
        },
    ) as alg:
        assert alg.Output().GetLayerCount() == 1


###############################################################################
# Test field content truncation related to SetWidth()


@gdaltest.enable_exceptions()
@only_without_postgis
def test_ogr_pg_field_truncation(pg_ds):

    lyr = pg_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("field", ogr.OFTString)
    fld_defn.SetWidth(5)
    lyr.CreateField(fld_defn)

    ds = reconnect(pg_ds, update=1)
    lyr = ds.GetLayerByName("test")

    f = ogr.Feature(lyr.GetLayerDefn())
    f["field"] = b"abcd\xc3\xa9f".decode("UTF-8")
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["field"] == b"abcd\xc3\xa9".decode("UTF-8")


###############################################################################
# Test real geometry intersection in spatial filter


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("GEOM_TYPE", ["geometry", "geography"])
@pytest.mark.parametrize(
    "open_options", [None, ["SPATIAL_FILTER_INTERSECTION=DATABASE"]]
)
@pytest.mark.require_geos
def test_ogr_pg_geometry_intersection_spatial_filter(
    pg_ds, use_postgis, GEOM_TYPE, open_options
):

    ds = reconnect(pg_ds, open_options=open_options)

    if use_postgis:
        srs = osr.SpatialReference(epsg=4326)
        lyr = ds.CreateLayer("test", srs, options=["GEOM_TYPE=" + GEOM_TYPE])
    else:
        lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0.5 0.5)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0.9 0.5)"))
    lyr.CreateFeature(f)

    lyr.ResetReading()

    # Pacman style polygon: almost a square except it doesn't include 0.9, 0.5
    lyr.SetSpatialFilter(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((0 0,0 1,1 1,1 0.6,0.8 0.6,0.8 0.4,1 0.4,1 0,0 0))"
        )
    )
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5)"
    f = lyr.GetNextFeature()
    assert f is None

    lyr.SetSpatialFilter(None)

    with ds.ExecuteSQL("SELECT * FROM test") as sql_lyr:
        sql_lyr.SetSpatialFilter(
            ogr.CreateGeometryFromWkt(
                "POLYGON ((0 0,0 1,1 1,1 0.6,0.8 0.6,0.8 0.4,1 0.4,1 0,0 0))"
            )
        )
        f = sql_lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (0.5 0.5)"
        f = sql_lyr.GetNextFeature()
        assert f is None
