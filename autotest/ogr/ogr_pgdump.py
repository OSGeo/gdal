#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PGDump driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("PGDump")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Create table from data/poly.shp


def test_ogr_pgdump_1(tmp_path):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(tmp_path / "tpoly.sql")

    ######################################################
    # Create Layer
    lyr = ds.CreateLayer("tpoly", options=["DIM=3", "POSTGIS_VERSION=1.5"])
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
        ],
    )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    shp_ds = ogr.Open("data/poly.shp")
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    ds.Close()

    with open(tmp_path / "tpoly.sql") as f:
        sql = f.read()

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""DROP TABLE IF EXISTS "public"."tpoly" CASCADE;""")
    check_and_remove(
        """DELETE FROM geometry_columns WHERE f_table_name = 'tpoly' AND f_table_schema = 'public';"""
    )
    check_and_remove("""BEGIN;""")
    check_and_remove("""CREATE TABLE "public"."tpoly"();""")
    check_and_remove(
        """ALTER TABLE "public"."tpoly" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "tpoly_pk" PRIMARY KEY;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('public','tpoly','wkb_geometry',-1,'GEOMETRY',3);"""
    )
    check_and_remove("""ALTER TABLE "public"."tpoly" ADD COLUMN "area" FLOAT8;""")
    check_and_remove("""ALTER TABLE "public"."tpoly" ADD COLUMN "eas_id" INTEGER;""")
    check_and_remove("""ALTER TABLE "public"."tpoly" ADD COLUMN "prfedea" VARCHAR;""")
    check_and_remove(
        """ALTER TABLE "public"."tpoly" ADD COLUMN "shortname" VARCHAR(8);"""
    )
    check_and_remove(
        """INSERT INTO "public"."tpoly" ("wkb_geometry", "area", "eas_id", "prfedea") VALUES ('01030000800100000014000000000000602F491D41000000207F2D52410000000000000000000000C028471D41000000E0922D52410000000000000000000000007C461D4100000060AE2D5241000000000000000000000080C9471D4100000020B62D52410000000000000000000000209C4C1D41000000E0D82D52410000000000000000000000608D4C1D41000000A0DD2D52410000000000000000000000207F4E1D41000000A0EA2D5241000000000000000000000020294F1D4100000080CA2D5241000000000000000000000000B4511D41000000E0552D52410000000000000000000000C016521D4100000080452D52410000000000000000000000E0174E1D41000000202E2D5241000000000000000000000020414D1D41000000E04C2D52410000000000000000000000E04B4D1D41000000605E2D5241000000000000000000000040634D1D41000000E0742D52410000000000000000000000A0EF4C1D41000000E08D2D52410000000000000000000000E04E4C1D41000000E0A12D52410000000000000000000000E0B04B1D4100000060B82D5241000000000000000000000080974A1D4100000080AE2D5241000000000000000000000080CF491D4100000080952D52410000000000000000000000602F491D41000000207F2D52410000000000000000', 215229.266, 168, '35043411');"""
    )
    check_and_remove(
        """CREATE INDEX "tpoly_wkb_geometry_geom_idx" ON "public"."tpoly" USING GIST ("wkb_geometry");"""
    )
    check_and_remove("""COMMIT;""")


###############################################################################
# Create table from data/poly.shp with PG_USE_COPY=YES


def test_ogr_pgdump_2(tmp_path):

    with gdal.config_option("PG_USE_COPY", "YES"):

        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_path / "tpoly.sql", options=["LINEFORMAT=CRLF"]
        )

        ######################################################
        # Create Layer
        lyr = ds.CreateLayer(
            'xx"yyy',
            geom_type=ogr.wkbPolygon,
            options=['SCHEMA=ano"ther_schema', "SRID=4326", 'GEOMETRY_NAME=the_"geom'],
        )

        ######################################################
        # Setup Schema
        ogrtest.quick_create_layer_def(
            lyr,
            [
                ("AREA", ogr.OFTReal),
                ("EAS_ID", ogr.OFTInteger),
                ("PRFEDEA", ogr.OFTString),
                ("SHORTNAME", ogr.OFTString, 8),
            ],
        )

        ######################################################
        # Copy in poly.shp

        dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

        shp_ds = ogr.Open("data/poly.shp")
        shp_lyr = shp_ds.GetLayer(0)
        feat = shp_lyr.GetNextFeature()

        while feat is not None:

            dst_feat.SetFrom(feat)
            lyr.CreateFeature(dst_feat)

            feat = shp_lyr.GetNextFeature()

        ds.Close()

    with open(tmp_path / "tpoly.sql") as f:
        sql = f.read()

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""CREATE SCHEMA "ano""ther_schema";""")
    check_and_remove("""DROP TABLE IF EXISTS "ano""ther_schema"."xx""yyy" CASCADE;""")
    check_and_remove("""BEGIN;""")
    check_and_remove("""CREATE TABLE "ano""ther_schema"."xx""yyy"();""")
    check_and_remove(
        """ALTER TABLE "ano""ther_schema"."xx""yyy" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "xx""yyy_pk" PRIMARY KEY;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('ano"ther_schema','xx"yyy','the_"geom',4326,'POLYGON',2);"""
    )
    check_and_remove(
        """ALTER TABLE "ano""ther_schema"."xx""yyy" ADD COLUMN "area" FLOAT8;"""
    )
    check_and_remove(
        """ALTER TABLE "ano""ther_schema"."xx""yyy" ADD COLUMN "eas_id" INTEGER;"""
    )
    check_and_remove(
        """ALTER TABLE "ano""ther_schema"."xx""yyy" ADD COLUMN "prfedea" VARCHAR;"""
    )
    check_and_remove(
        """ALTER TABLE "ano""ther_schema"."xx""yyy" ADD COLUMN "shortname" VARCHAR(8);"""
    )
    check_and_remove(
        """COPY "ano""ther_schema"."xx""yyy" ("the_""geom", "area", "eas_id", "prfedea", "shortname") FROM STDIN;"""
    )
    check_and_remove(
        """0103000020E61000000100000014000000000000602F491D41000000207F2D5241000000C028471D41000000E0922D5241000000007C461D4100000060AE2D524100000080C9471D4100000020B62D5241000000209C4C1D41000000E0D82D5241000000608D4C1D41000000A0DD2D5241000000207F4E1D41000000A0EA2D524100000020294F1D4100000080CA2D524100000000B4511D41000000E0552D5241000000C016521D4100000080452D5241000000E0174E1D41000000202E2D524100000020414D1D41000000E04C2D5241000000E04B4D1D41000000605E2D524100000040634D1D41000000E0742D5241000000A0EF4C1D41000000E08D2D5241000000E04E4C1D41000000E0A12D5241000000E0B04B1D4100000060B82D524100000080974A1D4100000080AE2D524100000080CF491D4100000080952D5241000000602F491D41000000207F2D5241\t215229.266\t168\t35043411\t\\N"""
    )
    check_and_remove("""\\.""")
    # Check that there's no semi-column after above command
    assert sql.startswith("\n") or sql.startswith("\r\n")
    check_and_remove(
        """CREATE INDEX "xx""yyy_the_""geom_geom_idx" ON "ano""ther_schema"."xx""yyy" USING GIST ("the_""geom");"""
    )
    check_and_remove("""COMMIT;""")


###############################################################################
# Create table from data/poly.shp without any geometry


def test_ogr_pgdump_3(tmp_path):

    with gdal.config_option("PG_USE_COPY", "YES"):

        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_path / "tpoly.sql", options=["LINEFORMAT=LF"]
        )

        ######################################################
        # Create Layer
        lyr = ds.CreateLayer(
            "tpoly", geom_type=ogr.wkbNone, options=["SCHEMA=another_schema"]
        )

        ######################################################
        # Setup Schema
        ogrtest.quick_create_layer_def(
            lyr,
            [
                ("EMPTYCHAR", ogr.OFTString),
                ("AREA", ogr.OFTReal),
                ("EAS_ID", ogr.OFTInteger),
                ("PRFEDEA", ogr.OFTString),
                ("SHORTNAME", ogr.OFTString, 8),
            ],
        )

        ######################################################
        # Copy in poly.shp

        dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

        shp_ds = ogr.Open("data/poly.shp")
        shp_lyr = shp_ds.GetLayer(0)
        feat = shp_lyr.GetNextFeature()

        i = 0

        while feat is not None:

            dst_feat.SetFrom(feat)
            if i == 0:
                # Be perverse and test the case where a feature has a geometry
                # even if it's a wkbNone layer ! (#4040)
                dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
            elif i == 1:
                # Field with 0 character (not empty!) (#4040)
                dst_feat.SetField(0, "")
            i = i + 1
            lyr.CreateFeature(dst_feat)

            feat = shp_lyr.GetNextFeature()

        ds.Close()

    with open(tmp_path / "tpoly.sql") as f:
        sql = f.read()

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    assert "SELECT AddGeometryColumn" not in sql
    assert "CREATE INDEX" not in sql

    check_and_remove("""CREATE SCHEMA "another_schema";""")
    check_and_remove("""DROP TABLE IF EXISTS "another_schema"."tpoly" CASCADE;""")
    check_and_remove("""BEGIN;""")
    check_and_remove("""CREATE TABLE "another_schema"."tpoly"();""")
    check_and_remove(
        """ALTER TABLE "another_schema"."tpoly" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "tpoly_pk" PRIMARY KEY;"""
    )
    check_and_remove(
        """ALTER TABLE "another_schema"."tpoly" ADD COLUMN "area" FLOAT8;"""
    )
    check_and_remove(
        """ALTER TABLE "another_schema"."tpoly" ADD COLUMN "eas_id" INTEGER;"""
    )
    check_and_remove(
        """ALTER TABLE "another_schema"."tpoly" ADD COLUMN "prfedea" VARCHAR;"""
    )
    check_and_remove(
        """ALTER TABLE "another_schema"."tpoly" ADD COLUMN "shortname" VARCHAR(8);"""
    )
    check_and_remove(
        """COPY "another_schema"."tpoly" ("emptychar", "area", "eas_id", "prfedea", "shortname") FROM STDIN;"""
    )
    check_and_remove("""\\N\t215229.266\t168\t35043411\t\\N""")
    check_and_remove("""\t5268.813\t170\t35043413\t\\N""")
    check_and_remove("""\\.""")
    # Check that there's no semi-column after above command
    assert sql.startswith("\n") or sql.startswith("\r\n")
    check_and_remove("""COMMIT;""")


###############################################################################
# Test multi-geometry support


def test_ogr_pgdump_4(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_4.sql", options=["LINEFORMAT=LF"]
    )
    assert ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) != 0

    ######################################################
    # Create Layer
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=["WRITE_EWKT_GEOM=YES"])
    assert lyr.TestCapability(ogr.OLCCreateGeomField) != 0

    gfld_defn = ogr.GeomFieldDefn("point_nosrs", ogr.wkbPoint)
    lyr.CreateGeomField(gfld_defn)

    gfld_defn = ogr.GeomFieldDefn("poly", ogr.wkbPolygon25D)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    lyr.CreateGeomField(gfld_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomFieldDirectly("point_nosrs", ogr.CreateGeometryFromWkt("POINT (1 2)"))
    feat.SetGeomFieldDirectly(
        "poly",
        ogr.CreateGeometryFromWkt("POLYGON Z ((0 0 0,0 1 0,1 1 0,1 0 0, 0 0 0))"),
    )
    lyr.CreateFeature(feat)

    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_4.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""CREATE TABLE "public"."test"();""")
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','point_nosrs',0,'POINT',2);"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','poly',4326,'POLYGON',3);"""
    )
    check_and_remove("""INSERT INTO "public"."test" DEFAULT VALUES;""")
    check_and_remove(
        """INSERT INTO "public"."test" ("point_nosrs", "poly") VALUES (GeomFromEWKT('SRID=0;POINT (1 2)'::TEXT) , GeomFromEWKT('SRID=4326;POLYGON Z ((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0))'::TEXT) );"""
    )
    check_and_remove(
        """CREATE INDEX "test_point_nosrs_geom_idx" ON "public"."test" USING GIST ("point_nosrs");"""
    )
    check_and_remove(
        """CREATE INDEX "test_poly_geom_idx" ON "public"."test" USING GIST ("poly");"""
    )


###############################################################################
# Test non nullable, unique and comment field support


def test_ogr_pgdump_non_nullable_unique_comment(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_5.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    field_defn.SetComment("this field is not nullable")
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    field_defn.SetUnique(True)
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

    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_5.sql", "rb")
    sql = gdal.VSIFReadL(1, 1000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_not_nullable" VARCHAR NOT NULL;"""
    )
    check_and_remove(
        """COMMENT ON COLUMN "public"."test"."field_not_nullable" IS 'this field is not nullable';"""
    )
    assert "COMMENT ON" not in sql
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_nullable" VARCHAR UNIQUE;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','geomfield_not_nullable',0,'POINT',2);"""
    )
    check_and_remove(
        """ALTER TABLE "test" ALTER COLUMN "geomfield_not_nullable" SET NOT NULL;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','geomfield_nullable',0,'POINT',2);"""
    )


###############################################################################
# Test default values


def test_ogr_pgdump_6(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_6.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)

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

    field_defn = ogr.FieldDefn("field_date", ogr.OFTDate)
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_time", ogr.OFTTime)
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    with gdal.config_option("PG_USE_COPY", "YES"):

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("field_string", "a")
        f.SetField("field_int", 456)
        f.SetField("field_real", 4.56)
        f.SetField("field_datetime", "2015/06/30 12:34:56")
        f.SetField("field_datetime2", "2015/06/30 12:34:56")
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
        f.SetField("field_date", "2015/06/30")
        f.SetField("field_time", "12:34:56")
        lyr.CreateFeature(f)
        f = None

    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_6.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_string" VARCHAR DEFAULT 'a''b';"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_int" INTEGER DEFAULT 123;"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_real" FLOAT8 DEFAULT 1.23;"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_datetime" timestamp with time zone DEFAULT CURRENT_TIMESTAMP;"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_datetime2" timestamp with time zone DEFAULT '2015/06/30 12:34:56+00'::timestamp with time zone;"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_date" date DEFAULT CURRENT_DATE;"""
    )
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "field_time" time DEFAULT CURRENT_TIME;"""
    )
    check_and_remove(
        """a\t456\t4.56\t\\N\t2015/06/30 12:34:56\t2015/06/30 12:34:56\t2015/06/30\t12:34:56"""
    )
    check_and_remove(
        """b\t456\t4.56\t\\N\t2015/06/30 12:34:56\t2015/06/30 12:34:56\t2015/06/30\t12:34:56"""
    )


###############################################################################
# Test creating a field with the fid name (PG_USE_COPY=NO)


def test_ogr_pgdump_7(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_7.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=["FID=myfid"])

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

    # feat.SetField('str', 'foo')
    # ret = lyr.SetFeature(feat)
    # if ret != 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    feat.SetField("myfid", 10)
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret != 0

    # gdal.PushErrorHandler()
    # ret = lyr.SetFeature(feat)
    # gdal.PopErrorHandler()
    # if ret == 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    # feat.UnsetField('myfid')
    # gdal.PushErrorHandler()
    # ret = lyr.SetFeature(feat)
    # gdal.PopErrorHandler()
    # if ret == 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str", "first string")
    feat.SetField("myfid", 12)
    feat.SetField("str2", "second string")
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 12

    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_7.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    assert """ALTER TABLE "public"."test" ADD COLUMN "myfid" VARCHAR""" not in sql
    assert """ALTER TABLE "public"."test" ADD COLUMN "myfid" INT""" not in sql

    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "myfid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str" VARCHAR;""")
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str2" VARCHAR;""")
    check_and_remove(
        """INSERT INTO "public"."test" ("myfid", "str", "str2") VALUES (10, 'first string', 'second string');"""
    )
    check_and_remove(
        """SELECT setval(pg_get_serial_sequence('"public"."test"', 'myfid'), MAX("myfid")) FROM "public"."test";"""
    )
    check_and_remove(
        """INSERT INTO "public"."test" ("myfid", "str", "str2") VALUES (12, 'first string', 'second string');"""
    )


###############################################################################
# Test creating a field with the fid name (PG_USE_COPY=YES)


def test_ogr_pgdump_8(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_8.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=["FID=myfid"])

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
    with gdal.config_option("PG_USE_COPY", "YES"):
        ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 10

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str2", "second string")
    with gdal.config_option("PG_USE_COPY", "YES"):
        ret = lyr.CreateFeature(feat)
    assert ret == 0
    if feat.GetFID() < 0:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetField("myfid") != feat.GetFID():
        feat.DumpReadable()
        pytest.fail()

    # feat.SetField('str', 'foo')
    # ret = lyr.SetFeature(feat)
    # if ret != 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    feat.SetField("myfid", 10)
    with gdaltest.error_handler(), gdal.config_option("PG_USE_COPY", "YES"):
        ret = lyr.CreateFeature(feat)
    assert ret != 0

    # gdal.PushErrorHandler()
    # ret = lyr.SetFeature(feat)
    # gdal.PopErrorHandler()
    # if ret == 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    # feat.UnsetField('myfid')
    # gdal.PushErrorHandler()
    # ret = lyr.SetFeature(feat)
    # gdal.PopErrorHandler()
    # if ret == 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str", "first string")
    feat.SetField("myfid", 12)
    feat.SetField("str2", "second string")
    with gdal.config_option("PG_USE_COPY", "YES"):
        ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 12

    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_8.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    assert """ALTER TABLE "public"."test" ADD COLUMN "myfid" VARCHAR""" not in sql
    assert """ALTER TABLE "public"."test" ADD COLUMN "myfid" INT""" not in sql

    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "myfid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str" VARCHAR;""")
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str2" VARCHAR;""")
    check_and_remove("""COPY "public"."test" ("myfid", "str", "str2") FROM STDIN;""")
    check_and_remove("""10\tfirst string\tsecond string""")
    check_and_remove("""\\.""")
    check_and_remove(
        """SELECT setval(pg_get_serial_sequence('"public"."test"', 'myfid'), MAX("myfid")) FROM "public"."test";"""
    )
    check_and_remove(
        """INSERT INTO "public"."test" ("str2") VALUES ('second string');"""
    )
    check_and_remove("""COPY "public"."test" ("myfid", "str", "str2") FROM STDIN;""")
    check_and_remove("""12\tfirst string\tsecond string""")
    check_and_remove("""\\.""")


###############################################################################
# Test creating a field with the fid name (PG_USE_COPY=NO)


@pytest.mark.parametrize("pg_use_copy", ["YES", "NO"])
def test_ogr_pgdump_9(tmp_vsimem, pg_use_copy):

    with gdaltest.config_option("PG_USE_COPY", pg_use_copy):
        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_vsimem / "ogr_pgdump_9.sql", options=["LINEFORMAT=LF"]
        )
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)

        fld = ogr.FieldDefn("str", ogr.OFTString)
        fld.SetWidth(5)
        lyr.CreateField(fld)
        fld = ogr.FieldDefn("str2", ogr.OFTString)
        lyr.CreateField(fld)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "01234")
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "ABCDEF")
        lyr.CreateFeature(feat)

        val4 = "\u00e9\u00e9\u00e9\u00e9"
        val5 = val4 + "\u00e9"
        val6 = val5 + "\u00e9"

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", val6)
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField("str", "a" + val5)
        lyr.CreateFeature(feat)

        ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_9.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    if pg_use_copy == "YES":
        eofield = "\t"
    else:
        eofield = "'"
    assert (
        """01234%s""" % eofield in sql
        and """ABCDE%s""" % eofield in sql
        and """%s%s""" % (val5, eofield) in sql
        and """%s%s""" % ("a" + val4, eofield) in sql
    )


###############################################################################
# Export POINT EMPTY for PostGIS 2.2


def test_ogr_pgdump_11(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_11.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_11.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # clang -m32 generates F8FF..., instead of F87F... for all other systems
    assert (
        "0101000000000000000000F87F000000000000F87F" in sql
        or "0101000000000000000000F8FF000000000000F8FF" in sql
    )


###############################################################################
# Test that GEOMETRY_NAME works even when the geometry column creation is
# done through CreateGeomField (#6366)
# This is important for the ogr2ogr use case when the source geometry column
# is not-nullable, and hence the CreateGeomField() interface is used.


def test_ogr_pgdump_12(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_12.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbNone, options=["GEOMETRY_NAME=another_name"]
    )
    lyr.CreateGeomField(ogr.GeomFieldDefn("my_geom", ogr.wkbPoint))
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_12.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    assert "another_name" in sql


###############################################################################
# Test ZM support

tests_zm = [
    [
        ogr.wkbUnknown,
        [],
        "POINT ZM (1 2 3 4)",
        ["'GEOMETRY',2)", "0101000000000000000000F03F0000000000000040"],
    ],
    [
        ogr.wkbUnknown,
        ["GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        ["geography(GEOMETRY)", "0101000000000000000000F03F0000000000000040"],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYZ"],
        "POINT ZM (1 2 3 4)",
        ["'GEOMETRY',3)", "0101000080000000000000F03F00000000000000400000000000000840"],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYZ", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(GEOMETRYZ)",
            "0101000080000000000000F03F00000000000000400000000000000840",
        ],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYZ"],
        "POINT ZM (1 2 3 4)",
        ["'POINT',3)", "0101000080000000000000F03F00000000000000400000000000000840"],
    ],
    [
        ogr.wkbPoint25D,
        [],
        "POINT ZM (1 2 3 4)",
        ["'POINT',3)", "0101000080000000000000F03F00000000000000400000000000000840"],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYZ", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(POINTZ)",
            "0101000080000000000000F03F00000000000000400000000000000840",
        ],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYM"],
        "POINT ZM (1 2 3 4)",
        ["'GEOMETRY',3)", "01D1070000000000000000F03F00000000000000400000000000001040"],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYM", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(GEOMETRYM)",
            "01D1070000000000000000F03F00000000000000400000000000001040",
        ],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYM"],
        "POINT ZM (1 2 3 4)",
        ["'POINTM',3)", "01D1070000000000000000F03F00000000000000400000000000001040"],
    ],
    [
        ogr.wkbPointM,
        [],
        "POINT ZM (1 2 3 4)",
        ["'POINTM',3)", "01D1070000000000000000F03F00000000000000400000000000001040"],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYM", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(POINTM)",
            "01D1070000000000000000F03F00000000000000400000000000001040",
        ],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYZM"],
        "POINT ZM (1 2 3 4)",
        [
            "'GEOMETRY',4)",
            "01B90B0000000000000000F03F000000000000004000000000000008400000000000001040",
        ],
    ],
    [
        ogr.wkbUnknown,
        ["DIM=XYZM", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(GEOMETRYZM)",
            "01B90B0000000000000000F03F000000000000004000000000000008400000000000001040",
        ],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYZM"],
        "POINT ZM (1 2 3 4)",
        [
            "'POINT',4)",
            "01B90B0000000000000000F03F000000000000004000000000000008400000000000001040",
        ],
    ],
    [
        ogr.wkbPointZM,
        [],
        "POINT ZM (1 2 3 4)",
        [
            "'POINT',4)",
            "01B90B0000000000000000F03F000000000000004000000000000008400000000000001040",
        ],
    ],
    [
        ogr.wkbPoint,
        ["DIM=XYZM", "GEOM_TYPE=geography"],
        "POINT ZM (1 2 3 4)",
        [
            "geography(POINTZM)",
            "01B90B0000000000000000F03F000000000000004000000000000008400000000000001040",
        ],
    ],
]


@pytest.mark.parametrize("geom_type,options,wkt,expected_strings", tests_zm)
def test_ogr_pgdump_zm(tmp_vsimem, geom_type, options, wkt, expected_strings):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_13.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=geom_type, options=options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_13.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    for expected_string in expected_strings:
        assert expected_string in sql, (geom_type, options, wkt, expected_string)


@pytest.mark.parametrize("geom_type,options,wkt,expected_strings", tests_zm)
def test_ogr_pgdump_zm_creategeomfield(
    tmp_vsimem, geom_type, options, wkt, expected_strings
):
    if "GEOM_TYPE=geography" in options:
        return

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_13.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=options)
    lyr.CreateGeomField(ogr.GeomFieldDefn("my_geom", geom_type))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_13.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    for expected_string in expected_strings:
        assert expected_string in sql, (geom_type, options, wkt, expected_string)


###############################################################################
# Test description


def test_ogr_pgdump_14(tmp_vsimem):

    # Set with DESCRIPTION layer creation option
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_14.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer(
        "ogr_pgdump_14", geom_type=ogr.wkbPoint, options=["DESCRIPTION=foo"]
    )
    # Test that SetMetadata() and SetMetadataItem() are without effect
    lyr.SetMetadata({"DESCRIPTION": "bar"})
    lyr.SetMetadataItem("DESCRIPTION", "baz")
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_14.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    assert (
        """COMMENT ON TABLE "public"."ogr_pgdump_14" IS 'foo';""" in sql
        and "bar" not in sql
        and "baz" not in sql
    )


def test_ogr_pgdump_14_a(tmp_vsimem):

    # Set with SetMetadataItem()
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_14.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("ogr_pgdump_14", geom_type=ogr.wkbPoint)
    lyr.SetMetadataItem("DESCRIPTION", "bar")
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_14.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    gdal.Unlink(tmp_vsimem / "ogr_pgdump_14.sql")
    assert """COMMENT ON TABLE "public"."ogr_pgdump_14" IS 'bar';""" in sql


def test_ogr_pgdump_14_b(tmp_vsimem):

    # Set with SetMetadata()
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_14.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("ogr_pgdump_14", geom_type=ogr.wkbPoint)
    lyr.SetMetadata({"DESCRIPTION": "baz"})
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_14.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    assert """COMMENT ON TABLE "public"."ogr_pgdump_14" IS 'baz';""" in sql


###############################################################################
# NULL vs unset


def test_ogr_pgdump_15(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_15.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull(0)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_15.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    assert (
        'INSERT INTO "public"."test" ("str") VALUES (NULL)' in sql
        or 'INSERT INTO "public"."test" DEFAULT VALUES' in sql
    )


###############################################################################
# Test sequence updating


@pytest.mark.parametrize("pg_use_copy", ["YES", "NO"])
def test_ogr_pgdump_16(tmp_vsimem, pg_use_copy):

    with gdal.config_option("PG_USE_COPY", pg_use_copy):
        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_vsimem / "ogr_pgdump_16.sql", options=["LINEFORMAT=LF"]
        )
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(1)
        lyr.CreateFeature(f)
        f = None
        ds = None

        f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_16.sql", "rb")
        sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
        gdal.VSIFCloseL(f)

        assert (
            """SELECT setval(pg_get_serial_sequence('"public"."test"', 'ogc_fid'), MAX("ogc_fid")) FROM "public"."test";"""
            in sql
        )


###############################################################################
# Test temporary layer creation option


def test_ogr_pgdump_17(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "ogr_pgdump_17.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["TEMPORARY=ON"])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_pgdump_17.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""DROP TABLE IF EXISTS "pg_temp"."test" CASCADE;""")
    check_and_remove("""CREATE TEMPORARY TABLE "test"();""")
    check_and_remove(
        """ALTER TABLE "pg_temp"."test" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove(
        """SELECT AddGeometryColumn('','test','wkb_geometry',0,'POINT',2);"""
    )
    check_and_remove(
        """INSERT INTO "pg_temp"."test" ("wkb_geometry") VALUES ('01010000000000000000000000000000000000F03F');"""
    )
    check_and_remove(
        """CREATE INDEX "test_wkb_geometry_geom_idx" ON "pg_temp"."test" USING GIST ("wkb_geometry");"""
    )


###############################################################################
# Test GEOM_COLUMN_POSITION=END


@pytest.mark.parametrize("pg_use_copy", ["YES", "NO"])
def test_ogr_pgdump_GEOM_COLUMN_POSITION_END(tmp_vsimem, pg_use_copy):

    with gdaltest.config_option("PG_USE_COPY", pg_use_copy):
        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_vsimem / "test_ogr_pgdump_GEOM_COLUMN_POSITION_END.sql",
            options=["LINEFORMAT=LF"],
        )
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbPoint, options=["GEOM_COLUMN_POSITION=END"]
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(10)
        f["str"] = "bar"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
        lyr.CreateFeature(f)
        f = None
        ds = None

    f = gdal.VSIFOpenL(
        tmp_vsimem / "test_ogr_pgdump_GEOM_COLUMN_POSITION_END.sql", "rb"
    )
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""CREATE TABLE "public"."test"();""")
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str" VARCHAR;""")
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','wkb_geometry',0,'POINT',2);"""
    )
    if pg_use_copy == "YES":
        check_and_remove("""COPY "public"."test" ("str", "wkb_geometry") FROM STDIN;""")
        check_and_remove("""foo\t01010000000000000000000000000000000000F03F""")
    else:
        check_and_remove(
            """INSERT INTO "public"."test" ("str", "wkb_geometry") VALUES ('foo', '01010000000000000000000000000000000000F03F');"""
        )
    check_and_remove(
        """INSERT INTO "public"."test" ("ogc_fid", "str", "wkb_geometry") VALUES (10, 'bar', '0101000000000000000000F03F0000000000000040');"""
    )
    check_and_remove(
        """SELECT setval(pg_get_serial_sequence('"public"."test"', 'ogc_fid'), MAX("ogc_fid")) FROM "public"."test";"""
    )
    check_and_remove(
        """CREATE INDEX "test_wkb_geometry_geom_idx" ON "public"."test" USING GIST ("wkb_geometry");"""
    )


###############################################################################
# Test GEOM_COLUMN_POSITION=END and FID=


@pytest.mark.parametrize("pg_use_copy", ["YES", "NO"])
def test_ogr_pgdump_GEOM_COLUMN_POSITION_END_FID_empty(tmp_vsimem, pg_use_copy):

    with gdaltest.config_option("PG_USE_COPY", pg_use_copy):
        ds = ogr.GetDriverByName("PGDump").CreateDataSource(
            tmp_vsimem / "test_ogr_pgdump_GEOM_COLUMN_POSITION_END_FID_empty.sql",
            options=["LINEFORMAT=LF"],
        )
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbPoint, options=["GEOM_COLUMN_POSITION=END", "FID="]
        )
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(10)
        f["str"] = "bar"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
        lyr.CreateFeature(f)
        f = None
        ds = None

    f = gdal.VSIFOpenL(
        tmp_vsimem / "test_ogr_pgdump_GEOM_COLUMN_POSITION_END_FID_empty.sql", "rb"
    )
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    assert (
        """ALTER TABLE "public"."test" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
        not in sql
    )
    assert """SELECT setval(""" not in sql

    check_and_remove("""CREATE TABLE "public"."test"();""")
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str" VARCHAR;""")
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','wkb_geometry',0,'POINT',2);"""
    )
    if pg_use_copy == "YES":
        check_and_remove("""COPY "public"."test" ("str", "wkb_geometry") FROM STDIN;""")
        check_and_remove("""foo\t01010000000000000000000000000000000000F03F""")
    else:
        check_and_remove(
            """INSERT INTO "public"."test" ("str", "wkb_geometry") VALUES ('foo', '01010000000000000000000000000000000000F03F');"""
        )
    check_and_remove(
        """INSERT INTO "public"."test" ("str", "wkb_geometry") VALUES ('bar', '0101000000000000000000F03F0000000000000040');"""
    )
    check_and_remove(
        """CREATE INDEX "test_wkb_geometry_geom_idx" ON "public"."test" USING GIST ("wkb_geometry");"""
    )


###############################################################################
# Test creating a layer without feature


def test_ogr_pgdump_no_feature(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "test_ogr_pgdump_no_feature.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbPoint, options=["GEOM_COLUMN_POSITION=END"]
    )
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "test_ogr_pgdump_no_feature.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove("""CREATE TABLE "public"."test"();""")
    check_and_remove(
        """ALTER TABLE "public"."test" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_pk" PRIMARY KEY;"""
    )
    check_and_remove("""ALTER TABLE "public"."test" ADD COLUMN "str" VARCHAR;""")
    check_and_remove(
        """SELECT AddGeometryColumn('public','test','wkb_geometry',0,'POINT',2);"""
    )
    check_and_remove(
        """CREATE INDEX "test_wkb_geometry_geom_idx" ON "public"."test" USING GIST ("wkb_geometry");"""
    )
    check_and_remove("""COMMIT;""")


###############################################################################
# Test CREATE_TABLE=NO


def test_ogr_pgdump_CREATE_TABLE_NO(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "test_ogr_pgdump_CREATE_TABLE_NO.sql", options=["LINEFORMAT=LF"]
    )
    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["CREATE_TABLE=NO", "GEOM_COLUMN_POSITION=END"],
    )
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "foo"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "test_ogr_pgdump_CREATE_TABLE_NO.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    assert "CREATE " not in sql
    assert "DROP " not in sql
    assert "ALTER " not in sql
    check_and_remove("""BEGIN;""")
    check_and_remove(
        """INSERT INTO "public"."test" ("str", "wkb_geometry") VALUES ('foo', '01010000000000000000000000000000000000F03F');"""
    )
    check_and_remove("""COMMIT;""")


###############################################################################
# Test long identifiers


@pytest.mark.parametrize(
    "launder,long_name,geometry_name,short_name,pk_name,idx_name",
    [
        (
            True,
            "test_" + ("X" * (63 - len("test_"))),
            "wkb_geometry",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_pk",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_a5a5c85f_0_geom_idx",
        ),
        (
            True,
            "test_" + ("X" * (63 - len("test_") - len("wkb_geometry") - 2)),
            "wkb_geometry",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_pk",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_0_geom_idx",
        ),
        (
            True,
            "test_" + ("X" * 64) + "_long_name",
            "wkb_geometry",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_3ba7c630",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_3ba7c_pk",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_05e1f255_0_geom_idx",
        ),
        (
            True,
            "test_" + ("X" * 64) + "_long_name2",
            "wkb_geometry",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_bb4afe1c",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_bb4af_pk",
            "test_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_950ad059_0_geom_idx",
        ),
        (
            False,
            "test_" + ("X" * 64) + "_long_name2",
            "wkb_geometry",
            "test_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX_bb4afe1c",
            "test_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX_bb4af_pk",
            "test_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX_2c8a17fc_0_geom_idx",
        ),
        (
            False,
            "test_" + ("é" * 64) + "_long_name",
            "wkb_geometry",
            "test_ééééééééééééééééééééééééééééééééééééééééééééééééé_aba056f0",
            "test_ééééééééééééééééééééééééééééééééééééééééééééééééé_aba05_pk",
            "test_éééééééééééééééééééééééééééééééééééééé_f883ade2_0_geom_idx",
        ),
        (
            True,
            "TEST_" + ("é" * 64) + "_long_name",
            "wkb_geometry",
            "test_ééééééééééééééééééééééééééééééééééééééééééééééééé_d8582e33",
            "test_ééééééééééééééééééééééééééééééééééééééééééééééééé_d8582_pk",
            "test_éééééééééééééééééééééééééééééééééééééé_6573ce0d_0_geom_idx",
        ),
    ],
)
def test_ogr_pgdump_long_identifiers(
    tmp_vsimem, launder, long_name, geometry_name, short_name, pk_name, idx_name
):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "test_ogr_pgdump_long_identifiers.sql", options=["LINEFORMAT=LF"]
    )

    assert len(short_name) <= 63
    assert len(idx_name) <= 63
    assert len(pk_name) <= 63

    with gdal.quiet_errors():
        lyr = ds.CreateLayer(
            long_name,
            geom_type=ogr.wkbPoint,
            options=[
                "LAUNDER=" + ("YES" if launder else "NO"),
                "GEOMETRY_NAME=" + geometry_name,
            ],
        )
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "foo"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "test_ogr_pgdump_long_identifiers.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove(f"""CREATE TABLE "public"."{short_name}"();""")
    check_and_remove(
        f"""ALTER TABLE "public"."{short_name}" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "{pk_name}" PRIMARY KEY;"""
    )
    check_and_remove(
        f"""CREATE INDEX "{idx_name}" ON "public"."{short_name}" USING GIST ("wkb_geometry");"""
    )


###############################################################################
# Test LAUNDER=YES


def test_ogr_pgdump_LAUNDER_YES(tmp_vsimem):

    eacute = b"\xc3\xa9".decode("utf-8")
    filename = str(tmp_vsimem / "test_ogr_pgdump_LAUNDER_YES.sql")
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(filename)
    lyr = ds.CreateLayer("a" + eacute + "#", options=["LAUNDER=YES"])
    lyr.CreateField(ogr.FieldDefn("b" + eacute + "#"))
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)
    assert '"a' + eacute + '_"' in sql
    assert '"b' + eacute + '_"' in sql


###############################################################################
# Test LAUNDER=NO


def test_ogr_pgdump_LAUNDER_NO(tmp_vsimem):

    eacute = b"\xc3\xa9".decode("utf-8")
    filename = str(tmp_vsimem / "test_ogr_pgdump_LAUNDER_NO.sql")
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(filename)
    lyr = ds.CreateLayer("a" + eacute + "#", options=["LAUNDER=NO"])
    lyr.CreateField(ogr.FieldDefn("b" + eacute + "#"))
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)
    assert '"a' + eacute + '#"' in sql
    assert '"b' + eacute + '#"' in sql


###############################################################################
# Test LAUNDER_ASCII


def test_ogr_pgdump_LAUNDER_ASCII(tmp_vsimem):

    eacute = b"\xc3\xa9".decode("utf-8")
    filename = str(tmp_vsimem / "test_ogr_pgdump_LAUNDER_ASCII.sql")
    ds = ogr.GetDriverByName("PGDump").CreateDataSource(filename)
    lyr = ds.CreateLayer("a" + eacute, options=["LAUNDER_ASCII=YES"])
    lyr.CreateField(ogr.FieldDefn("b" + eacute))
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)
    assert '"ae"' in sql
    assert '"be"' in sql


###############################################################################
# Test SKIP_CONFLICTS


def test_ogr_pgdump_skip_conflicts(tmp_vsimem):

    ds = ogr.GetDriverByName("PGDump").CreateDataSource(
        tmp_vsimem / "test_ogr_pgdump_skip_conflicts.sql"
    )

    with gdal.quiet_errors():
        lyr = ds.CreateLayer(
            "skip_conflicts",
            geom_type=ogr.wkbPoint,
            options=["SKIP_CONFLICTS=YES"],
        )
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "foo"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "test_ogr_pgdump_skip_conflicts.sql", "rb")
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf8")
    gdal.VSIFCloseL(f)

    # print(sql)

    def check_and_remove(needle):
        nonlocal sql
        assert needle in sql, sql
        sql = sql[sql.find(needle) + len(needle) :]

    check_and_remove(") ON CONFLICT DO NOTHING;")
