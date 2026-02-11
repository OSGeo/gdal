#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test various OGR SQL support options.
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

import math
import os
import shutil

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Test ExecuteSQL()


@pytest.mark.parametrize("use_gdal", [True, False])
def test_ogr_sql_execute_sql(tmp_path, use_gdal):

    shutil.copy("data/poly.shp", tmp_path / "test_ogr_sql_execute_sql.shp")
    shutil.copy("data/poly.shx", tmp_path / "test_ogr_sql_execute_sql.shx")

    def get_dataset():
        return (
            gdal.OpenEx(tmp_path / "test_ogr_sql_execute_sql.shp")
            if use_gdal
            else ogr.Open(tmp_path / "test_ogr_sql_execute_sql.shp")
        )

    def check_historic_way():
        ds = get_dataset()

        # "Manual" / historic way of using ExecuteSQL() / ReleaseResultSet()
        lyr = ds.ExecuteSQL("SELECT * FROM test_ogr_sql_execute_sql")
        assert lyr.GetFeatureCount() == 10
        ds.ReleaseResultSet(lyr)

        # lyr invalidated
        with pytest.raises(Exception):
            lyr.GetName()

        # lyr invalidated
        with pytest.raises(Exception):
            ds.ReleaseResultSet(lyr)

        ds = None

    check_historic_way()

    def check_context_manager():
        ds = get_dataset()

        # ExecuteSQL() as context manager
        with ds.ExecuteSQL("SELECT * FROM test_ogr_sql_execute_sql") as lyr:
            assert lyr.GetFeatureCount() == 10

        # lyr invalidated
        with pytest.raises(Exception):
            lyr.GetName()

        ds = None

    check_context_manager()

    # ExecuteSQL() with keep_ref_on_ds=True
    def get_lyr():
        return get_dataset().ExecuteSQL(
            "SELECT * FROM test_ogr_sql_execute_sql", keep_ref_on_ds=True
        )

    with get_lyr() as lyr:
        assert lyr.GetFeatureCount() == 10

    # lyr invalidated
    with pytest.raises(Exception):
        lyr.GetName()

    # This leaks memory
    if not gdaltest.is_travis_branch("sanitize"):
        assert get_lyr().GetFeatureCount() == 10

    # Check that we can actually remove the files (i.e. references on dataset have been dropped)
    os.unlink(tmp_path / "test_ogr_sql_execute_sql.shp")
    os.unlink(tmp_path / "test_ogr_sql_execute_sql.shx")


@pytest.mark.require_driver("SQLite")
def test_ogr_sql_execute_sql_empty_database(tmp_vsimem):

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmp_vsimem / "test.sqlite")

    with ds.ExecuteSQL("SELECT sqlite_version() AS version") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert type(f["version"]) is str


###############################################################################
# Test invalid use of ReleaseResultSet()


@pytest.mark.parametrize("use_gdal", [True, False])
def test_ogr_sql_invalid_release_result_set(use_gdal):

    ds = gdal.OpenEx("data/poly.shp") if use_gdal else ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)
    with pytest.raises(Exception):
        ds.ReleaseResultSet(lyr)


@pytest.fixture
def data_ds():
    with ogr.Open("data") as ds:
        yield ds


###############################################################################
# Test an unrecognized dialect name


def test_ogr_sql_unrecognized_dialect(data_ds):
    class MyHandler:
        def __init__(self):
            self.warning_raised = False

        def callback(self, err_type, err_no, err_msg):
            if (
                err_type == gdal.CE_Warning
                and "Dialect 'unknown' is unsupported" in err_msg
            ):
                self.warning_raised = True

    my_error_handler = MyHandler()
    with gdaltest.error_handler(my_error_handler.callback):
        with data_ds.ExecuteSQL(
            "SELECT * FROM poly WHERE eas_id < 167", dialect="unknown"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount(force=1) == 3
    assert my_error_handler.warning_raised


###############################################################################
# Test a simple query with a where clause.


def test_ogr_sql_1(data_ds):
    lyr = data_ds.GetLayerByName("poly")

    lyr.SetAttributeFilter("eas_id < 167")

    count = lyr.GetFeatureCount()
    assert count == 3, (
        "Got wrong count with GetFeatureCount() - %d, expecting 3" % count
    )

    lyr.SetAttributeFilter("")
    count = lyr.GetFeatureCount()
    assert count == 10, (
        "Got wrong count with GetFeatureCount() - %d, expecting 10" % count
    )

    with data_ds.ExecuteSQL("SELECT * FROM poly WHERE eas_id < 167") as sql_lyr:
        assert sql_lyr.GetFeatureCount(force=0) < 0
        assert sql_lyr.GetFeatureCount(force=1) == 3


###############################################################################
# Test DISTINCT handling


def test_ogr_sql_2(data_ds):

    expect = [168, 169, 166, 158, 165]

    with data_ds.ExecuteSQL(
        "select distinct eas_id from poly where eas_id < 170"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ORDER BY handling


def test_ogr_sql_3(data_ds):

    expect = [158, 165, 166, 168, 169]

    with data_ds.ExecuteSQL(
        "select distinct eas_id from poly where eas_id < 170 order by eas_id"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ORDER BY DESC handling


def test_ogr_sql_3_desc(data_ds):

    expect = [169, 168, 166, 165, 158]

    with data_ds.ExecuteSQL(
        "select distinct eas_id from poly where eas_id < 170 order by eas_id desc"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test DISTINCT and ORDER BY on strings.


def test_ogr_sql_4(data_ds):

    expect = ["_158_", "_165_", "_166_", "_168_", "_170_", "_171_", "_179_"]

    with data_ds.ExecuteSQL(
        "select distinct name from idlink order by name asc"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "name", expect)


###############################################################################
# Test column functions.


def test_ogr_sql_5(data_ds):

    with data_ds.ExecuteSQL(
        "select max(eas_id), min(eas_id), avg(eas_id), STDDEV_POP(eas_id), STDDEV_SAMP(eas_id), sum(eas_id), count(eas_id) from idlink"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat["max_eas_id"] == 179
        assert feat["min_eas_id"] == 158
        assert feat["avg_eas_id"] == pytest.approx(168.142857142857, abs=1e-12)
        assert feat["STDDEV_POP_eas_id"] == pytest.approx(5.9384599116647205, rel=1e-15)
        assert feat["STDDEV_SAMP_eas_id"] == pytest.approx(6.414269805898183, rel=1e-15)
        assert feat["count_eas_id"] == 7
        assert feat["sum_eas_id"] == 1177


###############################################################################
# Test simple COUNT() function.


def test_ogr_sql_6(data_ds):

    expect = [10]

    with data_ds.ExecuteSQL("select count(*) from poly") as sql_lyr:
        ogrtest.check_features_against_list(sql_lyr, "count_*", expect)


###############################################################################
# Verify that selecting the FID works properly.


def test_ogr_sql_7(data_ds):

    expect = [7, 8]

    with data_ds.ExecuteSQL(
        "select eas_id, fid from poly where eas_id in (158,165)"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "fid", expect)


###############################################################################
# Verify that wildcard expansion works properly.


def test_ogr_sql_8(data_ds):

    expect = ["35043369", "35043408"]

    with data_ds.ExecuteSQL("select * from poly where eas_id in (158,165)") as sql_lyr:
        ogrtest.check_features_against_list(sql_lyr, "PRFEDEA", expect)


###############################################################################
# Verify that quoted table names work.


def test_ogr_sql_9(data_ds):

    expect = ["35043369", "35043408"]

    with data_ds.ExecuteSQL(
        'select * from "poly" where eas_id in (158,165)'
    ) as sql_lyr:
        ogrtest.check_features_against_list(sql_lyr, "PRFEDEA", expect)


###############################################################################
# Test the ILIKE operator.


@pytest.mark.require_driver("CSV")
def test_ogr_sql_ilike():

    ds = ogr.Open("data/prime_meridian.csv")
    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME ilike 'GREEN%'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME ilike '%WICH'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME ilike 'FOO%'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0


###############################################################################
# Test the LIKE operator.


@pytest.mark.require_driver("CSV")
def test_ogr_sql_like():

    ds = ogr.Open("data/prime_meridian.csv")
    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME like 'Green%'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME like '%wich'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL(
        "select * from prime_meridian where PRIME_MERIDIAN_NAME like 'GREEN%'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    with gdaltest.config_option("OGR_SQL_LIKE_AS_ILIKE", "YES"):
        with ds.ExecuteSQL(
            "select * from prime_meridian where PRIME_MERIDIAN_NAME like 'GREEN%'"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 1


###############################################################################
# Test MAX() on empty dataset.


def test_ogr_sql_11():

    expect = [None]

    ds = ogr.Open("data/shp/empty.shp")
    with ds.ExecuteSQL("select max(eas_id) from empty") as sql_lyr:
        ogrtest.check_features_against_list(sql_lyr, "max_eas_id", expect)


###############################################################################
# Test DISTINCT on empty dataset.


def test_ogr_sql_12():

    expect = []

    ds = ogr.Open("data/shp/empty.shp")
    with ds.ExecuteSQL("select distinct eas_id from empty") as sql_lyr:
        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Verify selection of, and on ogr_geometry.


def test_ogr_sql_13(data_ds):

    expect = ["POLYGON"] * 10

    with data_ds.ExecuteSQL(
        "select ogr_geometry from poly where ogr_geometry = 'POLYGON'"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "ogr_geometry", expect)


###############################################################################
# Verify selection of, and on ogr_style and ogr_geom_wkt.


@pytest.mark.require_driver("MapInfo File")
def test_ogr_sql_14():

    expect = [
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0",cap:r,j:r)',
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0",cap:r,j:r)',
    ]

    ds = ogr.Open("data/mitab/small.mif")
    with ds.ExecuteSQL(
        "select ogr_style from small where ogr_geom_wkt LIKE 'POLYGON%'"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "ogr_style", expect)


###############################################################################
# Verify that selecting with filtering by FID works properly.


def test_ogr_sql_15(data_ds):

    expect = [7]

    with data_ds.ExecuteSQL(
        "select fid,eas_id,prfedea from poly where fid = %d" % expect[0]
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "fid", expect)


###############################################################################


@pytest.mark.require_driver("MapInfo File")
def test_ogr_sql_16():

    expect = [2]

    ds = ogr.Open("data/mitab/small.mif")
    with ds.ExecuteSQL("select fid from small where owner < 'H'") as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "fid", expect)


###############################################################################
# Test the RFC 21 CAST operator.
#
@pytest.mark.require_driver("MapInfo File")
def test_ogr_sql_17():

    expect = ["1", "2"]

    ds = ogr.Open("data/mitab/small.mif")
    with ds.ExecuteSQL(
        "select CAST(fid as CHARACTER(10)), CAST(data as numeric(7,3)) from small"
    ) as sql_lyr:

        fld_def = sql_lyr.GetLayerDefn().GetFieldDefn(0)

        assert fld_def.GetName() == "fid", "got wrong fid field name"

        assert fld_def.GetType() == ogr.OFTString, "got wrong fid field type"
        assert fld_def.GetWidth() == 10, "got wrong fid field width"

        fld_def = sql_lyr.GetLayerDefn().GetFieldDefn(1)

        assert fld_def.GetName() == "data", "got wrong data field name"

        assert fld_def.GetType() == ogr.OFTReal, "got wrong data field type"
        assert fld_def.GetWidth() == 7, "got wrong data field width"
        assert fld_def.GetPrecision() == 3, "got wrong data field precision"

        ogrtest.check_features_against_list(sql_lyr, "fid", expect)


###############################################################################
# Test query "SELECT * from my_layer" on layer without any field (#2788)


def test_ogr_sql_20():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer")

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature(feat)

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL("SELECT * from my_layer") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 2


###############################################################################
# Test query "SELECT *, fid from my_layer" on layer without any field (#2788)


def test_ogr_sql_21():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_ds.CreateLayer("my_layer")

    with mem_ds.ExecuteSQL("SELECT *, fid from my_layer") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 1
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "fid"


###############################################################################
# Test multiple expansion of '*' as in "SELECT *, fid, *, my_layer.* from my_layer" (#2788)


def test_ogr_sql_22():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer")
    mem_lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL("SELECT *, fid, *, my_layer.* from my_layer") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 4
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "test"
        assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "fid"
        assert sql_lyr.GetLayerDefn().GetFieldDefn(2).GetName() == "test"
        assert sql_lyr.GetLayerDefn().GetFieldDefn(3).GetName() == "my_layer.test"


###############################################################################
# Test query "SELECT DISTINCT test from my_layer" (#2788)


def test_ogr_sql_23():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer")
    mem_lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetField("test", 0)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature(feat)

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetField("test", 1)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL("SELECT DISTINCT test from my_layer") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 2


###############################################################################
# Test that style strings get carried with OGR SQL SELECT results. (#2808)


@pytest.mark.require_driver("DGN")
def test_ogr_sql_24():

    ds = ogr.Open("data/dgn/smalltest.dgn")

    with ds.ExecuteSQL(
        "SELECT * from elements where colorindex=83 and type=3"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert len(feat.GetStyleString()) >= 10


###############################################################################
# Test for OGR_GEOM_AREA special field (#2949)


def test_ogr_sql_25():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer")
    mem_lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetField("test", 0)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    mem_lyr.CreateFeature(feat)

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat.SetField("test", 1)
    feat.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON((0 0,0 0.5,0.5 0.5,0.5 0,0 0))")
    )
    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL(
        "SELECT test, OGR_GEOM_AREA from my_layer WHERE OGR_GEOM_AREA > 0.9"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFieldAsDouble("OGR_GEOM_AREA") == 1.0
        assert feat.GetFieldAsString("test") == "0"


###############################################################################
# Test query 'SELECT 'literal_value' AS column_name FROM a_table'
#


def test_ogr_sql_26():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer")

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL(
        "SELECT 'literal_value' AS my_column, 'literal_value2' my_column2 FROM my_layer"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFieldAsString("my_column") == "literal_value"
        assert feat.GetFieldAsString("my_column2") == "literal_value2"


###############################################################################

###############################################################################
# Test query on datetime columns
#


@pytest.mark.require_driver("CSV")
def test_ogr_sql_27():

    ds = ogr.Open("data/csv/testdatetime.csv")

    with ds.ExecuteSQL(
        "SELECT * FROM testdatetime WHERE "
        "timestamp < '2010/04/01 00:00:00' AND "
        "timestamp > '2009/11/15 11:59:59' AND "
        "timestamp != '2009/12/31 23:00:00' "
        "ORDER BY timestamp DESC"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "name", ["foo5", "foo4"])


###############################################################################
# Test robustness against invalid SQL statements.
# With RFC 28 new implementation, most of them are directly caught by the generated
# code from the grammar


@pytest.fixture(scope="module")
def ds_for_invalid_statements():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    lyr = ds.CreateLayer("my_layer")

    new_geom_field_defn = ogr.GeomFieldDefn("geom", ogr.wkbUnknown)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_NAME_FLAG
        )
        == ogr.OGRERR_NONE
    )

    field_defn = ogr.FieldDefn("strfield", ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("intfield", ogr.OFTInteger)
    lyr.CreateField(field_defn)

    lyr = ds.CreateLayer("my_layer2")
    field_defn = ogr.FieldDefn("strfield", ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("strfield2", ogr.OFTString)
    lyr.CreateField(field_defn)

    yield ds


@pytest.mark.parametrize(
    "sql",
    [
        None,
        "",
        "1",
        "*",
        "SELECT",
        "SELECT ' FROM my_layer",
        "SELECT + FROM my_layer",
        "SELECT (1 FROM my_layer",
        "SELECT (1)) FROM my_layer",
        "SELECT (1,) FROM my_layer",
        "SELECT 1 + FROM my_layer",
        "SELECT 1 + 'a' FROM my_layer",
        "SELECT 1 - FROM my_layer",
        "SELECT 1 * FROM my_layer",
        "SELECT 1 % FROM my_layer",
        "SELECT x.",
        "SELECT x AS",
        "SELECT *",
        "SELECT * FROM",
        "SELECT * FROM foo",
        "SELECT FROM my_layer",
        "SELECT FROM FROM my_layer",
        "SELECT ('strfield'",
        "SELECT 'strfield' +",
        "SELECT 'strfield' 'strfield'",
        "SELECT CONCAT('strfield')",
        "SELECT foo(strfield) FROM my_layer",  # Undefined function 'foo' used.
        "SELECT strfield, FROM my_layer",
        "SELECT strfield, foo FROM my_layer",
        "SELECT strfield AS FROM my_layer",
        "SELECT strfield AS 1 FROM my_layer",
        "SELECT strfield AS strfield2 FROM",
        "SELECT strfield + intfield FROM my_layer",
        "SELECT CAST",
        "SELECT CAST(",
        "SELECT CAST(strfield",
        "SELECT CAST(strfield AS",
        "SELECT CAST(strfield AS foo",
        "SELECT CAST(strfield AS foo)",
        "SELECT CAST(strfield AS foo) FROM",
        "SELECT CAST(strfield AS foo) FROM my_layer",
        "SELECT CAST(strfield AS CHARACTER",
        "SELECT CAST(strfield AS CHARACTER)",
        "SELECT CAST(strfield AS CHARACTER) FROM",
        "SELECT CAST(strfield AS CHARACTER) FROM foo",
        "SELECT CAST(strfield AS CHARACTER(",
        "SELECT CAST(strfield AS CHARACTER(2",
        "SELECT CAST(strfield AS CHARACTER(2)",
        "SELECT CAST(strfield AS CHARACTER(2))",
        "SELECT CAST(strfield AS CHARACTER(2)) FROM",
        "SELECT CAST(strfield AS CHARACTER(2)) FROM foo",
        "SELECT CAST(strfield AS 1) FROM my_layer",
        "SELECT * FROM my_layer WHERE",
        # 'SELECT * FROM my_layer WHERE strfield',
        "SELECT * FROM my_layer WHERE strfield = ",
        "SELECT * FROM my_layer WHERE strfield = foo",
        "SELECT * FROM my_layer WHERE foo = 'a'",
        "SELECT * FROM my_layer WHERE strfield = 'a"
        "SELECT * FROM my_layer WHERE strfield = 'a' ORDER ",
        "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY",
        "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY foo",
        "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY strfield UNK",
        "SELECT * FROM my_layer ORDER BY geom",  # Cannot use geometry field 'geom' in a ORDER BY clause
        "SELECT FOO(*) FROM my_layer",
        "SELECT FOO(*) AS bar FROM my_layer",
        "SELECT COUNT",
        "SELECT COUNT(",
        "SELECT COUNT() FROM my_layer",
        "SELECT COUNT(*",
        "SELECT COUNT(*)",
        "SELECT COUNT(*) FROM",
        "SELECT COUNT(*) AS foo FROM",
        "SELECT COUNT(* FROM my_layer",
        "SELECT COUNT(i_dont_exist) FROM my_layer",
        "SELECT COUNT(FOO intfield) FROM my_layer",
        "SELECT COUNT(DISTINCT intfield FROM my_layer",
        "SELECT COUNT(DISTINCT i_dont_exist) FROM my_layer",
        "SELECT COUNT(DISTINCT *) FROM my_layer",
        "SELECT FOO(DISTINCT intfield) FROM my_layer",
        "SELECT FOO(DISTINCT intfield) as foo FROM my_layer",
        "SELECT DISTINCT foo FROM my_layer",
        "SELECT DISTINCT foo AS 'id' 'id2' FROM",
        "SELECT DISTINCT foo AS id id2 FROM",
        "SELECT DISTINCT FROM my_layer",
        "SELECT DISTINCT strfield, COUNT(DISTINCT intfield) FROM my_layer",
        "SELECT MIN(intfield*2) FROM my_layer",
        "SELECT MIN(intfield,2) FROM my_layer",
        "SELECT MIN(foo) FROM my_layer",
        "SELECT MAX(foo) FROM my_layer",
        "SELECT SUM(foo) FROM my_layer",
        "SELECT AVG(foo) FROM my_layer",
        "SELECT STDDEV_POP(foo) FROM my_layer",
        "SELECT STDDEV_SAMP(foo) FROM my_layer",
        "SELECT SUM(strfield) FROM my_layer",
        "SELECT AVG(strfield) FROM my_layer",
        "SELECT AVG(intfield, intfield) FROM my_layer",
        "SELECT STDDEV_POP(strfield) FROM my_layer",
        "SELECT STDDEV_SAMP(strfield) FROM my_layer",
        "SELECT * FROM my_layer WHERE AVG(intfield) = 1",
        "SELECT * FROM my_layer WHERE STDDEV_POP(intfield) = 1",
        "SELECT * FROM 'foo' foo",
        "SELECT * FROM my_layer WHERE strfield =",
        "SELECT * FROM my_layer WHERE strfield = foo",
        "SELECT * FROM my_layer WHERE strfield = intfield",
        "SELECT * FROM my_layer WHERE strfield = 1",
        "SELECT * FROM my_layer WHERE strfield = '1' AND",
        # "SELECT * FROM my_layer WHERE 1 AND 2" ,
        "SELECT * FROM my_layer WHERE strfield LIKE",
        "SELECT * FROM my_layer WHERE strfield LIKE 1",
        "SELECT * FROM my_layer WHERE strfield IS",
        "SELECT * FROM my_layer WHERE strfield IS NOT",
        "SELECT * FROM my_layer WHERE strfield IS foo",
        "SELECT * FROM my_layer WHERE strfield IS NOT foo",
        "SELECT * FROM my_layer WHERE (strfield IS NOT NULL",
        "SELECT * FROM my_layer WHERE strfield IN",
        "SELECT * FROM my_layer WHERE strfield IN(",
        "SELECT * FROM my_layer WHERE strfield IN()",
        "SELECT * FROM my_layer WHERE strfield IN('a'",
        "SELECT * FROM my_layer WHERE strfield IN('a',",
        "SELECT * FROM my_layer WHERE strfield IN('a','b'",
        "SELECT * FROM my_layer WHERE strfield IN('a','b'))",
        "SELECT * FROM my_layer LEFT",
        "SELECT * FROM my_layer LEFT JOIN",
        "SELECT * FROM my_layer LEFT JOIN foo",
        "SELECT * FROM my_layer LEFT JOIN foo ON my_layer.strfield = my_layer2.strfield",
        "SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = foo.strfield",
        "SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.foo",
        # "SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield != my_layer2.strfield",
        "SELECT *, my_layer2. FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.strfield",
        "SELECT *, my_layer2.foo FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.strfield",
        "SELECT * FROM my_layer UNION",
        "SELECT * FROM my_layer UNION ALL",
        "SELECT * FROM my_layer UNION ALL SELECT",
        "SELECT * FROM my_layer UNION ALL SELECT *",
        "SELECT * FROM my_layer UNION ALL SELECT * FROM",
    ],
)
def test_ogr_sql_invalid_statements(ds_for_invalid_statements, sql):

    with pytest.raises(Exception):
        ds_for_invalid_statements.ExecuteSQL(None)


###############################################################################
# Verify that IS NULL and IS NOT NULL are working


def test_ogr_sql_29():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    lyr = ds.CreateLayer("my_layer")
    field_defn = ogr.FieldDefn("strfield", ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "a")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "b")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    with ds.ExecuteSQL("select * from my_layer where strfield is null") as sql_lyr:
        count_is_null = sql_lyr.GetFeatureCount()
        assert count_is_null == 1, "IS NULL failed"

    with ds.ExecuteSQL("select * from my_layer where strfield is not null") as sql_lyr:
        count_is_not_null = sql_lyr.GetFeatureCount()
        assert count_is_not_null == 2, "IS NOT NULL failed"


###############################################################################
# Verify a select mixing a count(*) with something else works without errors


def test_ogr_sql_30(data_ds):

    gdal.ErrorReset()

    with data_ds.ExecuteSQL("select min(eas_id), count(*) from poly") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val_count = feat.GetField(1)

        assert gdal.GetLastErrorMsg() == ""

        assert val_count == 10


###############################################################################
# Regression test for #4022


def test_ogr_sql_31(data_ds):

    gdal.ErrorReset()

    with data_ds.ExecuteSQL("select min(eas_id) from poly where area = 0") as sql_lyr:

        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)

        assert gdal.GetLastErrorMsg() == ""

        assert val is None


###############################################################################
# Regression test for #4022 (same as above, but with dialect = 'OGRSQL')


def test_ogr_sql_32(data_ds):

    gdal.ErrorReset()

    with data_ds.ExecuteSQL(
        "select min(eas_id) from poly where area = 0", dialect="OGRSQL"
    ) as sql_lyr:

        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)

        assert gdal.GetLastErrorMsg() == ""

        assert val is None


###############################################################################
# Check ALTER TABLE commands


def test_ogr_sql_33():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    lyr = ds.CreateLayer("my_layer")

    # We support with and without COLUMN keyword
    for extrakeyword in ("COLUMN ", ""):
        sql = "ALTER TABLE my_layer ADD %smyfield NUMERIC(20, 8)" % extrakeyword
        assert ds.ExecuteSQL(sql) is None
        assert (
            lyr.GetLayerDefn().GetFieldIndex("myfield") != -1
            and lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("myfield"))
            .GetType()
            == ogr.OFTReal
            and lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("myfield"))
            .GetWidth()
            == 20
            and lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("myfield"))
            .GetPrecision()
            == 8
        ), ("%s failed" % sql)

        sql = 'ALTER TABLE my_layer RENAME %smyfield TO "myfield 2"' % extrakeyword
        assert ds.ExecuteSQL(sql) is None
        assert (
            lyr.GetLayerDefn().GetFieldIndex("myfield") == -1
            and lyr.GetLayerDefn().GetFieldIndex("myfield 2") != -1
        ), ("%s failed" % sql)

        sql = 'ALTER TABLE my_layer ALTER %s"myfield 2" TYPE CHARACTER' % extrakeyword
        assert ds.ExecuteSQL(sql) is None
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("myfield 2"))
            .GetType()
            == ogr.OFTString
        ), ("%s failed" % sql)

        sql = (
            'ALTER TABLE my_layer ALTER %s"myfield 2" TYPE CHARACTER(15)' % extrakeyword
        )
        assert ds.ExecuteSQL(sql) is None
        assert (
            lyr.GetLayerDefn()
            .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("myfield 2"))
            .GetWidth()
            == 15
        ), ("%s failed" % sql)

        sql = 'ALTER TABLE my_layer DROP %s"myfield 2"' % extrakeyword
        assert ds.ExecuteSQL(sql) is None
        assert lyr.GetLayerDefn().GetFieldIndex("myfield 2") == -1, "%s failed" % sql

    ds = None


###############################################################################
# Test implicit conversion from string to numeric (#4259)


def test_ogr_sql_34(data_ds):

    with data_ds.ExecuteSQL(
        "select count(*) from poly where eas_id in ('165')"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)

        assert val == 1

    with pytest.raises(Exception):
        data_ds.ExecuteSQL("select count(*) from poly where eas_id in ('a165')")


###############################################################################
# Test huge SQL queries (#4262)


def test_ogr_sql_35(data_ds):

    cols = "area"
    for _ in range(10):
        cols = cols + "," + cols
    with data_ds.ExecuteSQL("select %s from poly" % cols) as sql_lyr:
        count_cols = sql_lyr.GetLayerDefn().GetFieldCount()

    assert count_cols == 1024


###############################################################################
# Test select distinct on null values (#4353)


def test_ogr_sql_36():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("ogr_sql_36")
    lyr = ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("floatfield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, 2.3)
    feat.SetField(2, "456")
    feat.SetField(3, 1234567890123)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    for fieldname in ["intfield", "int64field", "floatfield", "strfield"]:
        with ds.ExecuteSQL(
            "select distinct %s from layer order by %s asc" % (fieldname, fieldname)
        ) as sql_lyr:
            feat = sql_lyr.GetNextFeature()
            assert feat.IsFieldSetAndNotNull(0) == 0, fieldname
            feat = sql_lyr.GetNextFeature()
            assert feat.IsFieldSetAndNotNull(0) != 0, fieldname

    for fieldname in ["intfield", "int64field", "floatfield", "strfield"]:
        with ds.ExecuteSQL(
            "select distinct %s from layer order by %s desc" % (fieldname, fieldname)
        ) as sql_lyr:
            feat = sql_lyr.GetNextFeature()
            assert feat.IsFieldSetAndNotNull(0) != 0, fieldname
            feat = sql_lyr.GetNextFeature()
            assert feat.IsFieldSetAndNotNull(0) == 0, fieldname


###############################################################################
# Test select count([distinct] column) with null values (#4354)


def test_ogr_sql_count_and_null():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("ogr_sql_37")
    lyr = ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("floatfield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("strfield_first_null", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("strfield_never_set", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("intfield_never_set", ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(2, "456")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(2, "456")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(1, 2.3)
    feat.SetField("strfield_first_null", "foo")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(1, 2.3)
    lyr.CreateFeature(feat)
    feat = None

    for fieldname in ["intfield", "floatfield", "strfield"]:
        with ds.ExecuteSQL(
            "select count(%s), count(distinct %s), count(*) from layer"
            % (fieldname, fieldname)
        ) as sql_lyr:
            feat = sql_lyr.GetNextFeature()
            assert feat.GetFieldAsInteger(0) == 2, fieldname
            assert feat.GetFieldAsInteger(1) == 1, fieldname
            assert feat.GetFieldAsInteger(2) == 4, fieldname

    with ds.ExecuteSQL(
        "select avg(intfield), STDDEV_POP(intfield) from layer where intfield is null"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.IsFieldSetAndNotNull(0) == 0
        assert feat.IsFieldSetAndNotNull(1) == 0

    # Fix crash when first values is null (#4509)
    with ds.ExecuteSQL("select distinct strfield_first_null from layer") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert not feat.IsFieldSetAndNotNull("strfield_first_null")
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFieldAsString("strfield_first_null") == "foo"

    with ds.ExecuteSQL("select distinct strfield_never_set from layer") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert not feat.IsFieldSetAndNotNull("strfield_never_set")

    with ds.ExecuteSQL(
        "select min(intfield_never_set), max(intfield_never_set), avg(intfield_never_set), sum(intfield_never_set), count(intfield_never_set) from layer"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert not feat.IsFieldSetAndNotNull(0)
        assert not feat.IsFieldSetAndNotNull(1)
        assert not feat.IsFieldSetAndNotNull(2)
        assert not feat.IsFieldSetAndNotNull(3)
        assert feat.GetField(4) == 0


###############################################################################
# Test "SELECT MAX(OGR_GEOM_AREA) FROM XXXX" (#4633)


def test_ogr_sql_38(data_ds):

    with data_ds.ExecuteSQL("SELECT MAX(OGR_GEOM_AREA) FROM poly") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetFieldAsDouble(0)
        assert val == pytest.approx(1634833.39062, abs=1e-5)


###############################################################################
# Test ORDER BY on a float special field


def test_ogr_sql_39(data_ds):

    with data_ds.ExecuteSQL("SELECT * FROM poly ORDER BY OGR_GEOM_AREA") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetFieldAsDouble(0)
        assert val == pytest.approx(5268.813, abs=1e-5)


###############################################################################
# Test ORDER BY on a int special field


def test_ogr_sql_40(data_ds):

    with data_ds.ExecuteSQL("SELECT * FROM poly ORDER BY FID DESC") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFID() == 9


###############################################################################
# Test ORDER BY on a string special field


def test_ogr_sql_41(data_ds):

    with data_ds.ExecuteSQL("SELECT * FROM poly ORDER BY OGR_GEOMETRY") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFID() == 0


###############################################################################
# Test comparing to empty string


def test_ogr_sql_42(data_ds):

    lyr = data_ds.GetLayerByName("poly")
    lyr.SetAttributeFilter("prfedea <> ''")
    feat = lyr.GetNextFeature()
    lyr.SetAttributeFilter(None)
    assert feat is not None

    with data_ds.ExecuteSQL("SELECT * FROM poly WHERE prfedea <> ''") as sql_lyr:
        assert sql_lyr.GetNextFeature() is not None


###############################################################################
# Test escape sequences


def test_ogr_sql_43(data_ds):

    sql = "SELECT '\"' as a, '\\' as b, '''' as c FROM poly"
    with data_ds.ExecuteSQL(sql) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat["a"] == '"'
        assert feat["b"] == "\\"
        assert feat["c"] == "'"


###############################################################################
# Test hstore_get_value()


@pytest.mark.parametrize(
    "sql",
    [
        "SELECT hstore_get_value('a') FROM poly",
        "SELECT hstore_get_value(1, 1) FROM poly",
    ],
)
def test_ogr_sql_hstore_get_value_invalid_parameters(data_ds, sql):

    # Invalid parameters
    with pytest.raises(Exception):
        data_ds.ExecuteSQL(sql)


@pytest.mark.parametrize(
    "sql",
    [
        "SELECT hstore_get_value('a', null) FROM poly",
        "SELECT hstore_get_value(null, 'a') FROM poly",
        "SELECT hstore_get_value('a', 'a') FROM poly",
        "SELECT hstore_get_value('a=>b', 'c') FROM poly",
        "SELECT hstore_get_value('a=>', 'a') FROM poly",
        "SELECT hstore_get_value(' a => ', 'a') FROM poly",
        "SELECT hstore_get_value('a=>b,z,c=>d', 'c') FROM poly",
        "SELECT hstore_get_value('\"a', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\"', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\"=', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\" =>', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\" => ', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\" => \"', 'a') FROM poly",
        "SELECT hstore_get_value('\"a\" => \"\" z', 'a') FROM poly",
    ],
)
def test_ogr_sql_hstore_get_value_invalid_hstore_syntax_or_empty_result(data_ds, sql):

    # Invalid hstore syntax or empty result
    with data_ds.ExecuteSQL(sql) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert not f.IsFieldSetAndNotNull(0), sql


@pytest.mark.parametrize(
    "sql,expected",
    [
        ("SELECT hstore_get_value('a=>b', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value(' a => b ', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value('\"a\"=>b', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value(' \"a\" =>b', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value('a=>\"b\"', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value('a=> \"b\" ', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value('\"a\"=>\"b\"', 'a') FROM poly", "b"),
        ("SELECT hstore_get_value(' \"a\" => \"b\" ', 'a') FROM poly", "b"),
        ('SELECT hstore_get_value(\' "a\\"b" => "b" \', \'a"b\') FROM poly', "b"),
    ],
)
def test_ogr_sql_hstore_get_value_valid(data_ds, sql, expected):

    # Valid hstore syntax
    with data_ds.ExecuteSQL(sql) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == expected, sql


###############################################################################
# Test 64 bit GetFeatureCount()


@pytest.mark.require_driver("OGR_VRT")
def test_ogr_sql_45():

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="poly">
    <SrcDataSource relativeToVRT="0" shared="1">data/poly.shp</SrcDataSource>
    <SrcLayer>poly</SrcLayer>
    <GeometryType>wkbPolygon</GeometryType>
    <Field name="AREA" type="Real" src="AREA"/>
    <Field name="EAS_ID" type="Integer" src="EAS_ID"/>
    <Field name="PRFEDEA" type="Integer" src="PRFEDEA"/>
    <FeatureCount>1000000000000</FeatureCount>
  </OGRVRTLayer>
</OGRVRTDataSource>""")
    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 1000000000000

    with ds.ExecuteSQL("SELECT COUNT(*) FROM poly") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 1000000000000

    with ds.ExecuteSQL("SELECT COUNT(AREA) FROM poly") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 10


###############################################################################
# Test strict SQL quoting


@pytest.fixture(scope="module")
def ogr_sql_strit_quoting_ds():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("test")
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("from", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, "not_from")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 3)
    feat.SetField(1, "from")
    lyr.CreateFeature(feat)

    return ds


def test_ogr_sql_strict_quoting_non_aggregate(ogr_sql_strit_quoting_ds):

    with ogr_sql_strit_quoting_ds.ExecuteSQL(
        'select id, \'id\', "id" as id2, id as "id3", "from" from test where "from" = \'from\''
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.GetField(0) == 3
        assert feat.GetField(1) == "id"
        assert feat.GetField(2) == 3
        assert feat.GetField(3) == 3
        assert feat.GetField(4) == "from"

        feat = sql_lyr.GetNextFeature()
        assert feat is None


def test_ogr_sql_strict_quoting_aggregate(ogr_sql_strit_quoting_ds):

    with ogr_sql_strit_quoting_ds.ExecuteSQL(
        'select max("id"), max(id), count("id"), count(id) from "test"'
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.GetField(0) == 3
        assert feat.GetField(1) == 3
        assert feat.GetField(2) == 2
        assert feat.GetField(3) == 2


@pytest.mark.parametrize(
    "sql",
    [
        "select * from 'test'",
        "select distinct 'id' from 'test'",
        "select max('id') from 'test'",
        "select id as 'id2' from 'test'",
    ],
)
def test_ogr_sql_strict_quoting_errors(ogr_sql_strit_quoting_ds, sql):

    with pytest.raises(Exception):
        ogr_sql_strit_quoting_ds.ExecuteSQL(sql)


###############################################################################
# Test NULL sorting (#6155)


def test_ogr_sql_47():

    ds = ogr.Open("data/shp/sort_test.dbf")
    with ds.ExecuteSQL("SELECT * FROM sort_test ORDER BY text_value") as sql_lyr:
        prec_val = ""
        for f in sql_lyr:
            if f.IsFieldSetAndNotNull("text_value"):
                new_val = f["text_value"]
            else:
                new_val = ""
            assert new_val >= prec_val, "new_val = '%s', prec_val = '%s'" % (
                new_val,
                prec_val,
            )
            prec_val = new_val


###############################################################################
# Test sorting of more than 100 elements


def test_ogr_sql_48():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    for i in range(1000):
        f = ogr.Feature(lyr.GetLayerDefn())
        if (i % 2) == 0:
            f.SetField(0, i + 1)
        else:
            f.SetField(0, 1001 - i)
        lyr.CreateFeature(f)
    with ds.ExecuteSQL("SELECT * FROM test ORDER BY int_field") as sql_lyr:
        i = 1
        for f in sql_lyr:
            if f["int_field"] != i:
                f.DumpReadable()
                pytest.fail()
            i = i + 1

        assert i == 1001

        for i in range(1000):
            assert sql_lyr.GetFeature(i)["int_field"] == lyr.GetFeature(i)["int_field"]


###############################################################################
# Test arithmetic expressions


def test_ogr_sql_49(data_ds):

    # expressions and expected result
    expressions = [
        ("1/1", 1),
        ("1/1.", 1.0),
        ("cast((1) as integer)/1.", 1.0),
        ("1./cast((1) as integer)", 1.0),
        ("1.5+1", 2.5),
        ("(1*1)+1.5", 2.5),
        ("1+1", 2),
        ("cast(1 as integer)+ 1234567890123", 1234567890124),
        ("cast(1 as integer)* 1234567890123", 1234567890123),
    ]

    for expression, expected in expressions:
        with data_ds.ExecuteSQL(
            "select {} as result from poly limit 1".format(expression)
        ) as sql_lyr:
            ogrtest.check_features_against_list(sql_lyr, "result", [expected])


###############################################################################
# Test field names with same case


def test_ogr_sql_field_names_same_case():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("id"))
    lyr.CreateField(ogr.FieldDefn("ID"))
    lyr.CreateField(ogr.FieldDefn("ID2"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = "foo"
    f["ID"] = "bar"
    f["ID2"] = "baz"
    lyr.CreateFeature(f)

    with ds.ExecuteSQL("SELECT * FROM test") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["id"] == "foo"
        assert f["ID"] == "bar"
        assert f["ID2"] == "baz"


###############################################################################
# Test no crash when comparing string with integer array


def test_ogr_sql_string_int_array_comparison():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("id"))
    lyr.CreateField(ogr.FieldDefn("int_array", ogr.OFTIntegerList))
    f = ogr.Feature(lyr.GetLayerDefn())

    f["id"] = "foo"
    f.SetFieldIntegerList(1, [1, 2])

    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = lyr.GetNextFeature()
    assert f is not None

    assert lyr.SetAttributeFilter("id = 'foo'") == ogr.OGRERR_NONE
    f = lyr.GetNextFeature()
    assert f is not None

    for op in ("=", "<>", "<", "<=", ">", ">="):
        assert lyr.SetAttributeFilter("int_array {} 1".format(op)) == ogr.OGRERR_NONE
        f = lyr.GetNextFeature()
        assert f is None

    assert lyr.SetAttributeFilter("int_array BETWEEN 0 AND 3") == ogr.OGRERR_NONE
    f = lyr.GetNextFeature()
    assert f is None

    assert lyr.SetAttributeFilter("int_array IS NULL") == ogr.OGRERR_NONE
    f = lyr.GetNextFeature()
    assert f is None

    assert lyr.SetAttributeFilter("int_array IN (1, 2)") == ogr.OGRERR_NONE
    f = lyr.GetNextFeature()
    assert f is None

    del lyr
    del ds


###############################################################################
# Test SetAttributeFilter() on a GenSQL layer that doesn't forward its
# initial where clause to the source, particularly with explicit dialect="OGRSQL"


@pytest.mark.parametrize("dialect", [None, "OGRSQL"])
def test_ogr_sql_attribute_filter_on_top_of_non_forward_where_clause(dialect):

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    mem_lyr = mem_ds.CreateLayer("test")
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOLYGON EMPTY"))
    mem_lyr.CreateFeature(f)

    with mem_ds.ExecuteSQL(
        "SELECT * FROM test WHERE OGR_GEOMETRY = 'POLYGON'", dialect=dialect
    ) as sql_lyr:
        sql_lyr.SetAttributeFilter("")
        assert sql_lyr.GetFeatureCount() == 1

    with mem_ds.ExecuteSQL(
        "SELECT * FROM test WHERE OGR_GEOMETRY = 'POLYGON'", dialect=dialect
    ) as sql_lyr:
        sql_lyr.SetAttributeFilter("1")
        assert sql_lyr.GetFeatureCount() == 1

    with mem_ds.ExecuteSQL(
        "SELECT * FROM test WHERE OGR_GEOMETRY = 'POLYGON'", dialect=dialect
    ) as sql_lyr:
        sql_lyr.SetAttributeFilter("0")
        assert sql_lyr.GetFeatureCount() == 0


###############################################################################
# Test min/max on string fields


def test_ogr_sql_min_max_string_field():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    mem_lyr = mem_ds.CreateLayer("test")
    mem_lyr.CreateField(ogr.FieldDefn("str_field"))

    with mem_ds.ExecuteSQL(
        "select min(str_field), max(str_field) from test"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat["MIN_str_field"] is None
        assert feat["MAX_str_field"] is None

    for v in ("z", "b", "ab"):
        f = ogr.Feature(mem_lyr.GetLayerDefn())
        f["str_field"] = v
        mem_lyr.CreateFeature(f)

    with mem_ds.ExecuteSQL(
        "select min(str_field), max(str_field) from test"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat["MIN_str_field"] == "ab"
        assert feat["MAX_str_field"] == "z"


##############################################################################
# Test SELECT * EXCEPT


# Test some error cases. Some of these could potentially be tolerated
# in the future.
@pytest.mark.parametrize(
    "body",
    [
        "(",
        ")",
        "()",
        "(*)",
        "(EASID, ",
        "(EASID, DOES_NOT_EXIST)",
        "(EAS_ID, EAS_ID)",
        "(EAS_ID,, AREA)",
    ],
)
def test_ogr_sql_select_except_errors(data_ds, body):
    with pytest.raises(Exception):
        data_ds.ExecuteSQL(f"SELECT * EXCEPT {body} FROM poly")


def test_ogr_sql_select_except_attrs(data_ds):

    with data_ds.ExecuteSQL("SELECT * EXCEPT (EAS_ID, PRFEDEA) FROM poly") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 1
        assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 1


def test_ogr_sql_select_except_geom(data_ds):

    with data_ds.ExecuteSQL(
        'SELECT * EXCEPT (EAS_ID, "_ogr_geometry_") FROM poly'
    ) as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 2
        assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0


def test_ogr_sql_select_except_multiple_asterisk_1(data_ds):
    with data_ds.ExecuteSQL("SELECT * EXCEPT (EAS_ID), * FROM poly") as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetFieldCount() == 5

        assert defn.GetFieldDefn(0).GetName() == "AREA"
        assert defn.GetFieldDefn(1).GetName() == "PRFEDEA"
        assert defn.GetFieldDefn(2).GetName() == "AREA"
        assert defn.GetFieldDefn(3).GetName() == "EAS_ID"
        assert defn.GetFieldDefn(4).GetName() == "PRFEDEA"

        assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 2


def test_ogr_sql_select_except_multiple_asterisk_2(data_ds):
    with data_ds.ExecuteSQL("SELECT *, * EXCEPT (EAS_ID) FROM poly") as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetFieldCount() == 5

        assert defn.GetFieldDefn(0).GetName() == "AREA"
        assert defn.GetFieldDefn(1).GetName() == "EAS_ID"
        assert defn.GetFieldDefn(2).GetName() == "PRFEDEA"
        assert defn.GetFieldDefn(3).GetName() == "AREA"
        assert defn.GetFieldDefn(4).GetName() == "PRFEDEA"

        assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 2


def test_ogr_sql_select_except_named_geometry():

    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")
    mem_lyr = mem_ds.CreateLayer("my_layer", geom_type=ogr.wkbNone)
    mem_lyr.CreateGeomField(ogr.GeomFieldDefn("named_geom", ogr.wkbUnknown))
    mem_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    mem_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))

    feat = ogr.Feature(mem_lyr.GetLayerDefn())
    feat["id"] = 3
    feat["name"] = "test"
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))

    mem_lyr.CreateFeature(feat)

    with mem_ds.ExecuteSQL("SELECT * EXCEPT (named_geom, id) FROM my_layer") as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetGeomFieldCount() == 0
        assert defn.GetFieldCount() == 1
        assert defn.GetFieldDefn(0).GetName() == "name"


@pytest.fixture()
def select_except_join_ds():
    mem_ds = ogr.GetDriverByName("MEM").CreateDataSource("my_ds")

    pt_lyr = mem_ds.CreateLayer("point")
    pt_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    pt_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))

    pt_feat = ogr.Feature(pt_lyr.GetLayerDefn())
    pt_feat["id"] = 1
    pt_feat["name"] = "test"
    pt_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    pt_lyr.CreateFeature(pt_feat)

    line_lyr = mem_ds.CreateLayer("line")
    line_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    line_feat = ogr.Feature(line_lyr.GetLayerDefn())
    line_feat["id"] = 1
    line_feat.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 1, 2 2)"))
    line_lyr.CreateFeature(line_feat)

    return mem_ds


def test_ogr_sql_select_except_join_1(select_except_join_ds):

    with select_except_join_ds.ExecuteSQL(
        "SELECT * FROM point JOIN line ON point.id = line.id"
    ) as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetGeomFieldCount() == 1
        f = sql_lyr.GetNextFeature()
        assert f.GetGeometryRef().GetGeometryType() == ogr.wkbPoint

        assert defn.GetFieldCount() == 3
        assert defn.GetFieldDefn(0).GetName() == "id"
        assert defn.GetFieldDefn(1).GetName() == "name"
        assert defn.GetFieldDefn(2).GetName() == "line.id"


def test_ogr_sql_select_except_join_2(select_except_join_ds):

    # excluding "id" without a table name excludes only point.id

    with select_except_join_ds.ExecuteSQL(
        "SELECT * EXCEPT (id) FROM point JOIN line ON point.id = line.id"
    ) as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetGeomFieldCount() == 1
        assert defn.GetFieldCount() == 2
        assert defn.GetFieldDefn(0).GetName() == "name"
        assert defn.GetFieldDefn(1).GetName() == "line.id"


def test_ogr_sql_select_except_join_3(select_except_join_ds):

    with select_except_join_ds.ExecuteSQL(
        "SELECT * EXCLUDE (line.id) FROM point JOIN line ON point.id = line.id"
    ) as sql_lyr:

        defn = sql_lyr.GetLayerDefn()

        assert defn.GetGeomFieldCount() == 1
        assert defn.GetFieldCount() == 2
        assert defn.GetFieldDefn(0).GetName() == "id"
        assert defn.GetFieldDefn(1).GetName() == "name"


def test_ogr_sql_like_utf8():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test", options=["ADVERTIZE_UTF8=YES"])
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    lyr.SetAttributeFilter("'é' LIKE 'É'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'É' LIKE 'é'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'É' LIKE 'É'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'é' LIKE 'e'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' LIKE 'ê'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' LIKE ''")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' LIKE '_'")
    assert lyr.GetFeatureCount() == 1

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'\xc3' LIKE '_'")
    lyr.GetFeatureCount()  # we return 1 currently, we could as well return 0...

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'\xc3' LIKE 'é'")
    assert lyr.GetFeatureCount() == 0

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'é' LIKE '\xc3'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' LIKE '_ven'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'éven' LIKE '%ven'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'' LIKE '_'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' LIKE '_xen'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' LIKE '%xen'")
    assert lyr.GetFeatureCount() == 0


def test_ogr_sql_ilike_utf8():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test", options=["ADVERTIZE_UTF8=YES"])
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    lyr.SetAttributeFilter("'é' ILIKE 'é'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'é' ILIKE 'É'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'É' ILIKE 'é'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'É' ILIKE 'É'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'é' ILIKE 'e'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' ILIKE 'ê'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' ILIKE ''")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'é' ILIKE '_'")
    assert lyr.GetFeatureCount() == 1

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'\xc3' ILIKE '_'")
    lyr.GetFeatureCount()  # we return 1 currently, we could as well return 0...

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'\xc3' ILIKE 'é'")
    assert lyr.GetFeatureCount() == 0

    # Truncated UTF8 character
    lyr.SetAttributeFilter("'é' ILIKE '\xc3'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' ILIKE '_ven'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'éven' ILIKE '%ven'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("'' ILIKE '_'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' ILIKE '_xen'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'éven' ILIKE '%xen'")
    assert lyr.GetFeatureCount() == 0


###############################################################################
# Test error on setting a spatial filter during ExecuteSQL


def test_ogr_sql_test_execute_sql_error_on_spatial_filter_mem_layer():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("test", geom_type=ogr.wkbNone)
    geom = ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))")
    with pytest.raises(
        Exception, match="Cannot set spatial filter: no geometry field present in layer"
    ):
        ds.ExecuteSQL("SELECT 1 FROM test", spatialFilter=geom)


###############################################################################
# Test NOT/IN on a comparison where a NULL value was involved


@pytest.fixture(scope="module")
def ds_for_test_ogr_sql_on_null():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("test_ogr_sql_on_null")
    lyr = ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("realfield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("datetimefield", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat["intfield"] = 1
    feat["realfield"] = 1
    feat["datetimefield"] = "2024-01-01T00:00:00"
    feat["strfield"] = "foo"
    lyr.CreateFeature(feat)

    yield ds


def get_available_dialects():
    return [None, "SQLite"] if ogr.GetDriverByName("SQLite") else [None]


@pytest.mark.parametrize(
    "where,feature_count",
    [  # intfield
        ("1 + intfield >= 0", 1),
        ("intfield = 0", 0),
        ("intfield = 1", 1),
        ("NOT intfield = 0", 1),
        ("NOT intfield = 1", 0),
        ("intfield IS NULL", 1),
        ("intfield IS NOT NULL", 1),
        ("intfield IN (NULL)", 0),
        ("NULL IN (NULL)", 0),
        ("NULL NOT IN (NULL)", 0),
        ("intfield NOT IN (NULL)", 0),
        ("intfield IN (1, NULL)", 1),
        ("intfield IN (0, NULL)", 0),
        ("intfield IN (NULL, 1)", 1),
        ("intfield IN (NULL, 0)", 0),
        ("intfield NOT IN (1, NULL)", 0),
        ("intfield NOT IN (0, NULL)", 0),
        ("intfield NOT IN (NULL, 1)", 0),
        ("intfield NOT IN (NULL, 0)", 0),
        ("(NOT intfield = 0) OR intfield IS NULL", 2),
        ("NOT (intfield = 0 OR intfield = 0)", 1),
        ("(NOT intfield = 0) AND NOT (intfield = 0)", 1),
        ("NOT (intfield = 0 OR intfield IS NULL)", 1),
        ("(NOT intfield = 0) AND NOT (intfield IS NULL)", 1),
        ("NOT (intfield = 0 OR intfield IS NOT NULL)", 0),
        ("(NOT intfield = 0) AND NOT (intfield IS NOT NULL)", 0),
        ("intfield <> 0 AND intfield <> 2", 1),
        ("intfield IS NOT NULL AND intfield NOT IN (2)", 1),
        ("NOT(intfield NOT IN (1) AND NULL NOT IN (1))", 1),
        ("NOT(intfield IS NOT NULL AND intfield NOT IN (2))", 1),
        ("NOT(NOT(intfield IS NOT NULL AND intfield NOT IN (2)))", 1),
        ("NOT (intfield = 0 AND intfield = 0)", 1),
        ("(intfield NOT IN (1) AND NULL NOT IN (1)) IS NULL", 1),
        # realfield
        ("1 + realfield >= 0", 1),
        ("realfield = 0", 0),
        ("realfield = 1", 1),
        ("NOT realfield = 0", 1),
        ("NOT realfield = 1", 0),
        ("realfield IS NULL", 1),
        ("realfield IS NOT NULL", 1),
        ("realfield IN (NULL)", 0),
        ("realfield NOT IN (NULL)", 0),
        ("realfield IN (1, NULL)", 1),
        ("realfield IN (0, NULL)", 0),
        ("realfield NOT IN (1, NULL)", 0),
        ("realfield NOT IN (0, NULL)", 0),
        ("(NOT realfield = 0) OR realfield IS NULL", 2),
        ("NOT (realfield = 0 OR realfield = 0)", 1),
        ("NOT (realfield = 0 OR realfield IS NULL)", 1),
        ("NOT (realfield = 0 OR realfield IS NOT NULL)", 0),
        # strfield
        ("strfield = ''", 0),
        ("strfield = 'foo'", 1),
        ("NOT strfield = ''", 1),
        ("NOT strfield = 'foo'", 0),
        ("strfield IS NULL", 1),
        ("strfield IS NOT NULL", 1),
        ("strfield IN ('foo', NULL)", 1),
        ("strfield NOT IN ('foo', NULL)", 0),
        ("strfield IN ('', NULL)", 0),
        ("strfield NOT IN ('', NULL)", 0),
        # datetimefield
        ("datetimefield = '1970-01-01T00:00:00'", 0),
        ("datetimefield = '2024-01-01T00:00:00'", 1),
        ("NOT datetimefield = '1970-01-01T00:00:00'", 1),
        ("NOT datetimefield = '2024-01-01T00:00:00'", 0),
        ("datetimefield IS NULL", 1),
        ("datetimefield IS NOT NULL", 1),
        ("datetimefield IN ('2024-01-01T00:00:00', NULL)", 1),
        ("datetimefield NOT IN ('2024-01-01T00:00:00', NULL)", 0),
        ("datetimefield IN ('1970-01-01T00:00:00', NULL)", 0),
        ("datetimefield NOT IN ('1970-01-01T00:00:00', NULL)", 0),
        ("datetimefield IN ('invalid', NULL)", None),
    ],
)
@pytest.mark.parametrize("dialect", get_available_dialects())
def test_ogr_sql_on_null(where, feature_count, dialect, ds_for_test_ogr_sql_on_null):

    if feature_count is None:
        if dialect == "SQLite":
            with pytest.raises(Exception):
                with ds_for_test_ogr_sql_on_null.ExecuteSQL(
                    "select * from layer where " + where, dialect=dialect
                ) as sql_lyr:
                    pass
        else:
            with ds_for_test_ogr_sql_on_null.ExecuteSQL(
                "select * from layer where " + where, dialect=dialect
            ) as sql_lyr:
                with pytest.raises(Exception):
                    sql_lyr.GetFeatureCount()
    else:
        with ds_for_test_ogr_sql_on_null.ExecuteSQL(
            "select * from layer where " + where, dialect=dialect
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == feature_count


def test_ogr_sql_ogr_style_hidden():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("test_ogr_sql_ogr_style_hidden")
    lyr = ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat["intfield"] = 1
    feat["strfield"] = "my_style"
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    with ds.ExecuteSQL(
        "SELECT 'BRUSH(fc:#01234567)' AS OGR_STYLE HIDDEN FROM layer"
    ) as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 0
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() == "BRUSH(fc:#01234567)"

    with ds.ExecuteSQL("SELECT strfield OGR_STYLE HIDDEN FROM layer") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 0
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() == "my_style"
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None

    with ds.ExecuteSQL(
        "SELECT CAST(strfield AS CHARACTER(255)) AS OGR_STYLE HIDDEN FROM layer"
    ) as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 0
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() == "my_style"
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None

    with ds.ExecuteSQL("SELECT strfield OGR_STYLE HIDDEN, * FROM layer") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldCount() == 2
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() == "my_style"
        assert f["intfield"] == 1
        assert f["strfield"] == "my_style"
        f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None
        assert not f.IsFieldSet("intfield")
        assert not f.IsFieldSet("strfield")

    with pytest.raises(
        Exception, match="HIDDEN keyword only supported on a column named OGR_STYLE"
    ):
        with ds.ExecuteSQL(
            "SELECT 'foo' AS not_OGR_STYLE HIDDEN FROM layer"
        ) as sql_lyr:
            pass

    with ds.ExecuteSQL("SELECT 123 AS OGR_STYLE HIDDEN FROM layer") as sql_lyr:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None

    with ds.ExecuteSQL("SELECT intfield AS OGR_STYLE HIDDEN FROM layer") as sql_lyr:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None

    with ds.ExecuteSQL(
        'SELECT "_ogr_geometry_" AS OGR_STYLE HIDDEN FROM layer'
    ) as sql_lyr:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = sql_lyr.GetNextFeature()
        assert f.GetStyleString() is None


def test_ogr_sql_identifier_hidden():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("test_ogr_sql_ogr_style_hidden")
    lyr = ds.CreateLayer("hidden")
    lyr.CreateField(ogr.FieldDefn("hidden", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat["hidden"] = "val"
    lyr.CreateFeature(feat)

    with ds.ExecuteSQL("SELECT hidden FROM hidden") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["hidden"] == "val"

    with ds.ExecuteSQL("SELECT hidden hidden FROM hidden hidden") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["hidden"] == "val"

    with ds.ExecuteSQL("SELECT hidden AS hidden FROM hidden AS hidden") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["hidden"] == "val"

    with ds.ExecuteSQL("SELECT 'foo' AS hidden FROM hidden") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["hidden"] == "foo"


@pytest.mark.parametrize(
    "input,expected_output",
    [
        [(1, 1e100, 1, -1e100), 2],
        [(float("inf"), 1), float("inf")],
        [(1, float("-inf")), float("-inf")],
        [(1, float("nan")), float("nan")],
        [(float("inf"), float("-inf")), float("nan")],
    ],
)
def test_ogr_sql_kahan_babuska_eumaier_summation(input, expected_output):
    """Test accurate SUM() implementation using Kahan-Babuska-Neumaier algorithm"""

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("v", ogr.OFTReal))
    for v in input:
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat["v"] = v
        lyr.CreateFeature(feat)

    with ds.ExecuteSQL("SELECT SUM(v) FROM test") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        if math.isnan(expected_output):
            assert math.isnan(f["SUM_v"])
        else:
            assert f["SUM_v"] == expected_output


@pytest.mark.parametrize(
    "operator", ["+", "-", "*", "/", "%", "<", "<=", "=<", "=", "<>", ">", ">=", "=>"]
)
def test_ogr_sql_max_expr_depth(operator):

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("test")
    with ds.ExecuteSQL("SELECT " + operator.join(["1"] * 127) + " FROM test") as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT " + operator.join(["1"] * 128) + " FROM test")


def test_ogr_sql_max_expr_depth_other():
    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("test")

    with ds.ExecuteSQL(
        "SELECT CAST(" + "+".join(["1"] * 126) + " AS CHARACTER) FROM test"
    ) as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL(
            "SELECT CAST(" + "+".join(["1"] * 127) + " AS CHARACTER) FROM test"
        )

    with ds.ExecuteSQL(
        "SELECT 'a' IN (CAST(" + "+".join(["1"] * 125) + " AS CHARACTER)) FROM test"
    ) as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL(
            "SELECT 'a' IN (CAST(" + "+".join(["1"] * 126) + " AS CHARACTER)) FROM test"
        )

    with ds.ExecuteSQL("SELECT NOT " + "+".join(["1"] * 126) + " FROM test") as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT NOT " + "+".join(["1"] * 127) + " FROM test")

    with ds.ExecuteSQL("SELECT 1 AND " + "+".join(["1"] * 126) + " FROM test") as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT 1 AND " + "+".join(["1"] * 127) + " FROM test")

    with ds.ExecuteSQL("SELECT 1 OR " + "+".join(["1"] * 126) + " FROM test") as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT 1 OR " + "+".join(["1"] * 127) + " FROM test")

    with ds.ExecuteSQL("SELECT " + "+".join(["1"] * 126) + " IS NULL FROM test") as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT " + "+".join(["1"] * 127) + " IS NULL FROM test")

    with ds.ExecuteSQL(
        "SELECT " + "+".join(["1"] * 125) + " IS NOT NULL FROM test"
    ) as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT " + "+".join(["1"] * 126) + " IS NOT NULL FROM test")

    with ds.ExecuteSQL(
        "SELECT SUBSTR('a', " + "+".join(["1"] * 126) + ") FROM test"
    ) as _:
        pass
    with pytest.raises(Exception, match="Maximum expression depth reached"):
        ds.ExecuteSQL("SELECT SUBSTR('a', " + "+".join(["1"] * 127) + ") FROM test")


@pytest.mark.require_driver("GPKG")
def test_ogr_sql_union_layer_feature_count_add_overflow():

    with gdal.OpenEx("data/gpkg/huge_feature_count.gpkg") as ds:
        with ds.ExecuteSQL(
            "select * from test union all select * from test2", dialect="OGRSQL"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 0
