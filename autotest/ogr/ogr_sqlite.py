#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SQLite driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import shutil

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.require_driver("SQLite"),
]


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(autouse=True, scope="module")
def setup():

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    with gdal.config_option("OGR_SQLITE_SYNCHRONOUS", "OFF"):
        yield


@pytest.fixture(scope="module")
def spatialite_version():

    version = None

    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("SQLite").CreateDataSource(
            "/vsimem/foo.db", options=["SPATIALITE=YES"]
        )

        if ds is not None:
            sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
            feat = sql_lyr.GetNextFeature()
            version = feat.GetFieldAsString(0)
            ds.ReleaseResultSet(sql_lyr)
        ds = None
        gdal.Unlink("/vsimem/foo.db")

    return version


@pytest.fixture()
def require_spatialite(spatialite_version):

    if spatialite_version is None:
        pytest.skip("SpatiaLite not available")


def reopen_sqlite_db(ds, update=False, **kwargs):
    ds_loc = ds.GetDescription()
    ds.FlushCache()

    flags = gdal.OF_VECTOR
    if update:
        flags |= gdal.OF_UPDATE

    return gdal.OpenEx(ds_loc, flags, **kwargs)


###############################################################################
# Create a fresh database.
#
# By default, the database will not be created with SpatiaLite support.
# If a test is parametrized with @pytest.mark.parametrize("spatialite", [True]),
# then SpatiaLite will be enabled. To run the same test both with and without
# SpatiaLite, use @pytest.mark.parametrize("spatialite", [True, False])
#
# Test layers that are defined as fixtures can by included in the returned
# database by marking the test with @pytest.mark.usefixtures("a_layer", "tpoly"), etc.


@pytest.fixture()
def spatialite():
    return False


@pytest.fixture()
def sqlite_test_db(tmp_path, spatialite):

    if spatialite:
        dsco = ["SPATIALITE=YES"]
    else:
        dsco = []

    sqlite_dr = ogr.GetDriverByName("SQLite")
    if sqlite_dr is None:
        pytest.skip()

    sl_ds = sqlite_dr.CreateDataSource(tmp_path / "sqlite_test.db", options=dsco)

    if spatialite and sl_ds is None:
        pytest.skip("Spatialite not available")

    assert sl_ds is not None

    return sl_ds


@pytest.fixture()
def a_layer(sqlite_test_db):
    sqlite_test_db.CreateLayer(
        "a_layer", options=["FID=my_fid", "GEOMETRY_NAME=mygeom", "OVERWRITE=YES"]
    )


###############################################################################
# Create table from data/poly.shp


@pytest.fixture()
def tpoly(sqlite_test_db, poly_feat):

    ######################################################
    # Create Layer
    sl_lyr = sqlite_test_db.CreateLayer("tpoly")

    assert sl_lyr.GetDataset().GetDescription() == sqlite_test_db.GetDescription()

    ######################################################
    # Setup Schema

    fields = [
        ("AREA", ogr.OFTReal),
        ("EAS_ID", ogr.OFTInteger),
        ("PRFEDEA", ogr.OFTString),
        ("BINCONTENT", ogr.OFTBinary),
        ("INT64", ogr.OFTInteger64),
    ]

    ogrtest.quick_create_layer_def(sl_lyr, fields)
    fld_defn = ogr.FieldDefn("fld_boolean", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    sl_lyr.CreateField(fld_defn)

    ######################################################
    # Copy in poly.shp

    feature_def = sl_lyr.GetLayerDefn()

    dst_feat = ogr.Feature(feature_def)

    sl_lyr.StartTransaction()

    for feat in poly_feat:

        dst_feat.SetFrom(feat)
        dst_feat.SetField("int64", 1234567890123)
        sl_lyr.CreateFeature(dst_feat)

    sl_lyr.CommitTransaction()


def test_ogr_sqlite_2a(sqlite_test_db):

    # Test invalid FORMAT
    with gdal.quiet_errors():
        lyr = sqlite_test_db.CreateLayer("will_fail", options=["FORMAT=FOO"])
    assert lyr is None, "layer creation should have failed"


def test_ogr_sqlite_2b(sqlite_test_db):

    # Test creating a layer with an existing name
    lyr = sqlite_test_db.CreateLayer("a_layer")
    with gdal.quiet_errors():
        lyr = sqlite_test_db.CreateLayer("a_layer")
    assert lyr is None, "layer creation should have failed"


def test_ogr_sqlite_2c(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer("a_layer")

    # Test OVERWRITE=YES
    lyr = sqlite_test_db.CreateLayer(
        "a_layer", options=["FID=my_fid", "GEOMETRY_NAME=mygeom", "OVERWRITE=YES"]
    )
    assert lyr is not None, "layer creation should have succeeded"

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)

    assert sqlite_test_db.GetLayerByName("a_layer").GetGeometryColumn() == "mygeom"
    assert sqlite_test_db.GetLayerByName("a_layer").GetFIDColumn() == "my_fid"


def test_ogr_sqlite_2d(sqlite_test_db, poly_feat):

    ######################################################
    # Create Layer
    sl_lyr = sqlite_test_db.CreateLayer("tpoly")

    ######################################################
    # Setup Schema

    fields = [
        ("AREA", ogr.OFTReal),
        ("EAS_ID", ogr.OFTInteger),
        ("PRFEDEA", ogr.OFTString),
        ("BINCONTENT", ogr.OFTBinary),
        ("INT64", ogr.OFTInteger64),
    ]

    ogrtest.quick_create_layer_def(sl_lyr, fields)
    fld_defn = ogr.FieldDefn("fld_boolean", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    sl_lyr.CreateField(fld_defn)

    ######################################################
    # Reopen database to be sure that the data types are properly read
    # even if no record are written

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)
    sl_lyr = sqlite_test_db.GetLayerByName("tpoly")

    assert sl_lyr.GetGeometryColumn() == "GEOMETRY"

    for field_desc in fields:
        feature_def = sl_lyr.GetLayerDefn()
        field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex(field_desc[0]))
        if field_defn.GetType() != field_desc[1]:
            print(
                "Expected type for %s is %s, not %s"
                % (
                    field_desc[0],
                    field_defn.GetFieldTypeName(field_defn.GetType()),
                    field_defn.GetFieldTypeName(field_desc[1]),
                )
            )
    field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex("fld_boolean"))
    assert (
        field_defn.GetType() == ogr.OFTInteger
        and field_defn.GetSubType() == ogr.OFSTBoolean
    )
    field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex("INT64"))
    assert field_defn.GetType() == ogr.OFTInteger64


###############################################################################
# Verify that stuff we just wrote is still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_3(sqlite_test_db, poly_feat):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    assert sl_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(sl_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(sl_lyr, "eas_id", expect)

        assert sl_lyr.GetFeatureCount() == 5

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = sl_lyr.GetNextFeature()

        assert read_feat is not None, "Did not get as many features as expected."

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.001
        )

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )
        assert read_feat.GetField("int64") == 1234567890123


###############################################################################
# Test retrieving layers


@pytest.mark.usefixtures("a_layer", "tpoly")
def test_ogr_sqlite_layers(sqlite_test_db):

    assert sqlite_test_db.GetLayerCount() == 2, "did not get expected layer count"

    lyr = sqlite_test_db.GetLayer(0)
    assert lyr is not None
    assert lyr.GetName() == "a_layer", "did not get expected layer name"
    assert (
        lyr.GetGeomType() == ogr.wkbUnknown
    ), "did not get expected layer geometry type"
    assert lyr.GetFeatureCount() == 0, "did not get expected feature count"

    lyr = sqlite_test_db.GetLayer(1)
    assert lyr is not None
    assert lyr.GetName() == "tpoly", "did not get expected layer name"
    assert (
        lyr.GetGeomType() == ogr.wkbUnknown
    ), "did not get expected layer geometry type"
    assert lyr.GetFeatureCount() == 10, "did not get expected feature count"

    # Test LIST_ALL_TABLES=YES open option
    sl_ds_all_table = reopen_sqlite_db(
        sqlite_test_db, update=True, open_options=["LIST_ALL_TABLES=YES"]
    )

    assert sl_ds_all_table.GetLayerCount() == 5, "did not get expected layer count"
    lyr = sl_ds_all_table.GetLayer(0)
    assert lyr is not None
    assert lyr.GetName() == "a_layer", "did not get expected layer name"
    assert not sl_ds_all_table.IsLayerPrivate(0)

    lyr = sl_ds_all_table.GetLayer(1)
    assert lyr is not None
    assert lyr.GetName() == "tpoly", "did not get expected layer name"
    assert not sl_ds_all_table.IsLayerPrivate(1)

    lyr = sl_ds_all_table.GetLayer(2)
    assert lyr is not None
    assert lyr.GetName() == "geometry_columns", "did not get expected layer name"
    assert sl_ds_all_table.IsLayerPrivate(2)

    lyr = sl_ds_all_table.GetLayer(3)
    assert lyr is not None
    assert lyr.GetName() == "spatial_ref_sys", "did not get expected layer name"
    assert sl_ds_all_table.IsLayerPrivate(3)

    lyr = sl_ds_all_table.GetLayer(4)
    assert lyr is not None
    assert lyr.GetName() == "sqlite_sequence", "did not get expected layer name"
    assert sl_ds_all_table.IsLayerPrivate(4)


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_4(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    wkt_list = ["10", "2", "1", "3d_1", "4", "5", "6"]

    for item in wkt_list:

        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        dst_feat.SetFID(-1)
        sl_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        with ogrtest.attribute_filter(sl_lyr, "PRFEDEA = '%s'" % item):
            feat_read = sl_lyr.GetNextFeature()

        assert feat_read is not None, "Did not get as many features as expected."

        ogrtest.check_feature_geometry(feat_read, geom)


###############################################################################
# Test ExecuteSQL() results layers without geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_5(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")
    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    for _ in range(2):
        sl_lyr.CreateFeature(dst_feat)

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    with sqlite_test_db.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    ) as sql_lyr:

        assert sql_lyr.GetFeatureCount() == 11

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ExecuteSQL() results layers with geometry.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_6(sqlite_test_db):

    with sqlite_test_db.ExecuteSQL(
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


###############################################################################
# Test spatial filtering.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_7(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    with ogrtest.spatial_filter(sl_lyr, "LINESTRING(479505 4763195,480526 4762819)"):

        assert sl_lyr.GetFeatureCount() == 1

        ogrtest.check_features_against_list(sl_lyr, "eas_id", [158])

        with ogrtest.attribute_filter(sl_lyr, "eas_id = 158"):
            assert sl_lyr.GetFeatureCount() == 1


###############################################################################
# Test transactions with rollback.


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_8(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    ######################################################################
    # Prepare working feature.

    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(10 20)"))

    dst_feat.SetField("PRFEDEA", "rollbacktest")

    ######################################################################
    # Create it, but rollback the transaction.

    sl_lyr.StartTransaction()
    sl_lyr.CreateFeature(dst_feat)
    sl_lyr.RollbackTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    with ogrtest.attribute_filter(sl_lyr, "PRFEDEA = 'rollbacktest'"):
        feat_read = sl_lyr.GetNextFeature()

    assert feat_read is None, "Unexpectedly got rollbacktest feature."

    ######################################################################
    # Create it, and commit the transaction.

    sl_lyr.StartTransaction()
    sl_lyr.CreateFeature(dst_feat)
    sl_lyr.CommitTransaction()

    ######################################################################
    # Verify that it is in the layer.

    with ogrtest.attribute_filter(sl_lyr, "PRFEDEA = 'rollbacktest'"):
        feat_read = sl_lyr.GetNextFeature()

    assert feat_read is not None, "Failed to get committed feature."


###############################################################################
# Test SetFeature()


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_9(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    ######################################################################
    # Read feature with EAS_ID 158.

    with ogrtest.attribute_filter(sl_lyr, "eas_id = 158"):
        feat_read = sl_lyr.GetNextFeature()

    assert feat_read is not None, "did not find eas_id 158!"

    ######################################################################
    # Modify the PRFEDEA value, and reset it.

    feat_read.SetField("PRFEDEA", "SetWorked")
    err = sl_lyr.SetFeature(feat_read)
    assert err == 0, "SetFeature() reported error %d" % err

    ######################################################################
    # Read feature with EAS_ID 158 and check that PRFEDEA was altered.

    with ogrtest.attribute_filter(sl_lyr, "eas_id = 158"):
        feat_read_2 = sl_lyr.GetNextFeature()

    assert feat_read_2 is not None, "did not find eas_id 158!"

    if feat_read_2.GetField("PRFEDEA") != "SetWorked":
        feat_read_2.DumpReadable()
        pytest.fail("PRFEDEA apparently not reset as expected.")

    # Test updating non-existing feature
    feat_read.SetFID(-10)
    assert (
        sl_lyr.SetFeature(feat_read) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."

    # Test deleting non-existing feature
    assert (
        sl_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."


###############################################################################
# Test GetFeature()


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_10(sqlite_test_db):

    sl_lyr = sqlite_test_db.GetLayer("tpoly")

    ######################################################################
    # Read feature with EAS_ID 158.

    with ogrtest.attribute_filter(sl_lyr, "eas_id = 158"):
        feat_read = sl_lyr.GetNextFeature()

    assert feat_read is not None, "did not find eas_id 158!"

    ######################################################################
    # Now read the feature by FID.

    feat_read_2 = sl_lyr.GetFeature(feat_read.GetFID())

    assert feat_read_2 is not None, "did not find FID %d" % feat_read.GetFID()

    if feat_read_2.GetField("PRFEDEA") != feat_read.GetField("PRFEDEA"):
        feat_read.DumpReadable()
        feat_read_2.DumpReadable()
        pytest.fail("GetFeature() result seems to not match expected.")


###############################################################################
# Test FORMAT=WKB creation option


def test_ogr_sqlite_11(sqlite_test_db):

    ######################################################
    # Create Layer with WKB geometry
    sl_lyr = sqlite_test_db.CreateLayer("geomwkb", options=["FORMAT=WKB"])

    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")
    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    sl_lyr.CreateFeature(dst_feat)
    dst_feat = None

    # Test adding a column to see if geometry is preserved (#3471)
    sl_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    ######################################################
    # Reopen DB

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)

    sl_lyr = sqlite_test_db.GetLayerByName("geomwkb")

    feat_read = sl_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)


###############################################################################
# Test FORMAT=WKT creation option


def test_ogr_sqlite_12(sqlite_test_db):

    ######################################################
    # Create Layer with WKT geometry
    sl_lyr = sqlite_test_db.CreateLayer("geomwkt", options=["FORMAT=WKT"])

    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")
    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    sl_lyr.CreateFeature(dst_feat)
    dst_feat = None

    # Test adding a column to see if geometry is preserved (#3471)
    sl_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    ######################################################
    # Reopen DB

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)
    sl_lyr = sqlite_test_db.GetLayerByName("geomwkt")

    feat_read = sl_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)
    feat_read = None

    sl_lyr.ResetReading()

    with sqlite_test_db.ExecuteSQL("select * from geomwkt") as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()
        ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)
        feat_read = None

        feat_read = sql_lyr.GetFeature(0)
        ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)
        feat_read = None


###############################################################################
# Test SRID support


def test_ogr_sqlite_13(sqlite_test_db):

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    sl_lyr = sqlite_test_db.CreateLayer("wgs84layer", srs=srs)

    ######################################################
    # Reopen DB
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)
    sl_lyr = sqlite_test_db.GetLayerByName("wgs84layer")

    assert sl_lyr.GetSpatialRef().IsSame(
        srs, options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
    ), "SRS is not the one expected."

    ######################################################
    # Create second layer with very approximative EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]'
    )
    sl_lyr = sqlite_test_db.CreateLayer("wgs84layer_approx", srs=srs)

    # Must still be 1
    with sqlite_test_db.ExecuteSQL(
        "SELECT COUNT(*) AS count FROM spatial_ref_sys"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        assert feat.GetFieldAsInteger("count") == 1


###############################################################################
# Test all column types


def test_ogr_sqlite_14(sqlite_test_db):

    sl_lyr = sqlite_test_db.CreateLayer("testtypes")
    ogrtest.quick_create_layer_def(
        sl_lyr,
        [
            ("INTEGER", ogr.OFTInteger),
            ("FLOAT", ogr.OFTReal),
            ("STRING", ogr.OFTString),
            ("BLOB", ogr.OFTBinary),
            ("BLOB2", ogr.OFTBinary),
        ],
    )

    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())

    dst_feat.SetField("INTEGER", 1)
    dst_feat.SetField("FLOAT", 1.2)
    dst_feat.SetField("STRING", "myString'a")
    dst_feat.SetField("BLOB", b"\x00\x01\xff")

    sl_lyr.CreateFeature(dst_feat)

    ######################################################
    # Reopen DB
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)
    sl_lyr = sqlite_test_db.GetLayerByName("testtypes")

    # Duplicate the first record
    dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
    feat_read = sl_lyr.GetNextFeature()
    dst_feat.SetFrom(feat_read)
    sl_lyr.CreateFeature(dst_feat)

    # Check the 2 records
    sl_lyr.ResetReading()
    for _ in range(2):
        feat_read = sl_lyr.GetNextFeature()
        assert (
            feat_read.GetField("INTEGER") == 1
            and feat_read.GetField("FLOAT") == 1.2
            and feat_read.GetField("STRING") == "myString'a"
            and feat_read.GetFieldAsString("BLOB") == "0001FF"
        )


###############################################################################
# Test FORMAT=SPATIALITE layer creation option


def test_ogr_sqlite_15(sqlite_test_db):

    ######################################################
    # Create Layer with SPATIALITE geometry
    with gdal.quiet_errors():
        sl_lyr = sqlite_test_db.CreateLayer(
            "geomspatialite", options=["FORMAT=SPATIALITE"]
        )

    geoms = [
        ogr.CreateGeometryFromWkt("POINT(0 1)"),
        ogr.CreateGeometryFromWkt("MULTIPOINT EMPTY"),
        ogr.CreateGeometryFromWkt("MULTIPOINT (0 1,2 3)"),
        ogr.CreateGeometryFromWkt("LINESTRING EMPTY"),
        ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"),
        ogr.CreateGeometryFromWkt("MULTILINESTRING EMPTY"),
        ogr.CreateGeometryFromWkt("MULTILINESTRING ((1 2,3 4),(5 6,7 8))"),
        ogr.CreateGeometryFromWkt("POLYGON EMPTY"),
        ogr.CreateGeometryFromWkt("POLYGON ((1 2,3 4))"),
        ogr.CreateGeometryFromWkt("POLYGON ((1 2,3 4),(5 6,7 8))"),
        ogr.CreateGeometryFromWkt("MULTIPOLYGON EMPTY"),
        ogr.CreateGeometryFromWkt("MULTIPOLYGON (((1 2,3 4)),((5 6,7 8)))"),
        ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION EMPTY"),
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((5 6,7 8)))"
        ),
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POINT(0 1))"
        ),
    ]

    sl_lyr.StartTransaction()

    for geom in geoms:
        dst_feat = ogr.Feature(feature_def=sl_lyr.GetLayerDefn())
        dst_feat.SetGeometry(geom)
        sl_lyr.CreateFeature(dst_feat)

    sl_lyr.CommitTransaction()

    ######################################################
    # Reopen DB
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=False)

    sl_lyr = sqlite_test_db.GetLayerByName("geomspatialite")

    for geom in geoms:
        feat_read = sl_lyr.GetNextFeature()
        ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)

    sl_lyr.ResetReading()

    with sqlite_test_db.ExecuteSQL("select * from geomspatialite") as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()
        ogrtest.check_feature_geometry(feat_read, geoms[0], max_error=0.001)

        feat_read = sql_lyr.GetFeature(0)
        ogrtest.check_feature_geometry(feat_read, geoms[0], max_error=0.001)


def test_ogr_sqlite_15bis(sqlite_test_db):

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=False)

    # Test creating a layer on a read-only DB
    with gdal.quiet_errors():
        lyr = sqlite_test_db.CreateLayer("will_fail")
    assert lyr is None, "layer creation should have failed"


###############################################################################
# Test reading geometries in FGF (FDO Geometry Format) binary representation.


def test_ogr_sqlite_16(sqlite_test_db):

    # Hand create a table with FGF geometry
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO geometry_columns (f_table_name, f_geometry_column, geometry_type, coord_dimension, geometry_format) VALUES ('fgf_table', 'GEOMETRY', 0, 2, 'FGF')"
    )
    sqlite_test_db.ExecuteSQL(
        "CREATE TABLE fgf_table (OGC_FID INTEGER PRIMARY KEY, GEOMETRY BLOB)"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (1, X'0100000000000000000000000000F03F0000000000000040')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (2, X'020000000000000000000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (3, X'020000000000000002000000000000000000F03F000000000000004000000000000008400000000000001040')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (4, X'030000000000000000000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (5, X'03000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (6, X'0700000000000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (7, X'070000000200000003000000000000000200000002000000000000000000F03F0000000000000040000000000000084000000000000010400000000003000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (8, X'0100000001000000000000000000F03F00000000000000400000000000000840')"
    )

    # invalid geometries
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (9, X'0700000001000000')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (10,X'060000000100000001')"
    )
    sqlite_test_db.ExecuteSQL(
        "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (11,X'06000000010000000100000000000000000000000000F03F0000000000000040')"
    )

    ######################################################
    # Reopen DB
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)

    sl_lyr = sqlite_test_db.GetLayerByName("fgf_table")

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POINT (1 2)"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "LINESTRING EMPTY"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "LINESTRING (1 2,3 4)"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POLYGON EMPTY"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POLYGON ((1 2,3 4))"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "GEOMETRYCOLLECTION EMPTY"

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt()
        == "GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((1 2,3 4)))"
    )

    feat = sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "POINT (1 2 3)"

    # Test invalid geometries
    for _ in range(3):
        feat = sl_lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        assert geom is None


###############################################################################
# Test SPATIALITE dataset creation option


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_sqlite_17(sqlite_test_db):

    with gdal.quiet_errors():
        lyr = sqlite_test_db.CreateLayer("will_fail", options=["FORMAT=WKB"])
    assert lyr is None, "layer creation should have failed"

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = sqlite_test_db.CreateLayer("geomspatialite", srs=srs)

    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    lyr.CreateFeature(dst_feat)

    ######################################################
    # Reopen DB
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=False)
    lyr = sqlite_test_db.GetLayerByName("geomspatialite")

    feat_read = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("4326") != -1, "did not identify correctly SRS"


###############################################################################
# Create a layer with a non EPSG SRS into a SPATIALITE DB (#3506)


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_sqlite_18(sqlite_test_db):

    ds = reopen_sqlite_db(sqlite_test_db, update=True)

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=vandg")
    lyr = ds.CreateLayer("nonepsgsrs", srs=srs)

    ######################################################
    # Reopen DB
    ds = reopen_sqlite_db(ds, update=False)

    lyr = ds.GetLayerByName("nonepsgsrs")
    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("VanDerGrinten") != -1, "did not identify correctly SRS"

    with ds.ExecuteSQL(
        "SELECT * FROM spatial_ref_sys ORDER BY srid DESC LIMIT 1"
    ) as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        if (
            feat.GetField("auth_name") != "OGR"
            or feat.GetField("proj4text").find("+proj=vandg") != 0
        ):
            feat.DumpReadable()
            pytest.fail()


###############################################################################
# Create a SpatiaLite DB with INIT_WITH_EPSG=YES


def test_ogr_sqlite_19(tmp_path, spatialite_version):

    if spatialite_version is None or spatialite_version < "2.3.1":
        pytest.skip("SpatiaLite >= 2.3.1 required")

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmp_path / "spatialite_test_with_epsg.db",
        options=["SPATIALITE=YES", "INIT_WITH_EPSG=YES"],
    )

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:26632")
    ds.CreateLayer("test", srs=srs)

    ds = None
    ds = ogr.Open(tmp_path / "spatialite_test_with_epsg.db")

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    # Currently the injection of the EPSG DB as proj.4 strings adds 3915 entries
    assert nb_srs >= 3915, "did not get expected SRS count"


###############################################################################
# Create a SpatiaLite DB with INIT_WITH_EPSG=NO


def test_ogr_sqlite_19_bis(tmp_vsimem, spatialite_version):

    if spatialite_version is None or spatialite_version[0] < "4":
        pytest.skip("SpatiaLite 4 required")

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmp_vsimem / "spatialite_test_without_epsg.db",
        options=["SPATIALITE=YES", "INIT_WITH_EPSG=NO"],
    )

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:26632")
    ds.CreateLayer("test", srs=srs)

    ds = None
    ds = ogr.Open(tmp_vsimem / "spatialite_test_without_epsg.db")

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    assert nb_srs == 1, "did not get expected SRS count"


###############################################################################
# Create a regular DB with INIT_WITH_EPSG=YES


def test_ogr_sqlite_20(tmp_path):

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmp_path / "non_spatialite_test_with_epsg.db", options=["INIT_WITH_EPSG=YES"]
    )

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:26632")
    ds.CreateLayer("test", srs=srs)

    ds = None
    ds = ogr.Open(tmp_path / "non_spatialite_test_with_epsg.db")

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    # Currently the injection of the EPSG DB as proj.4 strings adds 3945 entries
    assert nb_srs >= 3945, "did not get expected SRS count"


###############################################################################
# Test CopyLayer() from a table layer (#3617)


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_21(sqlite_test_db):

    src_lyr = sqlite_test_db.GetLayerByName("tpoly")
    copy_lyr = sqlite_test_db.CopyLayer(src_lyr, "tpoly_2")

    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    assert src_lyr_count == copy_lyr_count, "did not get same number of features"


###############################################################################
# Test CopyLayer() from a result layer (#3617)


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_22(sqlite_test_db):

    src_lyr = sqlite_test_db.ExecuteSQL("select * from tpoly")
    copy_lyr = sqlite_test_db.CopyLayer(src_lyr, "tpoly_3")

    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    assert src_lyr_count == copy_lyr_count, "did not get same number of features"

    sqlite_test_db.ReleaseResultSet(src_lyr)


###############################################################################
# Test ignored fields works ok


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_23(sqlite_test_db):

    shp_layer = sqlite_test_db.GetLayerByName("tpoly")
    shp_layer.SetIgnoredFields(["AREA"])

    shp_layer.ResetReading()
    feat = shp_layer.GetNextFeature()

    assert not feat.IsFieldSet("AREA"), "got area despite request to ignore it."

    assert feat.GetFieldAsInteger("EAS_ID") == 168, "missing or wrong eas_id"

    wkt = "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    ogrtest.check_feature_geometry(feat, wkt, max_error=0.00000001)

    fd = shp_layer.GetLayerDefn()
    fld = fd.GetFieldDefn(0)  # area
    assert fld.IsIgnored(), "AREA unexpectedly not marked as ignored."

    fld = fd.GetFieldDefn(1)  # eas_id
    assert not fld.IsIgnored(), "EASI unexpectedly marked as ignored."

    assert not fd.IsGeometryIgnored(), "geometry unexpectedly ignored."

    assert not fd.IsStyleIgnored(), "style unexpectedly ignored."

    fd.SetGeometryIgnored(1)

    assert fd.IsGeometryIgnored(), "geometry unexpectedly not ignored."

    feat = shp_layer.GetNextFeature()

    assert feat.GetGeometryRef() is None, "Unexpectedly got a geometry on feature 2."

    assert not feat.IsFieldSet("AREA"), "got area despite request to ignore it."

    assert feat.GetFieldAsInteger("EAS_ID") == 179, "missing or wrong eas_id"


###############################################################################
# Test that ExecuteSQL() with OGRSQL dialect doesn't forward the where clause to sqlite (#4022)


def test_ogr_sqlite_24(tmp_path):

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmp_path / "test24.sqlite")
    lyr = ds.CreateLayer("test")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2 3)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((4 5,6 7))"))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open(tmp_path / "test24.sqlite")

    with gdal.quiet_errors():
        lyr = ds.ExecuteSQL("select OGR_GEOMETRY from test")
    if lyr is not None:
        ds.ReleaseResultSet(lyr)
        pytest.fail("this should not work (1)")

    lyr = ds.ExecuteSQL("select * from test")
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, "a feature was expected (2)"

    lyr = ds.GetLayerByName("test")
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    with gdal.quiet_errors():
        feat = lyr.GetNextFeature()
    assert feat is None, "a feature was not expected (3)"

    lyr = ds.ExecuteSQL("select OGR_GEOMETRY from test", dialect="OGRSQL")
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, "a feature was expected (4)"

    lyr = ds.ExecuteSQL(
        "select OGR_GEOMETRY from test WHERE OGR_GEOMETRY = 'POLYGON'", dialect="OGRSQL"
    )
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, "a feature was expected (5)"

    ds = None


###############################################################################


def get_sqlite_version():
    ds = ogr.Open(":memory:")
    sql_lyr = ds.ExecuteSQL("SELECT sqlite_version()")
    feat = sql_lyr.GetNextFeature()
    sqlite_version = feat.GetFieldAsString(0)
    print("SQLite version : %s" % sqlite_version)
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    return sqlite_version


###############################################################################
# Test opening a /vsicurl/ DB


@pytest.mark.require_curl()
def test_ogr_sqlite_25(tmp_vsimem):

    # Check that we have SQLite VFS support
    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("SQLite").CreateDataSource(
            tmp_vsimem / "ogr_sqlite_25.db"
        )
    if ds is None:
        pytest.skip("SQLite does not have VFS support")
    ds = None

    with gdal.config_option("GDAL_HTTP_TIMEOUT", "5"):
        ds = ogr.Open("/vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db")
    if ds is None:
        if (
            gdaltest.gdalurlopen(
                "http://download.osgeo.org/gdal/data/sqlite3/polygon.db", timeout=4
            )
            is None
        ):
            pytest.skip("cannot open URL")
        pytest.fail()

    lyr = ds.GetLayerByName("polygon")
    assert lyr is not None

    assert lyr.GetLayerDefn().GetFieldCount() != 0


###############################################################################
# Test creating a :memory: DB


def test_ogr_sqlite_26():

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(":memory:")
    sql_lyr = ds.ExecuteSQL("select count(*) from geometry_columns")
    assert sql_lyr is not None, "expected existing geometry_columns"

    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert count == 1, "expected existing geometry_columns"


###############################################################################
# Run test_ogrsf


def test_ogr_sqlite_27(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path()
        + f" -f SQLite {tmp_path}/ogr_sqlite_27.sqlite data/poly.shp --config OGR_SQLITE_SYNCHRONOUS OFF"
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" {tmp_path}/ogr_sqlite_27.sqlite"
    )

    pos = ret.find("ERROR: poLayerFeatSRS != NULL && poSQLFeatSRS == NULL.")
    if pos != -1:
        # Detect if libsqlite3 has been built with SQLITE_HAS_COLUMN_METADATA
        # If not, that explains the error.
        ds = ogr.Open(":memory:")
        sql_lyr = ds.ExecuteSQL("SQLITE_HAS_COLUMN_METADATA()")
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)
        ds.ReleaseResultSet(sql_lyr)
        if val == 0:
            ret = (
                ret[0:pos]
                + ret[
                    pos
                    + len("ERROR: poLayerFeatSRS != NULL && poSQLFeatSRS == NULL.") :
                ]
            )

            # And remove ERROR ret code consequently
            pos = ret.find("ERROR ret code = 1")
            if pos != -1:
                ret = ret[0:pos]

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    # Test on a result SQL layer
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f' -ro {tmp_path}/ogr_sqlite_27.sqlite -sql "SELECT * FROM poly"'
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Run test_ogrsf on a spatialite enabled DB


def test_ogr_sqlite_28(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # Test with a Spatialite 3.0 DB
    shutil.copy("data/sqlite/poly_spatialite.sqlite", tmp_path)
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" {tmp_path}/poly_spatialite.sqlite"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


def test_ogr_sqlite_28a():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # Test on a result SQL layer
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + ' -ro data/sqlite/poly_spatialite.sqlite -sql "SELECT * FROM poly"'
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


def test_ogr_sqlite_28b(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # Test with a Spatialite 4.0 DB
    shutil.copy("data/sqlite/poly_spatialite4.sqlite", tmp_path)
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" {tmp_path}/poly_spatialite4.sqlite"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


def test_ogr_sqlite_28c(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # Generic test
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -driver SQLite -dsco SPATIALITE=YES"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test CreateFeature() with empty feature


def test_ogr_sqlite_29(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer("test")
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0


###############################################################################
# Test ExecuteSQL() with empty result set (#4684)


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_30(sqlite_test_db):

    with sqlite_test_db.ExecuteSQL(
        "SELECT * FROM tpoly WHERE eas_id = 12345"
    ) as sql_lyr:

        # Test fix added in r24768
        feat = sql_lyr.GetNextFeature()
        assert feat is None


###############################################################################
# Test spatial filter when SpatiaLite is available


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_2(sqlite_test_db):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:4326")
    lyr = sqlite_test_db.CreateLayer("test_spatialfilter", srs=srs)
    lyr.CreateField(ogr.FieldDefn("intcol", ogr.OFTInteger))

    lyr.StartTransaction()

    for i in range(10):
        for j in range(10):
            geom = ogr.CreateGeometryFromWkt("POINT(%d %d)" % (i, j))
            dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
            dst_feat.SetGeometry(geom)
            lyr.CreateFeature(dst_feat)

    geom = ogr.CreateGeometryFromWkt("POLYGON((0 0,0 3,3 3,3 0,0 0))")
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    lyr.CreateFeature(dst_feat)
    dst_feat = None

    lyr.CommitTransaction()

    # Test OLCFastFeatureCount with spatial index (created by default)
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=False)
    lyr = sqlite_test_db.GetLayerByName("test_spatialfilter")

    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) is True

    extent = lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

    # Test caching
    extent = lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

    geom = ogr.CreateGeometryFromWkt("POLYGON((2 2,2 8,8 8,8 2,2 2))")
    lyr.SetSpatialFilter(geom)

    assert (
        lyr.TestCapability(ogr.OLCFastFeatureCount) is not False
    ), "OLCFastFeatureCount failed"
    assert (
        lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
    ), "OLCFastSpatialFilter failed"

    assert lyr.GetFeatureCount() == 50, "did not get expected feature count"

    # Test spatial filter with a SQL result layer without WHERE clause
    with sqlite_test_db.ExecuteSQL("SELECT * FROM 'test_spatialfilter'") as sql_lyr:

        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        # Test caching
        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        sql_lyr.SetSpatialFilter(geom)
        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        assert sql_lyr.GetFeatureCount() == 50, "did not get expected feature count"

    # Test spatial filter with a SQL result layer with WHERE clause
    with sqlite_test_db.ExecuteSQL(
        "SELECT * FROM test_spatialfilter WHERE 1=1"
    ) as sql_lyr:
        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        sql_lyr.SetSpatialFilter(geom)
        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        assert sql_lyr.GetFeatureCount() == 50, "did not get expected feature count"

    # Test spatial filter with a SQL result layer with ORDER BY clause
    with sqlite_test_db.ExecuteSQL(
        "SELECT * FROM test_spatialfilter ORDER BY intcol"
    ) as sql_lyr:

        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        # Test caching
        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        sql_lyr.SetSpatialFilter(geom)
        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        assert sql_lyr.GetFeatureCount() == 50, "did not get expected feature count"

    # Test spatial filter with a SQL result layer with WHERE and ORDER BY clause
    with sqlite_test_db.ExecuteSQL(
        "SELECT * FROM test_spatialfilter WHERE 1 = 1 ORDER BY intcol"
    ) as sql_lyr:

        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        # Test caching
        extent = sql_lyr.GetExtent()
        assert extent == (0.0, 9.0, 0.0, 9.0), "got bad extent"

        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        sql_lyr.SetSpatialFilter(geom)
        assert (
            sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False
        ), "OLCFastSpatialFilter failed"
        assert sql_lyr.GetFeatureCount() == 50, "did not get expected feature count"

    # Remove spatial index
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)
    with sqlite_test_db.ExecuteSQL(
        "SELECT DisableSpatialIndex('test_spatialfilter', 'Geometry')"
    ) as sql_lyr:
        sql_lyr.GetFeatureCount()
        feat = sql_lyr.GetNextFeature()
        ret = feat.GetFieldAsInteger(0)

    assert ret == 1, "DisableSpatialIndex failed"

    sqlite_test_db.ExecuteSQL("VACUUM")

    # Test OLCFastFeatureCount without spatial index
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)
    lyr = sqlite_test_db.GetLayerByName("test_spatialfilter")

    geom = ogr.CreateGeometryFromWkt("POLYGON((2 2,2 8,8 8,8 2,2 2))")
    lyr.SetSpatialFilter(geom)

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) is not True
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) is not True

    assert lyr.GetFeatureCount() == 50


###############################################################################
# Test VirtualShape feature of SpatiaLite


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_3(sqlite_test_db):

    sqlite_test_db.ExecuteSQL(
        "CREATE VIRTUAL TABLE testpoly USING VirtualShape(data/shp/testpoly, CP1252, -1)"
    )

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=False)

    lyr = sqlite_test_db.GetLayerByName("testpoly")
    assert lyr is not None

    lyr.SetSpatialFilterRect(-400, 22, -120, 400)

    ogrtest.check_features_against_list(lyr, "FID", [0, 4, 8])


###############################################################################
# Test updating a spatialite DB (#3471 and #3474)


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_4(sqlite_test_db):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = sqlite_test_db.CreateLayer("geomspatialite", srs=srs)

    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    lyr.CreateFeature(dst_feat)

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db, update=True)

    with sqlite_test_db.ExecuteSQL("SELECT * FROM sqlite_master") as lyr:
        nb_sqlite_master_objects_before = lyr.GetFeatureCount()

    with sqlite_test_db.ExecuteSQL("SELECT * FROM idx_geomspatialite_GEOMETRY") as lyr:
        nb_idx_before = lyr.GetFeatureCount()

    lyr = sqlite_test_db.GetLayerByName("geomspatialite")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    with sqlite_test_db.ExecuteSQL("SELECT * FROM geomspatialite") as lyr:
        feat = lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        assert geom is not None and geom.ExportToWkt() == "POINT (0 1)"

    # Check that triggers and index are restored (#3474)
    with sqlite_test_db.ExecuteSQL("SELECT * FROM sqlite_master") as lyr:
        nb_sqlite_master_objects_after = lyr.GetFeatureCount()

    assert (
        nb_sqlite_master_objects_before == nb_sqlite_master_objects_after
    ), "nb_sqlite_master_objects_before=%d, nb_sqlite_master_objects_after=%d" % (
        nb_sqlite_master_objects_before,
        nb_sqlite_master_objects_after,
    )

    # Add new feature
    lyr = sqlite_test_db.GetLayerByName("geomspatialite")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(100 -100)"))
    lyr.CreateFeature(feat)

    # Check that the trigger is functional (#3474).
    with sqlite_test_db.ExecuteSQL("SELECT * FROM idx_geomspatialite_GEOMETRY") as lyr:
        nb_idx_after = lyr.GetFeatureCount()

    assert nb_idx_before + 1 == nb_idx_after, "nb_idx_before=%d, nb_idx_after=%d" % (
        nb_idx_before,
        nb_idx_after,
    )


###############################################################################
# Test writing and reading back spatialite geometries (#4092)
# Test writing and reading back spatialite geometries in compressed form


@pytest.mark.parametrize("spatialite", [True])
@pytest.mark.parametrize(
    "bUseComprGeom",
    [False, True],
    ids=["dont-compress-geometries", "compress-geometries"],
)
def test_ogr_spatialite_5(sqlite_test_db, spatialite_version, bUseComprGeom):
    if bUseComprGeom and spatialite_version == "2.3.1":
        pytest.skip()

    ds = sqlite_test_db
    del sqlite_test_db

    geometries = [
        # 'POINT EMPTY',
        "POINT (1 2)",
        "POINT Z (1 2 3)",
        "POINT M (1 2 3)",
        "POINT ZM (1 2 3 4)",
        "LINESTRING EMPTY",
        "LINESTRING (1 2)",
        "LINESTRING (1 2,3 4)",
        "LINESTRING (1 2,3 4,5 6)",
        "LINESTRING Z (1 2 3,4 5 6)",
        "LINESTRING Z (1 2 3,4 5 6,7 8 9)",
        "LINESTRING M (1 2 3,4 5 6)",
        "LINESTRING M (1 2 3,4 5 6,7 8 9)",
        "LINESTRING ZM (1 2 3 4,5 6 7 8)",
        "LINESTRING ZM (1 2 3 4,5 6 7 8,9 10 11 12)",
        "POLYGON EMPTY",
        "POLYGON ((1 2,1 3,2 3,2 2,1 2))",
        "POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))",
        "POLYGON M ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))",
        "POLYGON ZM ((1 2 10 20,1 3 -10 -20,2 3 20 30,2 2 -20 -30,1 2 10 20))",
        "POLYGON ((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25))",
        "MULTIPOINT EMPTY",
        "MULTIPOINT ((1 2),(3 4))",
        "MULTIPOINT Z ((1 2 3),(4 5 6))",
        "MULTIPOINT M ((1 2 3),(4 5 6))",
        "MULTIPOINT ZM ((1 2 3 4),(5 6 7 8))",
        "MULTILINESTRING EMPTY",
        "MULTILINESTRING ((1 2,3 4),(5 6,7 8))",
        "MULTILINESTRING Z ((1 2 3,4 5 6),(7 8 9,10 11 12))",
        "MULTILINESTRING M ((1 2 3,4 5 6),(7 8 9,10 11 12))",
        "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8),(9 10 11 12,13 14 15 16))",
        "MULTIPOLYGON EMPTY",
        "MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))",
        "MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))",
        "MULTIPOLYGON Z (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0)))",
        "MULTIPOLYGON M (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0)))",
        "MULTIPOLYGON ZM (((1 2 -4 -40,1 3 -3 -30,2 3 -3 -30,2 2 -3 30,1 2 -6 -60)),((-1 -2 0 0,-1 -3 0 0,-2 -3 0 0,-2 -2 0 0,-1 -2 0 0)))",
        "GEOMETRYCOLLECTION EMPTY",
        # 'GEOMETRYCOLLECTION (GEOMETRYCOLLECTION EMPTY)',
        "GEOMETRYCOLLECTION (POINT (1 2))",
        "GEOMETRYCOLLECTION Z (POINT Z (1 2 3))",
        "GEOMETRYCOLLECTION M (POINT M (1 2 3))",
        "GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 4))",
        "GEOMETRYCOLLECTION (LINESTRING (1 2,3 4))",
        "GEOMETRYCOLLECTION Z (LINESTRING Z (1 2 3,4 5 6))",
        "GEOMETRYCOLLECTION (POLYGON ((1 2,1 3,2 3,2 2,1 2)))",
        "GEOMETRYCOLLECTION Z (POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))",
        "GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,3 4),POLYGON ((1 2,1 3,2 3,2 2,1 2)))",
        "GEOMETRYCOLLECTION Z (POINT Z (1 2 3),LINESTRING Z (1 2 3,4 5 6),POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))",
    ]

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    num_layer = 0
    for wkt in geometries:
        # print(wkt)
        geom = ogr.CreateGeometryFromWkt(wkt)
        if bUseComprGeom:
            options = ["COMPRESS_GEOM=YES"]
        else:
            options = []
        lyr = ds.CreateLayer(
            "test%d" % num_layer,
            geom_type=geom.GetGeometryType(),
            srs=srs,
            options=options,
        )
        feat = ogr.Feature(lyr.GetLayerDefn())
        # print(geom)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        num_layer = num_layer + 1

    ds = reopen_sqlite_db(ds)
    num_layer = 0
    for wkt in geometries:
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = ds.GetLayer(num_layer)
        assert lyr.GetGeomType() == geom.GetGeometryType()
        feat = lyr.GetNextFeature()
        got_wkt = feat.GetGeometryRef().ExportToIsoWkt()
        # Spatialite < 2.4 only supports 2D geometries
        if spatialite_version < "2.4" and (geom.GetGeometryType() & ogr.wkb25DBit) != 0:
            geom.SetCoordinateDimension(2)
            expected_wkt = geom.ExportToIsoWkt()
            assert got_wkt == expected_wkt
        elif got_wkt != wkt:
            pytest.fail("got %s, expected %s" % (got_wkt, wkt))

        num_layer = num_layer + 1

    if bUseComprGeom:
        num_layer = 0
        for wkt in geometries:
            if wkt.find("EMPTY") == -1 and wkt.find("POINT") == -1:
                sql_lyr = ds.ExecuteSQL(
                    "SELECT GEOMETRY == CompressGeometry(GEOMETRY) FROM test%d"
                    % num_layer
                )
                feat = sql_lyr.GetNextFeature()
                val = feat.GetFieldAsInteger(0)
                if wkt != "LINESTRING (1 2)":
                    if val != 1:
                        print(wkt)
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        pytest.fail("did not get expected compressed geometry")
                else:
                    if val != 0:
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        pytest.fail(wkt)
                feat = None
                ds.ReleaseResultSet(sql_lyr)
            num_layer = num_layer + 1

    ds = None


###############################################################################
# Test spatialite spatial views


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_6(sqlite_test_db, spatialite_version):
    if spatialite_version.startswith("2.3"):
        pytest.skip()

    ds = sqlite_test_db
    del sqlite_test_db

    if int(spatialite_version[0 : spatialite_version.find(".")]) >= 4:
        layername = "regular_layer"
        layername_single = "regular_layer"
        viewname = "view_of_regular_layer"
        viewname_single = "view_of_regular_layer"
        thegeom_single = "the_geom"
        pkid_single = "pk_id"
    else:
        layername = "regular_'layer"
        layername_single = "regular_''layer"
        viewname = "view_of_'regular_layer"
        viewname_single = "view_of_''regular_layer"
        thegeom_single = 'the_"' "geom"
        pkid_single = 'pk_"' "id"

    # Create regular layer
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(
        layername, geom_type=ogr.wkbPoint, srs=srs, options=["LAUNDER=NO"]
    )

    geometryname = lyr.GetGeometryColumn()

    lyr.CreateField(ogr.FieldDefn("int'col", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("realcol", ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt("POINT(2 49)")
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt("POINT(3 50)")
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 34)
    feat.SetField(1, 56.78)
    geom = ogr.CreateGeometryFromWkt("POINT(-30000 -50000)")
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)
    geom = ogr.CreateGeometryFromWkt("POINT(3 50)")
    feat.SetGeometryDirectly(geom)
    lyr.SetFeature(feat)

    # Create spatial view
    ds.ExecuteSQL(
        'CREATE VIEW "%s" AS SELECT OGC_FID AS \'%s\', %s AS \'%s\', "int\'col", realcol FROM "%s"'
        % (viewname, pkid_single, geometryname, thegeom_single, layername)
    )

    if int(spatialite_version[0 : spatialite_version.find(".")]) >= 4:
        ds.ExecuteSQL(
            "INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column, read_only) VALUES "
            + "('%s', '%s', '%s', '%s', Lower('%s'), 1)"
            % (
                viewname_single,
                thegeom_single,
                pkid_single,
                layername_single,
                geometryname,
            )
        )
    else:
        ds.ExecuteSQL(
            "INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column) VALUES "
            + "('%s', '%s', '%s', '%s', '%s')"
            % (
                viewname_single,
                thegeom_single,
                pkid_single,
                layername_single,
                geometryname,
            )
        )

    # Test spatial view
    ds = reopen_sqlite_db(ds)
    lyr = ds.GetLayerByName(layername)
    view_lyr = ds.GetLayerByName(viewname)
    assert view_lyr.GetFIDColumn() == pkid_single, view_lyr.GetGeometryColumn()
    assert view_lyr.GetGeometryColumn() == thegeom_single
    assert view_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "int'col"
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    assert view_lyr.GetSpatialRef().IsSame(lyr.GetSpatialRef()) == 1
    feat = view_lyr.GetFeature(3)
    if feat.GetFieldAsInteger(0) != 34:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetFieldAsDouble(1) != 56.78:
        feat.DumpReadable()
        pytest.fail()
    view_lyr.SetAttributeFilter('"int\'col" = 34')
    view_lyr.SetSpatialFilterRect(2.5, 49.5, 3.5, 50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != "POINT (3 50)":
        feat.DumpReadable()
        pytest.fail()

    # Remove spatial index
    ds = reopen_sqlite_db(ds, update=1)
    sql_lyr = ds.ExecuteSQL(
        "SELECT DisableSpatialIndex('%s', '%s')" % (layername_single, geometryname)
    )
    ds.ReleaseResultSet(sql_lyr)
    ds.ExecuteSQL('DROP TABLE "idx_%s_%s"' % (layername, geometryname))

    # Test spatial view again
    ds = reopen_sqlite_db(ds)
    view_lyr = ds.GetLayerByName(viewname)
    view_lyr.SetAttributeFilter('"int\'col" = 34')
    view_lyr.SetSpatialFilterRect(2.5, 49.5, 3.5, 50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        feat.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test VirtualShape:xxx.shp


def test_ogr_spatialite_7(require_spatialite):
    ds = ogr.Open("VirtualShape:data/poly.shp")
    assert ds is not None

    lyr = ds.GetLayerByName("poly")
    assert lyr is not None

    assert lyr.GetGeomType() == ogr.wkbPolygon

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None


###############################################################################
# Test tables with multiple geometry columns (#4768)


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_8(sqlite_test_db, spatialite_version):
    if spatialite_version.startswith("2.3"):
        pytest.skip()

    ds = sqlite_test_db
    del sqlite_test_db

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    fld = ogr.GeomFieldDefn("geom1", ogr.wkbPoint)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    fld = ogr.GeomFieldDefn("geom2", ogr.wkbLineString)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("foo", "bar")
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt("POINT(0 -1)"))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt("LINESTRING(0 -1,2 3)"))
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if (
        f.GetGeomFieldRef("geom1").ExportToWkt() != "POINT (0 -1)"
        or f.GetGeomFieldRef("geom2").ExportToWkt() != "LINESTRING (0 -1,2 3)"
    ):
        f.DumpReadable()
        pytest.fail()
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt("POINT(0 1)"))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt("LINESTRING(0 1,2 3)"))
    lyr.SetFeature(f)
    f = None
    ds.ExecuteSQL(
        "CREATE VIEW view_test_geom1 AS SELECT OGC_FID AS pk_id, foo, geom1 AS renamed_geom1 FROM test"
    )

    if int(spatialite_version[0 : spatialite_version.find(".")]) >= 4:
        readonly_col = ", read_only"
        readonly_val = ", 1"
    else:
        readonly_col = ""
        readonly_val = ""

    ds.ExecuteSQL(
        (
            "INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column%s) VALUES "
            % readonly_col
        )
        + (
            "('view_test_geom1', 'renamed_geom1', 'pk_id', 'test', 'geom1'%s)"
            % readonly_val
        )
    )
    ds.ExecuteSQL(
        "CREATE VIEW view_test_geom2 AS SELECT OGC_FID AS pk_id, foo, geom2 AS renamed_geom2 FROM test"
    )
    ds.ExecuteSQL(
        (
            "INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column%s) VALUES "
            % readonly_col
        )
        + (
            "('view_test_geom2', 'renamed_geom2', 'pk_id', 'test', 'geom2'%s)"
            % readonly_val
        )
    )

    ds = reopen_sqlite_db(ds)

    lyr = ds.GetLayerByName("test(geom1)")
    view_lyr = ds.GetLayerByName("view_test_geom1")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert view_lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == "geom1"
    assert view_lyr.GetGeometryColumn() == "renamed_geom1"
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    feat = view_lyr.GetFeature(1)
    if feat.GetFieldAsString(0) != "bar":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    view_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != "POINT (0 1)":
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr = ds.GetLayerByName("test(geom2)")
    view_lyr = ds.GetLayerByName("view_test_geom2")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert view_lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == "geom2"
    assert view_lyr.GetGeometryColumn() == "renamed_geom2"
    assert lyr.GetGeomType() == ogr.wkbLineString
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    feat = view_lyr.GetFeature(1)
    if feat.GetFieldAsString(0) != "bar":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    view_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != "LINESTRING (0 1,2 3)":
        feat.DumpReadable()
        pytest.fail()
    feat = None

    sql_lyr = ds.ExecuteSQL("SELECT foo, geom2 FROM test")
    sql_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != "LINESTRING (0 1,2 3)":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    with gdal.quiet_errors():
        lyr = ds.GetLayerByName("invalid_layer_name(geom1)")
    assert lyr is None

    lyr = ds.GetLayerByName("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 2
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "geom1"
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetName() == "geom2"
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeomFieldRef(0).ExportToWkt() != "POINT (0 1)":
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeomFieldRef(1).ExportToWkt() != "LINESTRING (0 1,2 3)":
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr.SetSpatialFilterRect(1, -1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None


###############################################################################
# Test tables with multiple geometry columns (#4768)


def test_ogr_sqlite_31(sqlite_test_db):

    ds = sqlite_test_db
    sqlite_test_db = None

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    fld = ogr.GeomFieldDefn("geom1", ogr.wkbPoint)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    fld = ogr.GeomFieldDefn("geom2", ogr.wkbLineString)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("foo", "bar")
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt("POINT(0 1)"))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt("LINESTRING(0 1,2 3)"))
    lyr.CreateFeature(f)
    f = None

    ds = reopen_sqlite_db(ds)

    lyr = ds.GetLayerByName("test(geom1)")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == "geom1"
    assert lyr.GetGeomType() == ogr.wkbPoint
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != "POINT (0 1)":
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr = ds.GetLayerByName("test(geom2)")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == "geom2"
    assert lyr.GetGeomType() == ogr.wkbLineString
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != "LINESTRING (0 1,2 3)":
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None


###############################################################################
# Test datetime support


def test_ogr_sqlite_32(sqlite_test_db):

    ds = sqlite_test_db
    del sqlite_test_db

    lyr = ds.CreateLayer("test")
    field_defn = ogr.FieldDefn("datetimefield", ogr.OFTDateTime)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("datefield", ogr.OFTDate)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("timefield", ogr.OFTTime)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("datetimefield", "2012/08/23 21:24:00  ")
    feat.SetField("datefield", "2012/08/23  ")
    feat.SetField("timefield", "21:24:00  ")
    lyr.CreateFeature(feat)
    feat = None

    ds = reopen_sqlite_db(ds)
    lyr = ds.GetLayer(0)

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTDateTime
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTDate
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTTime

    feat = lyr.GetNextFeature()
    if (
        feat.GetField("datetimefield") != "2012/08/23 21:24:00"
        or feat.GetField("datefield") != "2012/08/23"
        or feat.GetField("timefield") != "21:24:00"
    ):
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None


###############################################################################
# Test SRID layer creation option


@pytest.mark.parametrize("spatialite", [True, False])
def test_ogr_sqlite_33(sqlite_test_db, spatialite, spatialite_version):

    if spatialite and spatialite_version.startswith("2.3"):
        pytest.skip()

    ds = sqlite_test_db
    del sqlite_test_db

    if not spatialite:
        # To make sure that the entry is added in spatial_ref_sys
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer("test1", srs=srs)

    # Test with existing entry
    lyr = ds.CreateLayer("test2", options=["SRID=4326"])

    # Test with non-existing entry
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test3", options=["SRID=123456"])

    ds = reopen_sqlite_db(ds)
    lyr = ds.GetLayerByName("test2")
    srs = lyr.GetSpatialRef()
    if srs.ExportToWkt().find("4326") == -1:
        pytest.fail("failure")

    # 123456 should be referenced in geometry_columns...
    sql_lyr = ds.ExecuteSQL("SELECT * from geometry_columns WHERE srid=123456")
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert not is_none

    # ... but not in spatial_ref_sys
    sql_lyr = ds.ExecuteSQL("SELECT * from spatial_ref_sys WHERE srid=123456")
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert is_none


###############################################################################
# Test REGEXP support (#4823)


def test_ogr_sqlite_34(sqlite_test_db):

    with gdal.quiet_errors():
        sql_lyr = sqlite_test_db.ExecuteSQL("SELECT 'a' REGEXP 'a'")
    if sql_lyr is None:
        pytest.skip()
    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)
    sqlite_test_db.ReleaseResultSet(sql_lyr)
    assert val == 1
    del sql_lyr

    # Evaluates to FALSE
    with sqlite_test_db.ExecuteSQL("SELECT 'b' REGEXP 'a'") as ok:
        feat = ok.GetNextFeature()
        val = feat.GetField(0)
        assert val == 0

    # NULL left-member
    with sqlite_test_db.ExecuteSQL("SELECT NULL REGEXP 'a'") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)
        assert val is None

    # NULL regexp
    with sqlite_test_db.ExecuteSQL("SELECT 'a' REGEXP NULL") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)
        assert val is None

    # Invalid regexp
    with gdal.quiet_errors():
        sql_lyr = sqlite_test_db.ExecuteSQL("SELECT 'a' REGEXP '['")
    assert sql_lyr is None

    # Adds another pattern
    with sqlite_test_db.ExecuteSQL("SELECT 'b' REGEXP 'b'") as sql_lyr:
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)
    assert val == 1

    # Test cache
    for _ in range(2):
        for i in range(17):
            regexp = chr(ord("a") + i)
            with sqlite_test_db.ExecuteSQL(
                "SELECT '%s' REGEXP '%s'" % (regexp, regexp)
            ) as sql_lyr:
                feat = sql_lyr.GetNextFeature()
                val = feat.GetField(0)
            assert val == 1


###############################################################################
# Test SetAttributeFilter() on SQL result layer


@pytest.mark.parametrize("spatialite", [True, False])
def test_ogr_sqlite_35(sqlite_test_db, spatialite, spatialite_version):

    if spatialite and spatialite_version.find("2.3") >= 0:
        pytest.skip()

    ds = sqlite_test_db
    del sqlite_test_db
    lyr = ds.CreateLayer("test")
    field_defn = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 1)"))
    lyr.CreateFeature(feat)
    feat = None

    for sql in [
        "SELECT * FROM test",
        "SELECT * FROM test GROUP BY foo",
        "SELECT * FROM test ORDER BY foo",
        "SELECT * FROM test LIMIT 1",
        "SELECT * FROM test WHERE 1=1",
        "SELECT * FROM test WHERE 1=1 GROUP BY foo",
        "SELECT * FROM test WHERE 1=1 ORDER BY foo",
        "SELECT * FROM test WHERE 1=1 LIMIT 1",
    ]:
        sql_lyr = ds.ExecuteSQL(sql)

        with ogrtest.attribute_filter(sql_lyr, "foo = 'bar'"):
            sql_lyr.ResetReading()
            feat = sql_lyr.GetNextFeature()
            assert feat is not None
            feat = None

        with ogrtest.attribute_filter(sql_lyr, "foo = 'baz'"):
            sql_lyr.ResetReading()
            feat = sql_lyr.GetNextFeature()
            assert feat is None
            feat = None

        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = None

        with ogrtest.spatial_filter(sql_lyr, 0, 0, 2, 2), ogrtest.attribute_filter(
            sql_lyr, "foo = 'bar'"
        ):
            sql_lyr.ResetReading()
            feat = sql_lyr.GetNextFeature()
            assert feat is not None
            feat = None

        with ogrtest.attribute_filter(sql_lyr, "foo = 'bar'"), ogrtest.spatial_filter(
            sql_lyr, 1.5, 1.5, 2, 2
        ):
            sql_lyr.ResetReading()
            feat = sql_lyr.GetNextFeature()
            assert feat is None
            feat = None

        with ogrtest.spatial_filter(sql_lyr, 0, 0, 2, 2):
            sql_lyr.ResetReading()
            feat = sql_lyr.GetNextFeature()
            assert feat is not None
            feat = None

        ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test FID64 support


def test_ogr_sqlite_36(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer("test")
    field_defn = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    feat.SetFID(1234567890123)
    lyr.CreateFeature(feat)
    feat = None

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)
    lyr = sqlite_test_db.GetLayer(0)
    assert lyr.GetMetadataItem("") is None

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)
    lyr = sqlite_test_db.GetLayer(0)
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1234567890123

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)
    lyr = sqlite_test_db.GetLayer(0)
    assert ogr.OLMD_FID64 in lyr.GetMetadata()


###############################################################################
# Test not nullable fields


def test_ogr_sqlite_37(sqlite_test_db):

    ds = sqlite_test_db
    del sqlite_test_db

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
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

    ds = reopen_sqlite_db(ds, update=1)
    lyr = ds.GetLayerByName("test")
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
    ds.ExecuteSQL("UPDATE test SET field_nullable = '' WHERE field_nullable IS NULL")
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

    ds = reopen_sqlite_db(ds)

    lyr = ds.GetLayerByName("test")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("now_not_nullable"))
        .IsNullable()
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("now_nullable"))
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
    ds = None


###############################################################################
# Test  default values


def test_ogr_sqlite_38(sqlite_test_db):

    ds = sqlite_test_db
    del sqlite_test_db

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

    field_defn = ogr.FieldDefn("field_datetime3", ogr.OFTDateTime)
    field_defn.SetDefault("(strftime('%Y-%m-%dT%H:%M:%fZ','now'))")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_datetime4", ogr.OFTDateTime)
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_date", ogr.OFTDate)
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_time", ogr.OFTTime)
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    ds = reopen_sqlite_db(ds, update=1)
    lyr = ds.GetLayerByName("test")
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
        == "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_datetime4"))
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
    if (
        f.GetField("field_string") != "a'b"
        or f.GetField("field_int") != 123
        or f.GetField("field_real") != 1.23
        or not f.IsFieldNull("field_nodefault")
        or not f.IsFieldSet("field_datetime")
        or f.GetField("field_datetime2") != "2015/06/30 12:34:56"
        or f.GetField("field_datetime4") != "2015/06/30 12:34:56.123"
        or not f.IsFieldSet("field_datetime3")
        or not f.IsFieldSet("field_date")
        or not f.IsFieldSet("field_time")
    ):
        f.DumpReadable()
        pytest.fail()

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

    ds = reopen_sqlite_db(ds, update=1)
    lyr = ds.GetLayerByName("test")
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

    ds = None


###############################################################################
# Test querying a point column in a non-Spatialite DB
# (https://github.com/OSGeo/gdal/issues/8677)
# Also test with a Spatialite DB while we are it...


@pytest.mark.parametrize("spatialite", [True, False])
def test_ogr_spatialite_point_sql_check_srs(sqlite_test_db):

    with sqlite_test_db.ExecuteSQL("SQLITE_HAS_COLUMN_METADATA()") as sql_lyr:
        if sql_lyr.GetNextFeature().GetField(0) != 1:
            pytest.skip("sqlite built without SQLITE_HAS_COLUMN_METADATA")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = sqlite_test_db.CreateLayer("point", srs=srs, geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(feat)
    with sqlite_test_db.ExecuteSQL("SELECT * FROM point") as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 1
        assert sql_lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"


###############################################################################
# Test spatial filters with point extent


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_9(sqlite_test_db):
    lyr = sqlite_test_db.CreateLayer("point", geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(feat)
    lyr.SetSpatialFilterRect(1, 2, 1, 2)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None


###############################################################################
# Test not nullable fields


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_10(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer("test", geom_type=ogr.wkbNone)
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

    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)
    lyr = sqlite_test_db.GetLayerByName("test")
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


###############################################################################
# Test creating a field with the fid name


def test_ogr_sqlite_39(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer(
        "test", geom_type=ogr.wkbNone, options=["FID=myfid"]
    )

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
    f = None


###############################################################################
# Test dataset transactions


@pytest.mark.parametrize("spatialite", [True, False])
def test_ogr_sqlite_40(sqlite_test_db):

    ds = sqlite_test_db
    del sqlite_test_db

    assert ds.TestCapability(ogr.ODsCTransactions) == 1

    ret = ds.StartTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.StartTransaction()
    assert ret != 0

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ret = ds.RollbackTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.RollbackTransaction()
    assert ret != 0

    ds = reopen_sqlite_db(ds, update=1)
    assert ds.GetLayerCount() == 0
    ret = ds.StartTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.StartTransaction()
    assert ret != 0

    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ret = ds.CommitTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.CommitTransaction()
    assert ret != 0

    ds = reopen_sqlite_db(ds, update=1)
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("test")

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    assert lyr.GetFeatureCount() == 1
    ds.RollbackTransaction()
    assert lyr.GetFeatureCount() == 0

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    ds.CommitTransaction()
    # the cursor is still valid after CommitTransaction(), which isn't the case for other backends such as PG !
    f = lyr.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    assert lyr.GetFeatureCount() == 2

    ds.StartTransaction()
    lyr = ds.CreateLayer("test2", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0

    ds.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0

    ds.StartTransaction()
    lyr = ds.CreateLayer("test3", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    ret = lyr.CreateFeature(f)

    # ds.CommitTransaction()
    ds.ReleaseResultSet(ds.ExecuteSQL("SELECT 1"))
    # ds = None
    # ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update = 1)
    # lyr = ds.GetLayerByName('test3')
    # ds.StartTransaction()

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0


###############################################################################
# Test reading dates from Julian day floating point representation


def test_ogr_sqlite_41(tmp_vsimem):

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmp_vsimem / "ogr_sqlite_41.sqlite", options=["METADATA=NO"]
    )

    ds.ExecuteSQL("CREATE TABLE test(a_date DATETIME);")
    ds.ExecuteSQL(
        "INSERT INTO test(a_date) VALUES (strftime('%J', '2015-04-30 12:34:56'))"
    )

    ds = reopen_sqlite_db(ds)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["a_date"] == "2015/04/30 12:34:56"


###############################################################################
# Test ExecuteSQL() heuristics (#6107)


def test_ogr_sqlite_42(sqlite_test_db):

    ds = sqlite_test_db
    del sqlite_test_db

    lyr = ds.CreateLayer("aab")
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 1
    lyr.CreateFeature(f)
    lyr = None

    sql_lyr = ds.ExecuteSQL("SELECT id FROM aab")
    sql_lyr.SetAttributeFilter("id = 1")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT id FROM "aab"')
    sql_lyr.SetAttributeFilter("id = 1")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.CreateLayer('with"quotes')
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["id"] = 1
    lyr.CreateFeature(f)
    lyr = None

    sql_lyr = ds.ExecuteSQL('SELECT id FROM "with""quotes"')
    sql_lyr.SetAttributeFilter("id = 1")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    # Too complex to analyze
    sql_lyr = ds.ExecuteSQL(
        'SELECT id FROM "with""quotes" UNION ALL SELECT id FROM aab'
    )
    sql_lyr.SetAttributeFilter("id = 1")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test file:foo?mode=memory&cache=shared (#6150)


def test_ogr_sqlite_43():

    # Only available since sqlite 3.8.0
    version = get_sqlite_version().split(".")
    if not (
        len(version) >= 3
        and int(version[0]) * 10000 + int(version[1]) * 100 + int(version[2]) >= 30800
    ):
        pytest.skip()

    ds = ogr.Open("file:foo?mode=memory&cache=shared")
    assert ds is not None


###############################################################################
# Test reading/writing StringList, etc..


@pytest.mark.require_driver("CSV")
def test_ogr_sqlite_44(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_sqlite_44.csvt",
        "JsonStringList,JsonIntegerList,JsonInteger64List,JsonRealList,WKT",
    )
    gdal.FileFromMemBuffer(
        tmp_vsimem / "ogr_sqlite_44.csv",
        """stringlist,intlist,int64list,reallist,WKT
"[""a"",null]","[1]","[1234567890123]","[0.125]",
""",
    )

    gdal.VectorTranslate(
        tmp_vsimem / "ogr_sqlite_44.sqlite",
        tmp_vsimem / "ogr_sqlite_44.csv",
        format="SQLite",
    )
    gdal.VectorTranslate(
        tmp_vsimem / "ogr_sqlite_44_out.csv",
        tmp_vsimem / "ogr_sqlite_44.sqlite",
        format="CSV",
        layerCreationOptions=["CREATE_CSVT=YES", "LINEFORMAT=LF"],
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_sqlite_44_out.csv", "rb")
    assert f is not None
    data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert data.startswith(
        'stringlist,intlist,int64list,reallist,wkt\n"[ ""a"", """" ]",[ 1 ],[ 1234567890123 ],[ 0.125'
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_sqlite_44_out.csvt", "rb")
    data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert data.startswith(
        "JSonStringList,JSonIntegerList,JSonInteger64List,JSonRealList"
    )


###############################################################################
# Test WAL and opening in read-only (#6776)


def test_ogr_sqlite_45(tmp_path):

    # Only available since sqlite 3.7.0
    version = get_sqlite_version().split(".")
    if not (
        len(version) >= 3
        and int(version[0]) * 10000 + int(version[1]) * 100 + int(version[2]) >= 30700
    ):
        pytest.skip()

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmp_path / "ogr_sqlite_45.db")
    sql_lyr = ds.ExecuteSQL("PRAGMA journal_mode = WAL")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master")
    ds.ReleaseResultSet(sql_lyr)
    assert os.path.exists(tmp_path / "ogr_sqlite_45.db-wal")
    shutil.copy(tmp_path / "ogr_sqlite_45.db", tmp_path / "ogr_sqlite_45_bis.db")
    shutil.copy(
        tmp_path / "ogr_sqlite_45.db-shm", tmp_path / "ogr_sqlite_45_bis.db-shm"
    )
    shutil.copy(
        tmp_path / "ogr_sqlite_45.db-wal", tmp_path / "ogr_sqlite_45_bis.db-wal"
    )
    ds = None
    assert not os.path.exists(tmp_path / "ogr_sqlite_45.db-wal")

    ds = ogr.Open(tmp_path / "ogr_sqlite_45_bis.db")
    ds = None
    assert not os.path.exists(tmp_path / "ogr_sqlite_45_bis.db-wal")


###############################################################################
# Test creating unsupported geometry types


@pytest.mark.parametrize("spatialite", [True])
def test_ogr_spatialite_11(sqlite_test_db):

    # Will be converted to LineString
    lyr = sqlite_test_db.CreateLayer("test", geom_type=ogr.wkbCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    lyr = sqlite_test_db.CreateLayer("test2", geom_type=ogr.wkbNone)
    with gdal.quiet_errors():
        res = lyr.CreateGeomField(ogr.GeomFieldDefn("foo", ogr.wkbCurvePolygon))
    assert res != 0


###############################################################################
# Test opening a .sql file


def test_ogr_spatialite_12(require_spatialite):
    if (
        gdal.GetDriverByName("SQLite").GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT")
        != "YES"
    ):
        pytest.skip()

    ds = ogr.Open("data/sqlite/poly_spatialite.sqlite.sql")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None


###############################################################################


def test_ogr_sqlite_iterate_and_update(sqlite_test_db):

    lyr = sqlite_test_db.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("strfield"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["strfield"] = "foo"
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["strfield"] = "bar"
    lyr.CreateFeature(f)
    lyr.ResetReading()
    for f in lyr:
        f["strfield"] += "_updated"
        lyr.SetFeature(f)
    lyr.ResetReading()
    for f in lyr:
        assert f["strfield"].endswith("_updated")


###############################################################################
# Test unique constraints on fields


def test_ogr_sqlite_unique(tmp_vsimem):

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmp_vsimem / "ogr_gpkg_unique.db"
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)

    # Default: no unique constraints
    field_defn = ogr.FieldDefn("field_default", ogr.OFTString)
    lyr.CreateField(field_defn)

    # Explicit: no unique constraints
    field_defn = ogr.FieldDefn("field_no_unique", ogr.OFTString)
    field_defn.SetUnique(0)
    lyr.CreateField(field_defn)

    # Explicit: unique constraints
    field_defn = ogr.FieldDefn("field_unique", ogr.OFTString)
    field_defn.SetUnique(1)
    lyr.CreateField(field_defn)

    # Now check for getters
    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()

    # Create another layer from SQL to test quoting of fields
    # and indexes
    # Note: leave create table in a single line because of regex spaces testing
    sql = (
        'CREATE TABLE IF NOT EXISTS "test2" ( "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"field_default" TEXT, "field_no_unique" TEXT DEFAULT \'UNIQUE\',"field_unique" TEXT UNIQUE,`field unique2` TEXT UNIQUE,field_unique3 TEXT UNIQUE, FIELD_UNIQUE_INDEX TEXT, `field unique index2`, "field_unique_index3" TEXT, NOT_UNIQUE TEXT,field4 TEXT,field5 TEXT,field6 TEXT,CONSTRAINT ignored_constraint CHECK (fid >= 0),CONSTRAINT field5_6_uniq UNIQUE (field5, field6), CONSTRAINT field4_uniq UNIQUE (field4));',
        "CREATE UNIQUE INDEX test2_unique_idx ON test2(field_unique_index);",  # field_unique_index in lowercase whereas in uppercase in CREATE TABLE statement
        "CREATE UNIQUE INDEX test2_unique_idx2 ON test2(`field unique index2`);",
        'CREATE UNIQUE INDEX test2_unique_idx3 ON test2("field_unique_index3");',
        'CREATE UNIQUE INDEX test2_unique_idx4 ON test2("NOT_UNIQUE", "fid");',
    )

    for s in sql:
        ds.ExecuteSQL(s)

    ds = None

    # Reload
    ds = ogr.Open(tmp_vsimem / "ogr_gpkg_unique.db")

    lyr = ds.GetLayerByName("test")

    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()

    lyr = ds.GetLayerByName("test2")

    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(3)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(4)
    assert fldDef.IsUnique()

    # Check the last 3 field where the unique constraint is defined
    # from an index
    fldDef = layerDefinition.GetFieldDefn(5)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(6)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(7)
    assert fldDef.IsUnique()

    fldDef = layerDefinition.GetFieldDefn(8)
    assert not fldDef.IsUnique()

    # Constraint given by CONSTRAINT field4_uniq UNIQUE (field4)
    fldDef = layerDefinition.GetFieldDefn(layerDefinition.GetFieldIndex("field4"))
    assert fldDef.IsUnique()

    # Constraint given by CONSTRAINT field5_6_uniq UNIQUE (field5, field6) ==> ignored
    fldDef = layerDefinition.GetFieldDefn(layerDefinition.GetFieldIndex("field5"))
    assert not fldDef.IsUnique()

    ds = None


###############################################################################
# Test PRELUDE_STATEMENTS open option


def test_ogr_sqlite_prelude_statements(require_spatialite):

    ds = gdal.OpenEx(
        "data/sqlite/poly_spatialite.sqlite",
        open_options=[
            "PRELUDE_STATEMENTS=ATTACH DATABASE 'data/sqlite/poly_spatialite.sqlite' AS other"
        ],
    )
    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly JOIN other.poly USING (eas_id)")
    assert sql_lyr.GetFeatureCount() == 10
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test INTEGER_OR_TEXT affinity


def test_ogr_sqlite_integer_or_text():

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(":memory:")
    ds.ExecuteSQL("CREATE TABLE foo(c INTEGER_OR_TEXT)")
    ds.ExecuteSQL("INSERT INTO foo VALUES (5)")
    ds.ExecuteSQL("INSERT INTO foo VALUES ('five')")

    sql_lyr = ds.ExecuteSQL("SELECT typeof(c) FROM foo")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == "integer"
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer("foo")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f["c"] == "5"
    f = lyr.GetNextFeature()
    assert f["c"] == "five"


###############################################################################
# Test better guessing of columns in a view


def test_ogr_sqlite_view_type():

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(":memory:")
    ds.ExecuteSQL("CREATE TABLE t(c INTEGER)")
    ds.ExecuteSQL("CREATE TABLE u(d TEXT)")
    ds.ExecuteSQL("CREATE VIEW v AS SELECT c FROM t UNION ALL SELECT NULL FROM u")

    lyr = ds.GetLayer("v")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger

    ds.ExecuteSQL("INSERT INTO t VALUES(1)")
    f = lyr.GetNextFeature()
    assert f["c"] == 1
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["c"] == 1


###############################################################################
# Test table WITHOUT ROWID


def test_ogr_sqlite_without_rowid(tmp_vsimem):

    tmpfilename = tmp_vsimem / "without_rowid.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmpfilename)
    ds.ExecuteSQL(
        "CREATE TABLE t(key TEXT NOT NULL PRIMARY KEY, value TEXT) WITHOUT ROWID"
    )
    ds = None

    ds = ogr.Open(tmpfilename, update=1)
    lyr = ds.GetLayer("t")
    assert lyr.GetFIDColumn() == ""
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    f = ogr.Feature(lyr.GetLayerDefn())
    f["key"] = "foo"
    f["value"] = "bar"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == -1  # hard to do best

    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f["key"] == "foo"
    assert f["value"] == "bar"
    assert f.GetFID() == 0  # somewhat arbitrary

    f = lyr.GetFeature(0)
    assert f["key"] == "foo"

    ds = None


###############################################################################
# Test table in STRICT mode (sqlite >= 3.37)


def test_ogr_sqlite_strict(tmp_vsimem):

    if "FORCE_SQLITE_STRICT" not in os.environ and "STRICT" not in gdal.GetDriverByName(
        "SQLite"
    ).GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST"):
        pytest.skip("sqlite >= 3.37 required")

    tmpfilename = tmp_vsimem / "strict.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmpfilename)
    lyr = ds.CreateLayer("t", options=["STRICT=YES"])
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64_field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("text_field", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("blob_field", ogr.OFTBinary))
    ds = None

    ds = ogr.Open(tmpfilename, update=1)
    sql_lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE name='t'")
    f = sql_lyr.GetNextFeature()
    sql = f["sql"]
    ds.ReleaseResultSet(sql_lyr)
    assert ") STRICT" in sql

    lyr = ds.GetLayer("t")
    lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("datetime_field", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("date_field", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("time_field", ogr.OFTTime))
    ds = None

    ds = ogr.Open(tmpfilename, update=1)
    lyr = ds.GetLayer("t")
    layer_defn = lyr.GetLayerDefn()
    assert layer_defn.GetFieldCount() == 8
    assert layer_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert layer_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger64
    assert layer_defn.GetFieldDefn(2).GetType() == ogr.OFTString
    assert layer_defn.GetFieldDefn(3).GetType() == ogr.OFTBinary
    assert layer_defn.GetFieldDefn(4).GetType() == ogr.OFTReal
    assert layer_defn.GetFieldDefn(5).GetType() == ogr.OFTDateTime
    assert layer_defn.GetFieldDefn(6).GetType() == ogr.OFTDate
    assert layer_defn.GetFieldDefn(7).GetType() == ogr.OFTTime

    ds = None


###############################################################################
# Test CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE


def test_ogr_sqlite_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE(tmp_vsimem):

    # First check that CPL_TMPDIR is ignored for regular files
    filename = tmp_vsimem / "test_ogr_sqlite_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE.db"
    with gdaltest.config_option("CPL_TMPDIR", "/i_do/not/exist"):
        ds = ogr.GetDriverByName("SQLite").CreateDataSource(filename)
    assert ds is not None
    ds = None
    gdal.Unlink(filename)

    # Now check that CPL_TMPDIR is honored for CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=FORCED
    with gdaltest.config_options(
        {
            "CPL_TMPDIR": f"{tmp_vsimem}/temporary_location",
            "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE": "FORCED",
        }
    ):
        ds = ogr.GetDriverByName("SQLite").CreateDataSource(filename)
    assert ds is not None
    assert gdal.VSIStatL(filename) is None
    assert len(gdal.ReadDir(tmp_vsimem / "temporary_location")) != 0
    ds = None
    assert gdal.VSIStatL(filename) is not None
    assert gdal.ReadDir(tmp_vsimem / "temporary_location") is None


###############################################################################
# Test support for relationships


def test_ogr_sqlite_relationships(tmp_vsimem):

    tmpfilename = tmp_vsimem / "relationships.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmpfilename)
    ds = None

    # test with no relationships
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() is None

    ds.ExecuteSQL(
        "CREATE TABLE test_relation_a(artistid INTEGER PRIMARY KEY, artistname  TEXT)"
    )
    ds.ExecuteSQL(
        "CREATE TABLE test_relation_b(trackid INTEGER, trackname TEXT, trackartist INTEGER, FOREIGN KEY(trackartist) REFERENCES test_relation_a(artistid))"
    )
    ds = None

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() == ["test_relation_a_test_relation_b"]
    assert ds.GetRelationship("xxx") is None
    rel = ds.GetRelationship("test_relation_a_test_relation_b")
    assert rel is not None
    assert rel.GetName() == "test_relation_a_test_relation_b"
    assert rel.GetLeftTableName() == "test_relation_a"
    assert rel.GetRightTableName() == "test_relation_b"
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["artistid"]
    assert rel.GetRightTableFields() == ["trackartist"]
    assert rel.GetRelatedTableType() == "features"

    # test a multi-column join
    ds.ExecuteSQL(
        "CREATE TABLE test_relation_c(trackid INTEGER, trackname TEXT, trackartist INTEGER, trackartistname TEXT, FOREIGN KEY(trackartist, trackartistname) REFERENCES test_relation_a(artistid, artistname))"
    )

    ds = None

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() == [
        "test_relation_a_test_relation_b",
        "test_relation_a_test_relation_c",
    ]
    rel = ds.GetRelationship("test_relation_a_test_relation_c")
    assert rel is not None
    assert rel.GetName() == "test_relation_a_test_relation_c"
    assert rel.GetLeftTableName() == "test_relation_a"
    assert rel.GetRightTableName() == "test_relation_c"
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["artistid", "artistname"]
    assert rel.GetRightTableFields() == ["trackartist", "trackartistname"]
    assert rel.GetRelatedTableType() == "features"

    # a table with two joins
    ds.ExecuteSQL(
        "CREATE TABLE test_relation_d(trackid INTEGER, trackname TEXT, trackartist INTEGER, trackartistname TEXT, FOREIGN KEY(trackname) REFERENCES test_relation_b (trackname), FOREIGN KEY(trackartist, trackartistname) REFERENCES test_relation_a(artistid, artistname))"
    )
    ds = None

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() == [
        "test_relation_a_test_relation_b",
        "test_relation_a_test_relation_c",
        "test_relation_a_test_relation_d",
        "test_relation_b_test_relation_d_2",
    ]
    rel = ds.GetRelationship("test_relation_a_test_relation_d")
    assert rel is not None
    assert rel.GetName() == "test_relation_a_test_relation_d"
    assert rel.GetLeftTableName() == "test_relation_a"
    assert rel.GetRightTableName() == "test_relation_d"
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["artistid", "artistname"]
    assert rel.GetRightTableFields() == ["trackartist", "trackartistname"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("test_relation_b_test_relation_d_2")
    assert rel is not None
    assert rel.GetName() == "test_relation_b_test_relation_d_2"
    assert rel.GetLeftTableName() == "test_relation_b"
    assert rel.GetRightTableName() == "test_relation_d"
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["trackname"]
    assert rel.GetRightTableFields() == ["trackname"]
    assert rel.GetRelatedTableType() == "features"

    # with on delete cascade
    ds.ExecuteSQL(
        "CREATE TABLE test_relation_e(trackid INTEGER, trackname TEXT, trackartist INTEGER, FOREIGN KEY(trackartist) REFERENCES test_relation_a(artistid) ON DELETE CASCADE)"
    )
    ds = None
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() == [
        "test_relation_a_test_relation_b",
        "test_relation_a_test_relation_c",
        "test_relation_a_test_relation_d",
        "test_relation_a_test_relation_e",
        "test_relation_b_test_relation_d_2",
    ]
    rel = ds.GetRelationship("test_relation_a_test_relation_e")
    assert rel is not None
    assert rel.GetName() == "test_relation_a_test_relation_e"
    assert rel.GetLeftTableName() == "test_relation_a"
    assert rel.GetRightTableName() == "test_relation_e"
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["artistid"]
    assert rel.GetRightTableFields() == ["trackartist"]
    assert rel.GetRelatedTableType() == "features"


###############################################################################
# Test support for altering relationships


def test_ogr_sqlite_alter_relations(tmp_vsimem):
    def clone_relationship(relationship):
        res = gdal.Relationship(
            relationship.GetName(),
            relationship.GetLeftTableName(),
            relationship.GetRightTableName(),
            relationship.GetCardinality(),
        )
        res.SetLeftTableFields(relationship.GetLeftTableFields())
        res.SetRightTableFields(relationship.GetRightTableFields())
        res.SetMappingTableName(relationship.GetMappingTableName())
        res.SetLeftMappingTableFields(relationship.GetLeftMappingTableFields())
        res.SetRightMappingTableFields(relationship.GetRightMappingTableFields())
        res.SetType(relationship.GetType())
        res.SetForwardPathLabel(relationship.GetForwardPathLabel())
        res.SetBackwardPathLabel(relationship.GetBackwardPathLabel())
        res.SetRelatedTableType(relationship.GetRelatedTableType())

        return res

    filename = tmp_vsimem / "test_ogr_sqlite_relation_create.db"
    ds = ogr.GetDriverByName("SQLite").CreateDataSource(filename)

    def get_query_row_count(query):
        sql_lyr = ds.ExecuteSQL(query)
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        return res

    relationship = gdal.Relationship(
        "my_relationship", "origin_table", "dest_table", gdal.GRC_ONE_TO_MANY
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    # no tables yet
    assert not ds.AddRelationship(relationship)

    lyr = ds.CreateLayer("origin_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("o_pkey2", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    assert not ds.AddRelationship(relationship)

    lyr = ds.CreateLayer("dest_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("dest_pkey2", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    # add some features to the layers to ensure that they don't get modified/lost during the
    # relationship creation
    layer_origin = ds.GetLayerByName("origin_table")
    layer_dest = ds.GetLayerByName("dest_table")

    feat = ogr.Feature(feature_def=layer_origin.GetLayerDefn())
    feat.SetField("o_pkey", 1)
    assert layer_origin.CreateFeature(feat) == 0

    feat = ogr.Feature(feature_def=layer_origin.GetLayerDefn())
    feat.SetField("o_pkey", 2)
    assert layer_origin.CreateFeature(feat) == 0

    feat = ogr.Feature(feature_def=layer_dest.GetLayerDefn())
    feat.SetField("id", 1)
    feat.SetField("dest_pkey", 1)
    assert layer_dest.CreateFeature(feat) == 0

    feat = ogr.Feature(feature_def=layer_dest.GetLayerDefn())
    feat.SetField("id", 2)
    feat.SetField("dest_pkey", 1)
    assert layer_dest.CreateFeature(feat) == 0

    feat = ogr.Feature(feature_def=layer_dest.GetLayerDefn())
    feat.SetField("id", 3)
    feat.SetField("dest_pkey", 2)
    assert layer_dest.CreateFeature(feat) == 0

    # left table fields must be set, only one field
    relationship.SetLeftTableFields([])
    assert not ds.AddRelationship(relationship)
    relationship.SetLeftTableFields(["o_pkey", "another"])
    assert not ds.AddRelationship(relationship)
    # left table field must exist
    relationship.SetLeftTableFields(["o_pkey_nope"])
    assert not ds.AddRelationship(relationship)

    relationship.SetLeftTableFields(["o_pkey"])

    # right table fields must be set, only one field
    relationship.SetRightTableFields([])
    assert not ds.AddRelationship(relationship)
    relationship.SetRightTableFields(["dest_pkey", "another"])
    assert not ds.AddRelationship(relationship)
    # right table field must exist
    relationship.SetRightTableFields(["dest_pkey_nope"])
    assert not ds.AddRelationship(relationship)

    relationship.SetRightTableFields(["dest_pkey"])

    assert not ds.AddRelationship(relationship)

    # a unique index is required on base table key

    ds.ExecuteSQL("CREATE UNIQUE INDEX origin_table_o_pkey_idx ON origin_table(o_pkey)")

    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {"origin_table_dest_table"}
    retrieved_rel = ds.GetRelationship("origin_table_dest_table")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "features"
    assert retrieved_rel.GetMappingTableName() == ""
    assert retrieved_rel.GetLeftMappingTableFields() is None
    assert retrieved_rel.GetRightMappingTableFields() is None

    # try again, should fail because relationship already exists
    assert not ds.AddRelationship(relationship)

    # reopen and ensure that existing features remain unmodified in layers
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    layer_origin = ds.GetLayerByName("origin_table")
    layer_dest = ds.GetLayerByName("dest_table")

    feat_read = layer_origin.GetNextFeature()
    assert feat_read.GetField("o_pkey") == 1
    feat_read = layer_origin.GetNextFeature()
    assert feat_read.GetField("o_pkey") == 2
    feat_read = layer_origin.GetNextFeature()
    assert feat_read is None
    layer_origin.ResetReading()

    feat_read = layer_dest.GetNextFeature()
    assert feat_read.GetField("id") == 1
    assert feat_read.GetField("dest_pkey") == 1
    feat_read = layer_dest.GetNextFeature()
    assert feat_read.GetField("id") == 2
    assert feat_read.GetField("dest_pkey") == 1
    feat_read = layer_dest.GetNextFeature()
    assert feat_read.GetField("id") == 3
    assert feat_read.GetField("dest_pkey") == 2
    feat_read = layer_dest.GetNextFeature()
    assert feat_read is None
    layer_dest.ResetReading()

    lyr = ds.CreateLayer("origin_table2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    # only one-to-many relationships are supported
    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_ONE_TO_ONE
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("features")
    assert not ds.AddRelationship(relationship)

    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("features")
    assert not ds.AddRelationship(relationship)

    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_MANY_TO_ONE
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("features")
    assert not ds.AddRelationship(relationship)

    # add second relationship of composition type
    lyr = ds.CreateLayer("origin_table3", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table3", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    relationship = gdal.Relationship(
        "my_relationship", "origin_table3", "dest_table3", gdal.GRC_ONE_TO_MANY
    )
    relationship.SetType(gdal.GRT_COMPOSITE)

    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    ds.ExecuteSQL(
        "CREATE UNIQUE INDEX origin_table3_o_pkey_idx ON origin_table3(o_pkey)"
    )

    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table",
        "origin_table3_dest_table3",
    }
    retrieved_rel = ds.GetRelationship("origin_table3_dest_table3")
    assert retrieved_rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_COMPOSITE
    assert retrieved_rel.GetLeftTableName() == "origin_table3"
    assert retrieved_rel.GetRightTableName() == "dest_table3"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "features"


###############################################################################
# Test support for creating "foo(bar)" layer names


def test_ogr_sqlite_create_layer_names_with_parenthesis(tmp_vsimem):

    tmpfilename = tmp_vsimem / "test_ogr_sqlite_create_layer_names_with_parenthesis.db"

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("foo(bar)")
    gdal.ErrorReset()
    out_ds = gdal.VectorTranslate(tmpfilename, src_ds, format="SQLite")
    assert out_ds is not None
    assert gdal.GetLastErrorMsg() == ""
    out_ds = None
    ds = ogr.Open(tmpfilename)
    gdal.ErrorReset()
    assert ds.GetLayerByName("foo(bar)") is not None
    assert gdal.GetLastErrorMsg() == ""
    assert ds.GetLayerByName("bar(baz)") is None
    assert gdal.GetLastErrorMsg() == ""
    ds = None


###############################################################################
# Test ogr_layer_Extent()


def test_ogr_sqlite_ogr_layer_Extent(tmp_vsimem):

    tmpfilename = tmp_vsimem / "test_ogr_sqlite_ogr_layer_Extent.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(tmpfilename)
    lyr = ds.CreateLayer("my_layer", geom_type=ogr.wkbLineString)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING (0 1,2 3)"))
    lyr.CreateFeature(feat)
    feat = None

    # Test with invalid parameter
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent(12)")
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    assert geom is None

    # Test on non existing layer
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent('foo')")
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    assert geom is None

    # Test ogr_layer_Extent()
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent('my_layer')")
    feat = sql_lyr.GetNextFeature()
    geom_wkt = feat.GetGeometryRef().ExportToWkt()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert geom_wkt == "POLYGON ((0 1,2 1,2 3,0 3,0 1))"


@pytest.mark.usefixtures("tpoly")
def test_ogr_sqlite_delete(sqlite_test_db):

    lyr = sqlite_test_db.GetLayer("tpoly")
    assert lyr is not None

    sqlite_test_db.ExecuteSQL("DELLAYER:tpoly")
    sqlite_test_db = reopen_sqlite_db(sqlite_test_db)

    lyr = sqlite_test_db.GetLayer("tpoly")
    assert lyr is None


###############################################################################
# Test a SQL request with the geometry in the first row being null


def test_ogr_sql_sql_first_geom_null(require_spatialite):

    ds = ogr.Open("data/sqlite/first_geometry_null.db")
    with ds.ExecuteSQL("SELECT ST_Buffer(geom,0.1) FROM test") as sql_lyr:
        assert sql_lyr.GetGeometryColumn() == "ST_Buffer(geom,0.1)"
    with ds.ExecuteSQL("SELECT ST_Buffer(geom,0.1), * FROM test") as sql_lyr:
        assert sql_lyr.GetGeometryColumn() == "ST_Buffer(geom,0.1)"


###############################################################################
# Test our overloaded LIKE operator


@gdaltest.enable_exceptions()
def test_ogr_sqlite_like_utf8():

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(":memory:")
    lyr = ds.CreateLayer("test")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'e' LIKE 'E'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'e' LIKE 'i'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'é' LIKE 'É'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL(
        "SELECT * FROM test WHERE 'éx' LIKE 'Éxx' ESCAPE 'x'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    with ds.ExecuteSQL("SELECT * FROM test WHERE NULL LIKE 'É'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'é' LIKE NULL") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'é' LIKE 'É' ESCAPE NULL") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    with ds.ExecuteSQL(
        "SELECT * FROM test WHERE 'é' LIKE 'É' ESCAPE 'should be single char'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    ds.ExecuteSQL("PRAGMA case_sensitive_like = 1")

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'e' LIKE 'E'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0

    ds.ExecuteSQL("PRAGMA case_sensitive_like = 0")

    with ds.ExecuteSQL("SELECT * FROM test WHERE 'e' LIKE 'E'") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1


###############################################################################
# Test ST_Area(geom, use_ellipsoid=True)


def test_ogr_sql_ST_Area_on_ellipsoid(tmp_vsimem, require_spatialite):

    tmpfilename = tmp_vsimem / "test_ogr_sql_ST_Area_on_ellipsoid.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmpfilename, options=["SPATIALITE=YES"]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258)
    lyr = ds.CreateLayer("my_layer", srs=srs)
    geom_colname = lyr.GetGeometryColumn()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POLYGON((2 49,3 49,3 48,2 49))")
    )
    lyr.CreateFeature(feat)
    feat = None

    with ds.ExecuteSQL(f"SELECT ST_Area({geom_colname}, 1) FROM my_layer") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] == pytest.approx(4068384291.907715)

    with ds.ExecuteSQL(
        f"SELECT ST_Area(SetSRID({geom_colname}, -1), 1) FROM my_layer"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] == pytest.approx(4068384291.8911743)

    with gdal.quiet_errors():
        with ds.ExecuteSQL(
            f"SELECT ST_Area({geom_colname}, 0) FROM my_layer"
        ) as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f[0] == pytest.approx(4068384291.907715)

    with ds.ExecuteSQL("SELECT ST_Area(null, 1) FROM my_layer") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] is None


###############################################################################
# Test ST_Length(geom, use_ellipsoid=True)


def test_ogr_sql_ST_Length_on_ellipsoid(tmp_vsimem, require_spatialite):

    tmpfilename = tmp_vsimem / "test_ogr_sql_ST_Length_on_ellipsoid.db"

    ds = ogr.GetDriverByName("SQLite").CreateDataSource(
        tmpfilename, options=["SPATIALITE=YES"]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258)
    lyr = ds.CreateLayer("my_layer", srs=srs)
    geom_colname = lyr.GetGeometryColumn()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("LINESTRING(2 49,3 49,3 48,2 49)")
    )
    lyr.CreateFeature(feat)
    feat = None

    with ds.ExecuteSQL(f"SELECT ST_Length({geom_colname}, 1) FROM my_layer") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] == pytest.approx(317885.7863996293)

    with gdal.quiet_errors():
        with ds.ExecuteSQL(
            f"SELECT ST_Length({geom_colname}, 0) FROM my_layer"
        ) as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f[0] == pytest.approx(317885.7863996293)

    with ds.ExecuteSQL("SELECT ST_Length(null, 1) FROM my_layer") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] is None

    with gdal.quiet_errors():
        with ds.ExecuteSQL("SELECT ST_Length(X'FF', 1) FROM my_layer") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f[0] is None


def test_ogr_sqlite_stddev():
    """Test STDDEV_POP() and STDDEV_SAMP"""

    ds = ogr.Open(":memory:", update=1)
    ds.ExecuteSQL("CREATE TABLE test(v REAL)")
    ds.ExecuteSQL("INSERT INTO test VALUES (4),(NULL),('invalid'),(5)")
    with ds.ExecuteSQL("SELECT STDDEV_POP(v), STDDEV_SAMP(v) FROM test") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == pytest.approx(0.5, rel=1e-15)
        assert f.GetField(1) == pytest.approx(0.5**0.5, rel=1e-15)


@pytest.mark.parametrize(
    "input_values,expected_res",
    [
        ([], None),
        ([1], 1),
        ([2.5, None, 1], 1.75),
        ([3, 2.2, 1], 2.2),
        ([1, "invalid"], None),
    ],
)
def test_ogr_sqlite_median(input_values, expected_res):
    """Test MEDIAN"""

    ds = ogr.Open(":memory:", update=1)
    ds.ExecuteSQL("CREATE TABLE test(v)")
    for v in input_values:
        ds.ExecuteSQL(
            "INSERT INTO test VALUES (%s)"
            % (
                "NULL"
                if v is None
                else ("'" + v + "'") if isinstance(v, str) else str(v)
            )
        )
    if expected_res is None and input_values:
        with pytest.raises(Exception), gdaltest.error_handler():
            with ds.ExecuteSQL("SELECT MEDIAN(v) FROM test"):
                pass
    else:
        with ds.ExecuteSQL("SELECT MEDIAN(v) FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == pytest.approx(expected_res)
        with ds.ExecuteSQL("SELECT PERCENTILE(v, 50) FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == pytest.approx(expected_res)
        with ds.ExecuteSQL("SELECT PERCENTILE_CONT(v, 0.5) FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == pytest.approx(expected_res)


def test_ogr_sqlite_percentile():
    """Test PERCENTILE"""

    ds = ogr.Open(":memory:", update=1)
    ds.ExecuteSQL("CREATE TABLE test(v)")
    ds.ExecuteSQL("INSERT INTO test VALUES (5),(6),(4),(7),(3),(8),(2),(9),(1),(10)")

    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE(v, 'invalid') FROM test"):
            pass
    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE(v, -0.1) FROM test"):
            pass
    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE(v, 100.1) FROM test"):
            pass
    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE(v, v) FROM test"):
            pass


def test_ogr_sqlite_percentile_cont():
    """Test PERCENTILE_CONT"""

    ds = ogr.Open(":memory:", update=1)
    ds.ExecuteSQL("CREATE TABLE test(v)")
    ds.ExecuteSQL("INSERT INTO test VALUES (5),(6),(4),(7),(3),(8),(2),(9),(1),(10)")

    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE_CONT(v, 'invalid') FROM test"):
            pass
    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE_CONT(v, -0.1) FROM test"):
            pass
    with pytest.raises(Exception), gdaltest.error_handler():
        with ds.ExecuteSQL("SELECT PERCENTILE_CONT(v, 1.1) FROM test"):
            pass


@pytest.mark.parametrize(
    "input_values,expected_res",
    [
        ([], None),
        ([1, 2, None, 3, 2], 2),
        (["foo", "bar", "baz", "bar"], "bar"),
        ([1, "foo", 2, "foo", "bar"], "foo"),
        ([1, "foo", 2, "foo", 1], "foo"),
    ],
)
def test_ogr_sqlite_mode(input_values, expected_res):
    """Test MODE"""

    ds = ogr.Open(":memory:", update=1)
    ds.ExecuteSQL("CREATE TABLE test(v)")
    for v in input_values:
        ds.ExecuteSQL(
            "INSERT INTO test VALUES (%s)"
            % (
                "NULL"
                if v is None
                else ("'" + v + "'") if isinstance(v, str) else str(v)
            )
        )
    if expected_res is None and input_values:
        with pytest.raises(Exception), gdaltest.error_handler():
            with ds.ExecuteSQL("SELECT MODE(v) FROM test"):
                pass
    else:
        with ds.ExecuteSQL("SELECT MODE(v) FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == expected_res


def test_ogr_sqlite_run_deferred_actions_before_start_transaction():

    ds = ogr.Open(":memory:", update=1)
    lyr = ds.CreateLayer("test")
    ds.StartTransaction()
    ds.ExecuteSQL("INSERT INTO test VALUES (1, NULL)")
    ds.RollbackTransaction()
    ds.StartTransaction()
    ds.ExecuteSQL("INSERT INTO test VALUES (1, NULL)")
    ds.CommitTransaction()
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1


######################################################################
# Test schema override open option with SQLite driver
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
def test_ogr_sqlite_schema_override(
    tmp_path, open_options, expected_field_types, expected_field_names, expected_warning
):

    # Create SQLite database
    sqlite_db = tmp_path / "test_ogr_sqlite_schema_override.db"
    ds = ogr.GetDriverByName("SQLite").CreateDataSource(str(sqlite_db))
    lyr = ds.CreateLayer("test_point")
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("bool", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int_str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("real_str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("json_str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("uuid_str", ogr.OFTString))

    # Insert some data
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("str", "1")
    feat.SetField("int", 2)
    feat.SetField("real", 3.4)
    feat.SetField("bool", 1)
    feat.SetField("int_str", "2")
    feat.SetField("real_str", "3.4")
    feat.SetField("json_str", '{"key": "foo"}')
    feat.SetField("uuid_str", "123e4567-e89b-12d3-a456-426614174000")
    lyr.CreateFeature(feat)
    feat = None

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
                    sqlite_db,
                    gdal.OF_VECTOR | gdal.OF_READONLY,
                    open_options=open_options,
                    allowed_drivers=["SQLite"],
                )
                assert (
                    gdal.GetLastErrorMsg().find(expected_warning) != -1
                ), f"Warning {expected_warning} not found, got {gdal.GetLastErrorMsg()} instead"
                assert ds is None
        else:

            ds = gdal.OpenEx(
                sqlite_db,
                gdal.OF_VECTOR | gdal.OF_READONLY,
                open_options=open_options,
                allowed_drivers=["SQLite"],
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


######################################################################
# Test field operations rolling back changes.
#


@pytest.mark.parametrize("start_transaction", [False, True])
def test_ogr_sqlite_field_operations_rollback(tmp_vsimem, start_transaction):

    filename = str(tmp_vsimem / "test.db")
    with ogr.GetDriverByName("SQLite").CreateDataSource(filename) as ds:
        ogrtest.check_transaction_rollback(ds, start_transaction, test_geometry=False)


@pytest.mark.parametrize("start_transaction", [False, True])
def test_ogr_sqlite_field_operations_savepoint_rollback(tmp_vsimem, start_transaction):

    filename = str(tmp_vsimem / "test_savepoint.db")
    with ogr.GetDriverByName("SQLite").CreateDataSource(filename) as ds:
        ogrtest.check_transaction_rollback_with_savepoint(
            ds, start_transaction, test_geometry=False
        )


@pytest.mark.parametrize("auto_begin_transaction", [False, True])
@pytest.mark.parametrize("start_transaction", [False, True])
@pytest.mark.parametrize(
    "release_to,rollback_to,expected",
    (
        ([1], [], ["fld3"]),
        ([2], [], ["fld3"]),
        ([3], [], ["fld3"]),
        ([4], [], ["fld3"]),
        ([], [1], ["fld1", "fld2", "fld3", "fld4", "fld5"]),
        ([], [2], ["fld1", "fld3", "fld4", "fld5"]),
        ([], [3], ["fld1", "fld3", "fld5"]),
        ([], [4], ["fld3", "fld5"]),
    ),
)
def test_ogr_sqlite_field_operations_savepoint_release(
    tmp_vsimem,
    auto_begin_transaction,
    start_transaction,
    release_to,
    rollback_to,
    expected,
):

    filename = str(tmp_vsimem / "test_savepoint_release.db")
    ogrtest.check_transaction_savepoint_release(
        filename,
        "SQLite",
        auto_begin_transaction,
        start_transaction,
        release_to,
        rollback_to,
        expected,
        test_geometry=False,
    )


###############################################################################
# Test ST_Hilbert()


@gdaltest.enable_exceptions()
def test_ogr_sqlite_ST_Hilbert(tmp_vsimem, require_spatialite):

    with gdal.GetDriverByName("SQLite").CreateVector(
        tmp_vsimem / "tmp.db", options=["SPATIALITE=YES"]
    ) as ds:
        lyr = ds.CreateLayer("test")
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((10 20,10 21,11 21,10 20))"))
        lyr.CreateFeature(f)

    with ogr.Open(tmp_vsimem / "tmp.db") as ds:

        # Test ST_Hilbert(x, y, minx, miny, maxx, maxy)

        with ds.ExecuteSQL("SELECT ST_Hilbert(10, 20, 10, 20, 30, 40)") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 0

        with ds.ExecuteSQL("SELECT ST_Hilbert(11, 22, 10, 20, 30, 40)") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 53687090

        with ds.ExecuteSQL("SELECT ST_Hilbert(30, 40, 10, 20, 30, 40)") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 2863311528

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(10-1e-3, 20, 10, 20, 30, 40)"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(10, 20-1e-3, 10, 20, 30, 40)"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(30+1e-3, 40, 10, 20, 30, 40)"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(30, 40+1e-3, 10, 20, 30, 40)"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        # Test ST_Hilbert(x, y, layer_name)

        with ds.ExecuteSQL("SELECT ST_Hilbert(10, 20, 'test')") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 0

        with ds.ExecuteSQL("SELECT ST_Hilbert(11, 21, 'test')") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 2863311528

        with pytest.raises(Exception, match="unknown layer 'non_existing'"):
            with ds.ExecuteSQL("SELECT ST_Hilbert(10, 20, 'non_existing')") as sql_lyr:
                pass

        with pytest.raises(Exception, match="Invalid argument type"):
            with ds.ExecuteSQL("SELECT ST_Hilbert(10, 20, NULL)") as sql_lyr:
                pass

        with gdal.quiet_errors():
            with ds.ExecuteSQL("SELECT ST_Hilbert(10-1e-3, 20, 'test')") as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL("SELECT ST_Hilbert(11+1e-3, 20, 'test')") as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL("SELECT ST_Hilbert(10, 20-1e-3, 'test')") as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL("SELECT ST_Hilbert(10, 21+1e-3, 'test')") as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        # Test ST_Hilbert(geom, minx, miny, maxx, maxy)

        with ds.ExecuteSQL(
            "SELECT ST_Hilbert(geometry, 10, 20, 11, 21) FROM test"
        ) as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 715827882

        with ds.ExecuteSQL(
            "SELECT ST_Hilbert(NULL, 10, 20, 11, 21) FROM test"
        ) as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, 10.5+1e-3, 20, 11, 21) FROM test"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, 10, 20.5+1e-3, 11, 21) FROM test"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, 10, 20, 10.5-1e-3, 21) FROM test"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        with gdal.quiet_errors():
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, 10, 20, 11, 20.5-1e-3) FROM test"
            ) as sql_lyr:
                f = sql_lyr.GetNextFeature()
                assert f.GetField(0) is None

        # Test ST_Hilbert(geom, layer_name)

        with ds.ExecuteSQL("SELECT ST_Hilbert(geometry, 'test') FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) == 715827882

        with ds.ExecuteSQL("SELECT ST_Hilbert(NULL, 'test') FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0) is None

        with pytest.raises(Exception, match="unknown layer 'non_existing'"):
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, 'non_existing') FROM test"
            ) as sql_lyr:
                pass

        with pytest.raises(Exception, match="Invalid argument type"):
            with ds.ExecuteSQL(
                "SELECT ST_Hilbert(geometry, NULL) FROM test"
            ) as sql_lyr:
                pass
