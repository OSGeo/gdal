#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoPackage driver functionality.
# Author:   Paul Ramsey <pramsey@boundlessgeom.com>
#
###############################################################################
# Copyright (c) 2004, Paul Ramsey <pramsey@boundlessgeom.com>
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

import math
import os
import struct
import sys
import threading
import time

import gdaltest
import ogrtest
import pytest
from test_py_scripts import samples_path

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("GPKG")

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    gdaltest.gpkg_dr = ogr.GetDriverByName("GPKG")

    try:
        os.remove("tmp/gpkg_test.gpkg")
    except OSError:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    with gdal.config_option("OGR_SQLITE_SYNCHRONOUS", "OFF"):

        yield

    if gdal.ReadDir("/vsimem") is not None:
        print(gdal.ReadDir("/vsimem"))
        for f in gdal.ReadDir("/vsimem"):
            gdal.Unlink("/vsimem/" + f)

    try:
        os.remove("tmp/gpkg_test.gpkg")
    except OSError:
        pass


@pytest.fixture()
def gpkg_dsn(tmp_path):
    return tmp_path / "test.gpkg"


@pytest.fixture()
def gpkg_ds(gpkg_dsn):
    return ogr.GetDriverByName("GPKG").CreateDataSource(gpkg_dsn)


@pytest.fixture()
def tpoly(gpkg_ds, poly_feat):

    lyr = gpkg_ds.CreateLayer("tpoly")

    ogrtest.quick_create_layer_def(
        lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
            ("REALLIST", ogr.OFTRealList),
        ],
    )

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    for feat in poly_feat:
        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)


@pytest.fixture()
def a_layer(gpkg_ds):
    gpkg_ds.CreateLayer("a_layer", options=["SPATIAL_INDEX=NO"])


@pytest.fixture()
def tbl_linestring(gpkg_ds):

    srs = osr.SpatialReference()
    # Test a non-default SRS
    srs.ImportFromEPSG(32631)

    lyr = gpkg_ds.CreateLayer("tbl_linestring", geom_type=ogr.wkbLineString, srs=srs)
    assert lyr is not None

    lyr.StartTransaction()
    lyr.CreateField(ogr.FieldDefn("fld_integer", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("fld_string", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("fld_real", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("fld_date", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("fld_datetime", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("fld_binary", ogr.OFTBinary))
    fld_defn = ogr.FieldDefn("fld_boolean", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("fld_smallint", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("fld_float", ogr.OFTReal)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)
    lyr.CreateField(ogr.FieldDefn("fld_integer64", ogr.OFTInteger64))

    geom = ogr.CreateGeometryFromWkt("LINESTRING(5 5,10 5,10 10,5 10)")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetFID(-1)
        feat.SetField("fld_integer", 10 + i)
        feat.SetField("fld_real", 3.14159 / (i + 1))
        feat.SetField("fld_string", "test string %d test" % i)
        feat.SetField("fld_date", "2014/05/17 ")
        feat.SetField("fld_datetime", "2014/12/31  23:59:59.999Z")
        feat.SetField("fld_binary", b"\xFF\xFE")
        feat.SetField("fld_boolean", 1)
        feat.SetField("fld_smallint", -32768)
        feat.SetField("fld_float", 1.23)
        feat.SetField("fld_integer64", 1000000000000 + i)

        assert lyr.CreateFeature(feat) == 0, "cannot create feature %d" % i
    lyr.CommitTransaction()


@pytest.fixture()
def point_no_spi_but_with_dashes(gpkg_ds):

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)

    lyr = gpkg_ds.CreateLayer(
        "point_no_spi-but-with-dashes",
        geom_type=ogr.wkbPoint,
        options=["SPATIAL_INDEX=NO"],
        srs=sr,
    )

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1000 30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-1000 30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1000 -30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-1000 -30000000)"))
    lyr.CreateFeature(feat)
    # Test null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    # Test empty geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(feat)


@pytest.fixture()
def point_with_spi_and_dashes(gpkg_ds):

    lyr = gpkg_ds.CreateLayer("point-with-spi-and-dashes", geom_type=ogr.wkbPoint)
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1000 30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-1000 30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1000 -30000000)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-1000 -30000000)"))
    lyr.CreateFeature(feat)
    # Test null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    # Test empty geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(feat)


###############################################################################


def get_sqlite_version():
    with gdaltest.disable_exceptions():
        ds = ogr.Open(":memory:")
    if ds is None:
        return (0, 0, 0)
    sql_lyr = ds.ExecuteSQL("SELECT sqlite_version()")
    f = sql_lyr.GetNextFeature()
    version = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    return tuple([int(x) for x in version.split(".")[0:3]])


###############################################################################
# Validate a geopackage


def has_validate():
    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_gpkg

        validate_gpkg.check
    except ImportError:
        print("Cannot import validate_gpkg")
        return False
    return True


def _validate_check(filename):
    if not has_validate():
        return
    import validate_gpkg

    validate_gpkg.check(filename, extra_checks=True, warning_as_error=True)


def validate(gpkg, quiet=False, tmpdir=None):

    try:
        filename = gpkg.GetDescription()
    except AttributeError:
        filename = str(gpkg)

    my_filename = filename
    if my_filename.startswith("/vsimem/"):
        assert tmpdir is not None, "need tmpdir to validate from /vsimem"

        my_filename = os.path.join(tmpdir, "validate.gpkg")
        f = gdal.VSIFOpenL(filename, "rb")
        if f is None:
            print("Cannot open %s" % filename)
            return False
        content = gdal.VSIFReadL(1, 10000000, f)
        gdal.VSIFCloseL(f)
        open(my_filename, "wb").write(content)
    try:
        _validate_check(my_filename)
    except Exception:
        if not quiet:
            raise
        return False
    finally:
        if my_filename != filename:
            os.unlink(my_filename)
    return True


###############################################################################
# Create a fresh database.


def test_ogr_gpkg_1(gpkg_ds):

    assert validate(gpkg_ds), "validation failed"


###############################################################################
# Re-open database to test validity


def test_ogr_gpkg_2(gpkg_ds):

    # Should default to GPKG 1.2
    with gpkg_ds.ExecuteSQL("PRAGMA application_id") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["application_id"] == 1196444487

    with gpkg_ds.ExecuteSQL("PRAGMA user_version") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["user_version"] == 10200


###############################################################################
# Create a layer


def test_ogr_gpkg_2bis(gpkg_ds):

    # Test creating a layer with an existing name
    lyr = gpkg_ds.CreateLayer("a_layer", options=["SPATIAL_INDEX=NO"])
    assert lyr is not None
    with gdal.quiet_errors():
        lyr = gpkg_ds.CreateLayer("a_layer", options=["SPATIAL_INDEX=NO"])
    assert lyr is None, "layer creation should have failed"


def test_ogr_gpkg_3(gpkg_ds):

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG(4326)
    lyr = gpkg_ds.CreateLayer(
        "first_layer",
        geom_type=ogr.wkbPoint,
        srs=srs4326,
        options=["GEOMETRY_NAME=gpkg_geometry", "SPATIAL_INDEX=NO"],
    )
    assert lyr is not None

    lyr = gpkg_ds.CreateLayer("a_layer", options=["SPATIAL_INDEX=NO"])

    ###############################################################################
    # Close and re-open to test the layer registration

    assert validate(gpkg_ds), "validation failed"

    gpkg_ds = gdaltest.reopen(gpkg_ds)

    assert gpkg_ds is not None

    assert gpkg_ds.GetLayerCount() == 2, "unexpected number of layers"

    lyr0 = gpkg_ds.GetLayer(0)

    assert lyr0.GetFIDColumn() == "fid", "unexpected FID name for layer 0"
    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)

    lyr0 = gpkg_ds.GetLayer(0)

    assert lyr0.GetName() == "first_layer", "unexpected layer name for layer 0"

    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)

    lyr0 = gpkg_ds.GetLayer(0)
    lyr1 = gpkg_ds.GetLayer(1)

    assert (
        lyr0.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "gpkg_geometry"
    ), "unexpected geometry field name for layer 0"

    assert lyr1.GetName() == "a_layer", "unexpected layer name for layer 1"

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    gpkg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Delete a layer


@pytest.mark.usefixtures("a_layer", "tpoly")
def test_ogr_gpkg_5(gpkg_ds):

    assert gpkg_ds.GetLayerCount() == 2, "unexpected number of layers"

    with gdal.quiet_errors():
        ret = gpkg_ds.DeleteLayer(-1)
    assert ret != 0, "expected error"

    with gdal.quiet_errors():
        ret = gpkg_ds.DeleteLayer(gpkg_ds.GetLayerCount())
    assert ret != 0, "expected error"

    assert gpkg_ds.DeleteLayer(1) == 0, "got error code from DeleteLayer(1)"

    assert gpkg_ds.DeleteLayer(0) == 0, "got error code from DeleteLayer(0)"

    assert gpkg_ds.GetLayerCount() == 0, "unexpected number of layers (not 0)"


###############################################################################
# Add fields


def test_ogr_gpkg_6(gpkg_ds):

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG(4326)
    lyr = gpkg_ds.CreateLayer("field_test_layer", geom_type=ogr.wkbPoint, srs=srs4326)
    assert lyr is not None

    field_defn = ogr.FieldDefn("dummy", ogr.OFTString)
    lyr.CreateField(field_defn)

    assert (
        lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    ), "wrong field type"

    gpkg_ds = gdaltest.reopen(gpkg_ds)

    assert validate(gpkg_ds), "validation failed"

    with gdal.quiet_errors():
        gpkg_ds = gdaltest.reopen(gpkg_ds)

    assert gpkg_ds is not None

    assert gpkg_ds.GetLayerCount() == 1

    lyr = gpkg_ds.GetLayer(0)
    assert lyr.GetName() == "field_test_layer"

    field_defn_out = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn_out.GetType() == ogr.OFTString, "wrong field type after reopen"

    assert field_defn_out.GetName() == "dummy", "wrong field name after reopen"


###############################################################################
# Add a feature / read a feature / set a feature / upsert a feature / delete a feature


def test_ogr_gpkg_7(gpkg_ds):

    lyr = gpkg_ds.CreateLayer("field_test_layer", geom_type=ogr.wkbPoint)
    field_defn = ogr.FieldDefn("dummy", ogr.OFTString)
    lyr.CreateField(field_defn)

    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)

    lyr = gpkg_ds.GetLayerByName("field_test_layer")
    geom = ogr.CreateGeometryFromWkt("POINT(10 10)")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField("dummy", "a dummy value")

    assert (
        lyr.TestCapability(ogr.OLCSequentialWrite) == 1
    ), "lyr.TestCapability(ogr.OLCSequentialWrite) != 1"

    assert lyr.CreateFeature(feat) == 0, "cannot create feature"

    # Read back what we just inserted
    lyr.ResetReading()
    feat_read = lyr.GetNextFeature()
    assert feat_read.GetField("dummy") == "a dummy value", "output does not match input"

    # Only inserted one thing, so second feature should return NULL
    feat_read = lyr.GetNextFeature()
    assert feat_read is None, "last call should return NULL"

    # Check that calling again GetNextFeature() does not reset the iterator
    feat_read = lyr.GetNextFeature()
    assert feat_read is None, "last call should still return NULL"

    # Add another feature
    geom = ogr.CreateGeometryFromWkt("POINT(100 100)")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField("dummy", "who you calling a dummy?")
    assert lyr.CreateFeature(feat) == 0, "cannot create feature"

    assert (
        lyr.TestCapability(ogr.OLCRandomRead) == 1
    ), "lyr.TestCapability(ogr.OLCRandomRead) != 1"

    # Random read a feature
    feat_read_random = lyr.GetFeature(feat.GetFID())
    assert (
        feat_read_random.GetField("dummy") == "who you calling a dummy?"
    ), "random read output does not match input"

    assert (
        lyr.TestCapability(ogr.OLCRandomWrite) == 1
    ), "lyr.TestCapability(ogr.OLCRandomWrite) != 1"

    # Random write a feature
    feat.SetField("dummy", "i am no dummy")
    lyr.SetFeature(feat)
    feat_read_random = lyr.GetFeature(feat.GetFID())
    assert (
        feat_read_random.GetField("dummy") == "i am no dummy"
    ), "random read output does not match random write input"

    assert (
        lyr.TestCapability(ogr.OLCDeleteFeature) == 1
    ), "lyr.TestCapability(ogr.OLCDeleteFeature) != 1"

    assert lyr.GetFeatureCount() == 2

    def get_feature_count_from_gpkg_contents():
        sql_lyr = gpkg_ds.ExecuteSQL(
            'SELECT feature_count FROM gpkg_ogr_contents WHERE table_name = "field_test_layer"',
            dialect="DEBUG",
        )
        f = sql_lyr.GetNextFeature()
        ret = f.GetField(0)
        gpkg_ds.ReleaseResultSet(sql_lyr)
        return ret

    assert get_feature_count_from_gpkg_contents() == 2

    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE

    assert lyr.GetFeatureCount() == 3

    # 2 is expected here since CreateFeature() has temporarily disable triggers
    assert get_feature_count_from_gpkg_contents() == 2

    # Test upserting an existing feature
    feat.SetField("dummy", "updated")
    fid = feat.GetFID()
    assert lyr.UpsertFeature(feat) == ogr.OGRERR_NONE, "cannot upsert existing feature"

    assert feat.GetFID() == fid

    # UpsertFeature() has serialized value 3 and re-enables triggers
    assert get_feature_count_from_gpkg_contents() == 3

    upserted_feat = lyr.GetFeature(feat.GetFID())
    assert (
        upserted_feat.GetField("dummy") == "updated"
    ), "upsert failed to update existing feature"

    # Delete a feature
    lyr.DeleteFeature(feat.GetFID())

    assert get_feature_count_from_gpkg_contents() is None

    lyr.SyncToDisk()

    assert get_feature_count_from_gpkg_contents() is None

    assert lyr.GetFeatureCount() == 2, "delete feature did not delete"

    assert get_feature_count_from_gpkg_contents() == 2

    # Test upserting a non-existing feature
    assert (
        lyr.UpsertFeature(feat) == ogr.OGRERR_NONE
    ), "cannot upsert non-existing feature"
    assert feat.GetFID() == fid

    assert get_feature_count_from_gpkg_contents() == 3

    assert lyr.GetFeatureCount() == 3, "upsert failed to add non-existing feature"

    lyr.SyncToDisk()

    assert get_feature_count_from_gpkg_contents() == 3

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert (
        lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."

    # Test deleting non-existing feature
    assert (
        lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."

    # Delete the layer
    assert gpkg_ds.DeleteLayer("field_test_layer") == ogr.OGRERR_NONE


###############################################################################
# Test a variety of geometry feature types and attribute types


@pytest.mark.usefixtures("tbl_linestring")
def test_ogr_gpkg_8(gpkg_ds):

    lyr = gpkg_ds.GetLayer("tbl_linestring")

    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0, "cannot insert empty"

    feat.SetFID(6)
    assert lyr.SetFeature(feat) == 0, "cannot update with empty"

    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)

    assert validate(gpkg_ds.GetDescription(), "validation failed")

    lyr = gpkg_ds.GetLayerByName("tbl_linestring")
    assert lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() == ogr.OFSTFloat32
    feat = lyr.GetNextFeature()

    assert feat.GetField(0) == 10
    assert feat.GetField(1) == "test string 0 test"
    assert feat.GetField(2) == 3.14159
    assert feat.GetField(3) == "2014/05/17"
    assert feat.GetField(4) == "2014/12/31 23:59:59.999+00"
    assert feat.GetField(5) == "FFFE"
    assert feat.GetField(6) == 1
    assert feat.GetField(7) == -32768
    assert feat.GetField(8) == 1.23
    assert feat.GetField(9) == 1000000000000


def test_ogr_gpkg_8a(gpkg_ds):

    srs = osr.SpatialReference()
    # Test a non-default SRS
    srs.ImportFromEPSG(32631)

    lyr = gpkg_ds.CreateLayer("tbl_polygon", geom_type=ogr.wkbPolygon, srs=srs)
    assert lyr is not None

    lyr.StartTransaction()
    lyr.CreateField(ogr.FieldDefn("fld_datetime", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("fld_string", ogr.OFTString))

    geom = ogr.CreateGeometryFromWkt(
        "POLYGON((5 5, 10 5, 10 10, 5 10, 5 5),(6 6, 6 7, 7 7, 7 6, 6 6))"
    )
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetFID(-1)
        feat.SetField("fld_string", "my super string %d" % i)
        feat.SetField("fld_datetime", "2010-01-01")

        assert lyr.CreateFeature(feat) == 0, "cannot create polygon feature %d" % i
    lyr.CommitTransaction()

    feat = lyr.GetFeature(3)
    geom_read = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt() == geom_read.ExportToWkt()
    ), "geom output not equal to geom input"


def test_ogr_gpkg_8b(gpkg_ds):

    srs = osr.SpatialReference()
    # Test a non-default SRS
    srs.ImportFromEPSG(32631)

    # Test out the 3D support...
    lyr = gpkg_ds.CreateLayer("tbl_polygon25d", geom_type=ogr.wkbPolygon25D, srs=srs)
    assert lyr is not None

    lyr.CreateField(ogr.FieldDefn("fld_string", ogr.OFTString))
    geom = ogr.CreateGeometryFromWkt(
        "POLYGON((5 5 1, 10 5 2, 10 10 3, 5 104 , 5 5 1),(6 6 4, 6 7 5, 7 7 6, 7 6 7, 6 6 4))"
    )
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom_read = feat.GetGeometryRef()
    assert (
        geom.ExportToWkt() == geom_read.ExportToWkt()
    ), "3d geom output not equal to geom input"


###############################################################################
# Test support for extents and counts


@pytest.mark.usefixtures("tbl_linestring")
def test_ogr_gpkg_9(gpkg_ds):

    lyr = gpkg_ds.GetLayerByName("tbl_linestring")
    extent = lyr.GetExtent()
    assert extent == (5.0, 10.0, 5.0, 10.0), "got bad extent"

    fcount = lyr.GetFeatureCount()
    assert fcount == 10, "got bad featurecount"


###############################################################################
# Test non-SELECT SQL commands


@pytest.mark.usefixtures("tbl_linestring")
def test_ogr_gpkg_11(gpkg_ds):

    gpkg_ds.ExecuteSQL(
        "CREATE INDEX tbl_linestring_fld_integer_idx ON tbl_linestring(fld_integer)"
    )
    gpkg_ds.ExecuteSQL("ALTER TABLE tbl_linestring RENAME TO tbl_linestring_renamed;")
    gpkg_ds.ExecuteSQL("VACUUM")

    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)
    lyr = gpkg_ds.GetLayerByName("tbl_linestring_renamed")
    assert lyr is not None
    lyr.SetAttributeFilter("fld_integer = 10")
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test SELECT SQL commands


@pytest.mark.usefixtures("tbl_linestring")
def test_ogr_gpkg_12(gpkg_ds):

    gpkg_ds.ExecuteSQL("ALTER TABLE tbl_linestring RENAME TO tbl_linestring_renamed;")

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT * FROM tbl_linestring_renamed")
    assert sql_lyr.GetFIDColumn() == "fid"
    assert sql_lyr.GetGeomType() == ogr.wkbLineString
    assert sql_lyr.GetGeometryColumn() == "geom"
    assert sql_lyr.GetSpatialRef().ExportToWkt().find("32631") >= 0
    feat = sql_lyr.GetNextFeature()
    assert feat.GetFID() == 1
    assert sql_lyr.GetFeatureCount() == 10
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 10
    assert sql_lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() == ogr.OFSTBoolean
    assert sql_lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() == ogr.OFSTInt16
    assert sql_lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() == ogr.OFSTFloat32
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT "
        "CAST(fid AS INTEGER) AS FID, "
        "CAST(fid AS INTEGER) AS FID, "
        "_rowid_ ,"
        "CAST(geom AS BLOB) AS GEOM, "
        "CAST(geom AS BLOB) AS GEOM, "
        "CAST(fld_integer AS INTEGER) AS FLD_INTEGER, "
        "CAST(fld_integer AS INTEGER) AS FLD_INTEGER, "
        "CAST(fld_string AS TEXT) AS FLD_STRING, "
        "CAST(fld_real AS REAL) AS FLD_REAL, "
        "CAST(fld_binary as BLOB) as FLD_BINARY, "
        "CAST(fld_integer64 AS INTEGER) AS FLD_INTEGER64 "
        "FROM tbl_linestring_renamed"
    )
    assert sql_lyr.GetFIDColumn() == "FID"
    assert sql_lyr.GetGeometryColumn() == "GEOM"
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 5
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "FLD_INTEGER"
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "FLD_STRING"
    assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
    assert sql_lyr.GetLayerDefn().GetFieldDefn(2).GetName() == "FLD_REAL"
    assert sql_lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTReal
    assert sql_lyr.GetLayerDefn().GetFieldDefn(3).GetName() == "FLD_BINARY"
    assert sql_lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTBinary
    assert sql_lyr.GetLayerDefn().GetFieldDefn(4).GetName() == "FLD_INTEGER64"
    assert sql_lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTInteger64
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT * FROM tbl_linestring_renamed WHERE 0=1")
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    for sql in [
        "SELECT * FROM tbl_linestring_renamed LIMIT 1",
        "SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1",
        "SELECT * FROM tbl_linestring_renamed UNION ALL SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1",
    ]:
        sql_lyr = gpkg_ds.ExecuteSQL(sql)
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        assert sql_lyr.GetFeatureCount() == 1
        gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT sqlite_version()")
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 1
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0
    gpkg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test non-spatial tables


def test_ogr_gpkg_13(gpkg_ds):

    lyr = gpkg_ds.CreateLayer("non_spatial", geom_type=ogr.wkbNone)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None
    lyr.CreateField(ogr.FieldDefn("fld_integer", ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("fld_integer", 1)
    lyr.CreateFeature(feat)
    feat = None
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    assert feat.IsFieldNull("fld_integer")
    feat = lyr.GetNextFeature()
    assert feat.GetField("fld_integer") == 1

    # Test second aspatial layer
    lyr = gpkg_ds.CreateLayer("non_spatial2", geom_type=ogr.wkbNone)

    gdal.ErrorReset()
    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)
    assert gdal.GetLastErrorMsg() == "", "fail : warning NOT expected"
    assert gpkg_ds.GetLayerCount() == 2
    lyr = gpkg_ds.GetLayer("non_spatial")
    assert lyr.GetGeomType() == ogr.wkbNone
    feat = lyr.GetNextFeature()
    assert feat.IsFieldNull("fld_integer")
    feat = lyr.GetNextFeature()
    assert feat.GetField("fld_integer") == 1


###############################################################################
# Add various geometries to test spatial filtering


@pytest.mark.usefixtures("point_no_spi_but_with_dashes", "point_with_spi_and_dashes")
def test_ogr_gpkg_14(gpkg_ds):

    lyr = gpkg_ds.GetLayer("point_no_spi-but-with-dashes")

    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0

    f = lyr.GetFeature(5)
    assert f.GetGeometryRef() is None

    f = lyr.GetFeature(6)
    assert f.GetGeometryRef().ExportToWkt() == "POINT EMPTY"
    f = None

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM "point_no_spi-but-with-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gpkg_ds.ReleaseResultSet(sql_lyr)
    assert res == 0

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM "point-with-spi-and-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gpkg_ds.ReleaseResultSet(sql_lyr)
    assert res == 1

    # Test spatial filer right away
    lyr.SetSpatialFilterRect(1000, 30000000, 1000, 30000000)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is None


###############################################################################
def _has_spatialite_4_3_or_later(ds):
    has_spatialite_4_3_or_later = False
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
        if sql_lyr:
            f = sql_lyr.GetNextFeature()
            version = f.GetField(0)
            version = ".".join(version.split(".")[0:2])
            version = float(version)
            if version >= 4.3:
                has_spatialite_4_3_or_later = True
                # print('Spatialite 4.3 or later found')
            ds.ReleaseResultSet(sql_lyr)
    return has_spatialite_4_3_or_later


###############################################################################
# Test SQL functions


@pytest.mark.usefixtures(
    "tbl_linestring", "point_no_spi_but_with_dashes", "point_with_spi_and_dashes"
)
def test_ogr_gpkg_15(gpkg_ds):

    gpkg_ds = gdaltest.reopen(gpkg_ds, update=1)

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), "
        + 'ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM "point_no_spi-but-with-dashes" WHERE fid = 1'
    )
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.GetField(0) == 0
    assert feat.GetField(1) == 32631
    assert feat.GetField(2) == "POINT"
    assert feat.GetField(3) == 1000
    assert feat.GetField(4) == 30000000
    assert feat.GetField(5) == 1000
    assert feat.GetField(6) == 30000000
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # add an empty feature to tbl_linestring
    lyr = gpkg_ds.GetLayer("tbl_linestring")
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), "
        + "ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM tbl_linestring WHERE geom IS NULL"
    )
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.IsFieldNull(0)
    assert feat.IsFieldNull(1)
    assert feat.IsFieldNull(2)
    assert feat.IsFieldNull(3)
    assert feat.IsFieldNull(4)
    assert feat.IsFieldNull(5)
    assert feat.IsFieldNull(6)
    gpkg_ds.ReleaseResultSet(sql_lyr)

    for (expected_type, actual_type, expected_result) in [
        ("POINT", "POINT", 1),
        ("LINESTRING", "POINT", 0),
        ("GEOMETRY", "POINT", 1),
        ("POINT", "GEOMETRY", 0),
        ("GEOMETRYCOLLECTION", "MULTIPOINT", 1),
        ("GEOMETRYCOLLECTION", "POINT", 0),
    ]:
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT GPKG_IsAssignable('%s', '%s')" % (expected_type, actual_type)
        )
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gpkg_ds.ReleaseResultSet(sql_lyr)
        assert (
            got_result == expected_result
        ), "expected_type=%s actual_type=%s expected_result=%d got_result=%d" % (
            expected_type,
            actual_type,
            expected_result,
            got_result,
        )

    lyr = gpkg_ds.CreateLayer("non_spatial", geom_type=ogr.wkbNone)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    for (sql, expected_result) in [
        ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
        ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
        ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
        ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
        ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
        ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
        ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
        ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
        ("SELECT HasSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
        ("SELECT CreateSpatialIndex(NULL, 'geom')", 0),
        ("SELECT CreateSpatialIndex('bla', 'geom')", 0),
        ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
        ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
        ("SELECT DisableSpatialIndex(NULL, 'geom')", 0),
        ("SELECT DisableSpatialIndex('bla', 'geom')", 0),
        ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
        ("SELECT HasSpatialIndex(NULL, 'geom')", 0),
        ("SELECT HasSpatialIndex('bla', 'geom')", 0),
        ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
        ("SELECT CreateSpatialIndex('non_spatial', '')", 0),
        ("SELECT CreateSpatialIndex('point_no_spi-but-with-dashes', 'geom')", 1),
        # Final DisableSpatialIndex: will be effectively deleted at dataset closing
        ("SELECT DisableSpatialIndex('point_no_spi-but-with-dashes', 'geom')", 1),
    ]:
        if expected_result == 0:
            gdal.PushErrorHandler("CPLQuietErrorHandler")
        sql_lyr = gpkg_ds.ExecuteSQL(sql)
        if expected_result == 0:
            gdal.PopErrorHandler()
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gpkg_ds.ReleaseResultSet(sql_lyr)
        assert got_result == expected_result, sql

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Non existing entry
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 1234)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # New entry in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(32633)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 32633:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid code
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(0)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(x'00', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, NULL) FROM tbl_linestring")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid target SRID=0
    # GeoPackage: The record with an srs_id of 0 SHALL be used for undefined geographic coordinate reference systems.
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT ST_Transform(geom, 0), ST_SRID(ST_Transform(geom, 0)) FROM tbl_linestring"
        )
    assert sql_lyr.GetSpatialRef().ExportToWkt().find("Undefined geographic SRS") >= 0
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or feat.GetField(0) != 0:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid source SRID=0
    # GeoPackage: The record with an srs_id of 0 SHALL be used for undefined geographic coordinate reference systems.
    # The source is undefined geographic coordinate reference systems (based on WGS84) and the target is WGS84,
    # and the result is an identity transformation that leaves geometry unchanged.
    src_lyr = gpkg_ds.GetLayerByName("point-with-spi-and-dashes")
    assert src_lyr.GetSpatialRef().ExportToWkt().find("Undefined geographic SRS") >= 0
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL(
            'SELECT ST_Transform(geom, 4326), ST_SRID(ST_Transform(geom, 4326)) FROM "point-with-spi-and-dashes"'
        )
    assert sql_lyr.GetSpatialRef().ExportToWkt().find("WGS_1984") >= 0
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid spatialite geometry: SRID=4326,MULTIPOINT EMPTY truncated
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C04000000000000FE', 4326) FROM tbl_linestring"
        )
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_Transform(geom, ST_SRID(geom)) FROM tbl_linestring"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != "LINESTRING (5 5,10 5,10 10,5 10)":
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_SRID(ST_Transform(geom, 4326)) FROM tbl_linestring"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry: SRID=4326,MULTIPOINT EMPTY
    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_SRID(ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE', 4326)) FROM tbl_linestring"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: less than 8 bytes
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'00')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: 8 wrong bytes
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'0001020304050607')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'4750001100000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'475000110000000001040000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry, but long enough for our purpose...
    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_GeometryType(x'47500011000000000104000000')"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != "MULTIPOINT":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_GeometryType(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != "MULTIPOINT":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gpkg_ds.ExecuteSQL(
        "SELECT ST_IsEmpty(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')"
    )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 1:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid geometry
    with gdal.quiet_errors():
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT ST_GeometryType(x'475000030000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')"
        )
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable('POINT', NULL)")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable(NULL, 'POINT')")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != "b":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'x')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value(NULL, 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    if (
        ogr.GetGEOSVersionMajor() * 10000
        + ogr.GetGEOSVersionMinor() * 100
        + ogr.GetGEOSVersionMicro()
        >= 30800
    ):
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MakeValid(NULL)")
        feat = sql_lyr.GetNextFeature()
        assert feat.GetGeometryRef() is None
        gpkg_ds.ReleaseResultSet(sql_lyr)

        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MakeValid('invalid')")
        feat = sql_lyr.GetNextFeature()
        assert feat.GetGeometryRef() is None
        gpkg_ds.ReleaseResultSet(sql_lyr)

        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MakeValid(geom) FROM tbl_linestring")
        feat = sql_lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != "LINESTRING (5 5,10 5,10 10,5 10)":
            feat.DumpReadable()
            pytest.fail()
        gpkg_ds.ReleaseResultSet(sql_lyr)

    if _has_spatialite_4_3_or_later(gpkg_ds):
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT ST_Buffer(geom, 1e-10) FROM tbl_linestring"
        )
        assert sql_lyr.GetGeomType() == ogr.wkbPolygon
        assert sql_lyr.GetSpatialRef().ExportToWkt().find("32631") >= 0
        gpkg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test SetSRID() function


def test_ogr_gpkg_SetSRID():

    filename = "/vsimem/test_ogr_gpkg_SetSRID.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)

    for srid in (32631, 0, -1, 12345678):
        sql_lyr = ds.ExecuteSQL("SELECT ST_SRID(SetSRID(geom, %d)) FROM foo" % srid)
        f = sql_lyr.GetNextFeature()
        try:
            assert f.GetField(0) == srid, srid
        finally:
            ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT ST_SRID(SetSRID(NULL, 32631)) FROM foo")
    f = sql_lyr.GetNextFeature()
    try:
        assert f.GetField(0) is None
    finally:
        ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdal.Unlink("/vsimem/test_ogr_gpkg_SetSRID.gpkg")


###############################################################################
# Test ST_EnvIntersects() function


def test_ogr_gpkg_ST_EnvIntersects():

    filename = "/vsimem/test_ogr_gpkg_ST_EnvIntersects.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 2,3 4)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(5 6,7 8)"))
    lyr.CreateFeature(f)

    sql_lyr = ds.ExecuteSQL(
        "SELECT ST_EnvIntersects(geom, 0, 0, 0.99, 100),"
        + "       ST_EnvIntersects(geom, 0, 4.01, 100, 100),"
        + "       ST_EnvIntersects(geom, 3.01, 0, 100, 100),"
        + "       ST_EnvIntersects(geom, 0, 0, 100, 1.99),"
        + "       ST_EnvIntersects(geom, 0.99, 1.99, 1.01, 2.01),"
        + "       ST_EnvIntersects(geom, 0.99, 3.99, 1.01, 4.01),"
        + "       ST_EnvIntersects(geom, 2.99, 3.99, 3.01, 4.01),"
        + "       ST_EnvIntersects(geom, 2.99, 1.99, 3.01, 2.01)"
        + " FROM foo WHERE fid = 1"
    )
    f = sql_lyr.GetNextFeature()
    try:
        assert f.GetField(0) == 0
        assert f.GetField(1) == 0
        assert f.GetField(2) == 0
        assert f.GetField(3) == 0
        assert f.GetField(4) == 1
        assert f.GetField(5) == 1
        assert f.GetField(6) == 1
        assert f.GetField(7) == 1
    finally:
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT ST_EnvIntersects(a.geom, b.geom) FROM foo a, foo b WHERE a.fid = 1 AND b.fid = 1"
    )
    f = sql_lyr.GetNextFeature()
    try:
        assert f.GetField(0) == 1
    finally:
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT ST_EnvIntersects(a.geom, b.geom) FROM foo a, foo b WHERE a.fid = 1 AND b.fid = 2"
    )
    f = sql_lyr.GetNextFeature()
    try:
        assert f.GetField(0) == 0
    finally:
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT ST_EnvIntersects(a.geom, b.geom) FROM foo a, foo b WHERE a.fid = 2 AND b.fid = 1"
    )
    f = sql_lyr.GetNextFeature()
    try:
        assert f.GetField(0) == 0
    finally:
        ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdal.Unlink("/vsimem/test_ogr_gpkg_ST_EnvIntersects.gpkg")


###############################################################################
# Test unknown extensions


def test_ogr_gpkg_16(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpk_16.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    ds.CreateLayer("foo")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions ( table_name, column_name, "
        + "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'myext', 'some ext', 'write-only' ) "
    )
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open(fname)
    lyr = ds.GetLayer(0)
    lyr.GetLayerDefn()
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == "", "fail : warning NOT expected"

    # Warning since we open as read-write
    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"

    ds.ExecuteSQL(
        "UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'"
    )
    ds = None

    # Warning since we open as read-only
    ds = ogr.Open(fname)
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"

    # and also as read-write
    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"
    ds = None


def test_ogr_gpkg_16a(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpk_16.gpkg"

    # Test with unsupported geometry type
    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    ds.CreateLayer("foo")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions ( table_name, column_name, "
        + "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'gpkg_geom_XXXX', 'some ext', 'read-write' ) "
    )
    ds = None

    ds = ogr.Open(fname)
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"


def test_ogr_gpkg_16b(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpk_16.gpkg"

    # Test with database wide unknown extension
    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    ds.CreateLayer("foo")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions ( "
        + "extension_name, definition, scope ) VALUES ( 'myext', 'some ext', 'write-only' ) "
    )
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open(fname)
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() == "", "fail : warning NOT expected"

    # Warning since we open as read-write
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(fname, update=1)
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"

    ds.ExecuteSQL(
        "UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'"
    )
    ds = None

    # Warning since we open as read-only
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(fname)
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"

    # and also as read-write
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(fname, update=1)
    assert gdal.GetLastErrorMsg() != "", "fail : warning expected"
    ds = None


###############################################################################
# Run INDIRECT_SQLITE dialect


def test_ogr_gpkg_17(tmp_vsimem):

    ds = gdaltest.gpkg_dr.CreateDataSource(tmp_vsimem / "ogr_gpkg_17.gpkg")
    sql_lyr = ds.ExecuteSQL("SELECT ogr_version()", dialect="INDIRECT_SQLITE")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test geometry type extension


def test_ogr_gpkg_18(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_18.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("wkbCircularString", geom_type=ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"

    gdal.ErrorReset()
    ds = ogr.Open(fname)
    assert gdal.GetLastErrorMsg() == "", "fail : warning NOT expected"

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbCircularString
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'wkbCircularString' AND extension_name = 'gpkg_geom_CIRCULARSTRING'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None


def test_ogr_gpkg_18a(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_18.gpkg"

    # Also test with a wkbUnknown layer and add curve geometries afterwards
    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    lyr.CreateFeature(f)
    f = None

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name = 'gpkg_geom_CIRCULARSTRING'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    gdal.ErrorReset()
    ds = ogr.Open(fname)
    assert gdal.GetLastErrorMsg() == "", "fail : warning NOT expected"

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    gdal.ErrorReset()
    ret = lyr.CreateFeature(f)
    assert ret == 0 and gdal.GetLastErrorMsg() == ""
    f = None
    ds = None


def test_ogr_gpkg_18b(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_18.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbTriangle)
    assert lyr is not None
    with gdal.quiet_errors():
        # Warning 1: Registering non-standard gpkg_geom_TRIANGLE extension
        ds.FlushCache()
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name = 'gpkg_geom_TRIANGLE'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    if has_validate():
        ret = validate(fname, tmpdir=tmp_path, quiet=True)
        assert not ret, "validation unexpectedly succeeded"


def test_ogr_gpkg_18c(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_18.gpkg"

    # Test non-linear geometry in GeometryCollection
    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION(CIRCULARSTRING(0 0,1 0,0 0))")
    )
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name LIKE 'gpkg_geom_%'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test metadata


def test_ogr_gpkg_19(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_19.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    assert not ds.GetMetadata()
    lyr = ds.CreateLayer("test_without_md")
    assert not lyr.GetMetadata()

    ds.SetMetadataItem("foo", "bar")

    # GEOPACKAGE metadata domain is not allowed in a non-raster context
    with gdal.quiet_errors():
        ds.SetMetadata(ds.GetMetadata("GEOPACKAGE"), "GEOPACKAGE")
        ds.SetMetadataItem("foo", ds.GetMetadataItem("foo", "GEOPACKAGE"), "GEOPACKAGE")

    ds = None

    ds = ogr.Open(fname)
    assert ds.GetMetadataDomainList() == [""]

    ds = ogr.Open(fname)
    assert len(ds.GetMetadata()) == 1

    ds = ogr.Open(fname)
    assert ds.GetMetadataItem("foo") == "bar", ds.GetMetadata()
    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.CreateLayer(
        "test_with_md", options=["IDENTIFIER=ident", "DESCRIPTION=desc"]
    )
    lyr.SetMetadataItem("IDENTIFIER", "ignored_because_of_lco")
    lyr.SetMetadataItem("DESCRIPTION", "ignored_because_of_lco")
    lyr.SetMetadata(
        {
            "IDENTIFIER": "ignored_because_of_lco",
            "DESCRIPTION": "ignored_because_of_lco",
        }
    )
    ds = None

    ds = ogr.Open(fname)

    # Check that we don't create triggers
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'trigger' AND tbl_name IN ('gpkg_metadata', 'gpkg_metadata_reference')"
    )
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadataItem("IDENTIFIER") == "ident"
    assert lyr.GetMetadataItem("DESCRIPTION") == "desc"

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadata() == {"IDENTIFIER": "ident", "DESCRIPTION": "desc"}
    lyr.SetMetadataItem("IDENTIFIER", "another_ident")
    lyr.SetMetadataItem("DESCRIPTION", "another_desc")
    ds = None

    # FIXME? Is it expected to have a .aux.xml here ?
    gdal.Unlink(fname.with_suffix(".gpkg.aux.xml"))

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadata() == {
        "IDENTIFIER": "another_ident",
        "DESCRIPTION": "another_desc",
    }
    lyr.SetMetadataItem("foo", "bar")
    lyr.SetMetadataItem("bar", "baz", "another_domain")
    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadataDomainList() == ["", "another_domain"]
    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadata() == {
        "IDENTIFIER": "another_ident",
        "foo": "bar",
        "DESCRIPTION": "another_desc",
    }
    assert lyr.GetMetadata("another_domain") == {"bar": "baz"}
    lyr.SetMetadata(None)
    lyr.SetMetadata(None, "another_domain")
    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_with_md")
    assert lyr.GetMetadata() == {
        "IDENTIFIER": "another_ident",
        "DESCRIPTION": "another_desc",
    }
    assert lyr.GetMetadataDomainList() == [""]
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"


###############################################################################
# Test spatial reference system


def test_ogr_gpkg_20(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_20.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)

    # "Conflict" with EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        """GEOGCS["my geogcs",
    DATUM["my datum",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["my_org","4326"]]"""
    )
    lyr = ds.CreateLayer("my_org_4326", srs=srs)

    # No authority node
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        """GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]"""
    )
    lyr = ds.CreateLayer("without_org", srs=srs)

    ds = None

    ds = ogr.Open(fname)

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs' AND srs_id = 100000 AND organization='MY_ORG' AND organization_coordsys_id=4326 AND description is NULL"
    )
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='another geogcs' AND srs_id = 100001 AND organization='NONE' AND organization_coordsys_id=100001 AND description is NULL"
    )
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    lyr = ds.GetLayer("my_org_4326")
    assert lyr.GetSpatialRef().ExportToWkt().find("my geogcs") >= 0
    lyr = ds.GetLayer("without_org")
    assert lyr.GetSpatialRef().ExportToWkt().find("another geogcs") >= 0
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"


def test_ogr_gpkg_20a(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_20.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("foo4326", srs=srs)
    assert lyr is not None
    ds.ExecuteSQL(
        "UPDATE gpkg_spatial_ref_sys SET definition='invalid', "
        "organization='', organization_coordsys_id = 0 "
        "WHERE srs_id = 4326"
    )
    ds = None

    # Unable to parse srs_id '4326' well-known text 'invalid'
    with gdal.quiet_errors():
        ds = ogr.Open(fname, update=1)

    ds.ExecuteSQL("DELETE FROM gpkg_spatial_ref_sys WHERE srs_id = 4326")
    ds = None
    with gdal.config_option(
        "OGR_GPKG_FOREIGN_KEY_CHECK", "NO"
    ), gdaltest.error_handler():
        # Warning 1: unable to read srs_id '4326' from gpkg_spatial_ref_sys
        ds = ogr.Open(fname, update=1)
    ds = None


def test_ogr_gpkg_20b(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_20.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("foo4326", srs=srs)
    assert lyr is not None

    ds.ExecuteSQL("DROP TABLE gpkg_spatial_ref_sys")
    ds.ExecuteSQL(
        "CREATE TABLE gpkg_spatial_ref_sys (srs_name TEXT, "
        "srs_id INTEGER, organization TEXT, "
        "organization_coordsys_id INTEGER, definition TEXT)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_spatial_ref_sys "
        "(srs_name,srs_id,organization,organization_coordsys_id,"
        "definition) VALUES (NULL,4326,NULL,NULL,NULL)"
    )
    ds = None

    # Warning 1: null definition for srs_id '4326' in gpkg_spatial_ref_sys
    with gdal.config_option(
        "OGR_GPKG_FOREIGN_KEY_CHECK", "NO"
    ), gdaltest.error_handler():
        ds = ogr.Open(fname, update=1)
    ds = None


def test_ogr_gpkg_srs_non_duplication_custom_crs(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_20.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        """GEOGCS["my custom geogcs",
    DATUM["my datum",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]"""
    )
    lyr = ds.CreateLayer("test", srs=srs)
    assert lyr
    lyr = ds.CreateLayer("test2", srs=srs)
    assert lyr

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 4  # srs_id 0, 1, 4326 + custom one

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my custom geogcs'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f["srs_id"] == 100000
    assert f["organization"] == "NONE"
    assert f["organization_coordsys_id"] == 100000
    ds.ReleaseResultSet(sql_lyr)

    # Test now transitionning to definition_12_063 / WKT2 database structure...
    srs_3d = osr.SpatialReference()
    srs_3d.SetFromUserInput(
        """GEOGCRS["srs 3d",
    DATUM["some datum",
        ELLIPSOID["some ellipsoid",6378137,298.257223563,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,3],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["ellipsoidal height (h)",up,
            ORDER[3],
            LENGTHUNIT["metre",1]]]"""
    )
    lyr = ds.CreateLayer("test_3d", srs=srs_3d)
    assert lyr
    lyr = ds.CreateLayer("test_3d_bis", srs=srs_3d)
    assert lyr

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='srs 3d'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    # Test again with SRS that can be represented in WKT1
    lyr = ds.CreateLayer("test3", srs=srs)
    assert lyr

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my custom geogcs'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    ds = None


def test_ogr_gpkg_srs_non_consistent_with_official_definition(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_20.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    test_fake_4267 = osr.SpatialReference()
    test_fake_4267.SetFromUserInput(
        """GEOGCS["my geogcs 4267",
    DATUM["WGS_1984",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["EPSG","4267"]]"""
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test_fake_4267", srs=test_fake_4267)
    assert (
        gdal.GetLastErrorMsg()
        == "Passed SRS uses EPSG:4267 identification, but its definition is not compatible with the official definition of the object. Registering it as a non-EPSG entry into the database."
    )
    assert lyr

    # EPSG:4326 already in the database
    test_fake_4326 = osr.SpatialReference()
    test_fake_4326.SetFromUserInput(
        """GEOGCS["my geogcs 4326",
    DATUM["WGS_1984",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["EPSG","4326"]]"""
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test_fake_4326", srs=test_fake_4326)
    assert (
        gdal.GetLastErrorMsg()
        == "Passed SRS uses EPSG:4326 identification, but its definition is not compatible with the definition of that object already in the database. Registering it as a new entry into the database."
    )
    assert lyr

    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer("test_fake_4267")
    assert (
        lyr.GetSpatialRef().ExportToWkt().replace(',AUTHORITY["EPSG","9122"]', "")
        == 'GEOGCS["my geogcs 4267",DATUM["WGS_1984",SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4267"]]'
    )

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs 4267'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f["srs_id"] == 100000
    assert f["organization"] == "NONE"
    assert f["organization_coordsys_id"] == 100000
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer("test_fake_4326")
    assert (
        lyr.GetSpatialRef().ExportToWkt().replace(',AUTHORITY["EPSG","9122"]', "")
        == 'GEOGCS["my geogcs 4326",DATUM["WGS_1984",SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'
    )

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs 4326'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f["srs_id"] == 100001
    assert f["organization"] == "NONE"
    assert f["organization_coordsys_id"] == 100001
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc_before = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    gdal.ErrorReset()
    gdal.ErrorReset()
    lyr = ds.CreateLayer("test_fake_4267_bis", srs=test_fake_4267)
    assert gdal.GetLastErrorMsg() == ""

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc_after = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    assert fc_before == fc_after
    ds = None


def test_ogr_gpkg_write_srs_undefined_geographic(tmp_path):

    fname = tmp_path / "ogr_gpkg_srs_undefined_geographic.gpkg"

    gpkg_ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    assert gpkg_ds is not None

    # Check initial default SRS entries in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    gpkg_spatial_ref_sys_total = sql_lyr.GetNextFeature().GetField(0)
    assert gpkg_spatial_ref_sys_total == 3  # entries with SRS IDs: -1, 0, 4326
    gpkg_ds.ReleaseResultSet(sql_lyr)

    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'GEOGCS["Undefined geographic SRS",DATUM["unknown",SPHEROID["unknown",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    )
    lyr = gpkg_ds.CreateLayer(
        "srs_test_geographic_layer", geom_type=ogr.wkbPoint, srs=srs
    )
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find("Undefined geographic SRS") >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsGeographic()

    gpkg_ds = None
    gpkg_ds = ogr.Open(fname)

    # Check no new SRS entries have been inserted into gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    assert gpkg_spatial_ref_sys_total == sql_lyr.GetNextFeature().GetField(0)
    gpkg_ds.ReleaseResultSet(sql_lyr)

    lyr = gpkg_ds.GetLayer(0)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find("Undefined geographic SRS") >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsGeographic()

    gpkg_ds = None


def test_ogr_gpkg_write_srs_undefined_Cartesian(tmp_path):

    fname = tmp_path / "ogr_gpkg_srs_Cartesian.gpkg"

    gpkg_ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    assert gpkg_ds is not None

    # Check initial default SRS entries in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    gpkg_spatial_ref_sys_total = sql_lyr.GetNextFeature().GetField(0)
    assert gpkg_spatial_ref_sys_total == 3  # SRS with IDs: -1, 0, 4326
    gpkg_ds.ReleaseResultSet(sql_lyr)

    srs = osr.SpatialReference()
    srs.SetFromUserInput('LOCAL_CS["Undefined Cartesian SRS"]')
    lyr = gpkg_ds.CreateLayer(
        "srs_test_Cartesian_layer", geom_type=ogr.wkbPoint, srs=srs
    )
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find("Undefined Cartesian SRS") >= 0
    assert lyr.GetSpatialRef().IsLocal()

    gpkg_ds = None
    gpkg_ds = ogr.Open(fname)

    # Check no new SRS entries have been inserted into gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    assert gpkg_spatial_ref_sys_total == sql_lyr.GetNextFeature().GetField(0)
    gpkg_ds.ReleaseResultSet(sql_lyr)

    lyr = gpkg_ds.GetLayer(0)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find("Undefined Cartesian SRS") >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsLocal()

    gpkg_ds = None


###############################################################################
# Test maximum width of text fields


def test_ogr_gpkg_21(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_21.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test")
    field_defn = ogr.FieldDefn("str", ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)
    ds = None

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetWidth() == 2
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, "ab")
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() == ""

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, "41E9")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != ""

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, "abc")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != ""

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == "abc"


def test_ogr_gpkg_21a(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_21.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test", options=["TRUNCATE_FIELDS=YES"])
    field_defn = ogr.FieldDefn("str", ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, "41E9")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != ""

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == "A_"

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, "abc")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != ""

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == "ab"


###############################################################################


def test_ogr_gpkg_table_in_gpkg_content_but_missing(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_table_in_gpkg_content_but_missing.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.CreateLayer("valid")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents VALUES('non_existent','attributes','non_existent','','2022-05-18T17:24:49.837Z',NULL,NULL,NULL,NULL,-1);"
    )
    ds = None
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(filename)
    assert "non_existent" in gdal.GetLastErrorMsg()
    assert ds.GetLayerCount() == 1

    ds = None


###############################################################################
# Test FID64 support


def test_ogr_gpkg_22(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_22.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test")
    field_defn = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    feat.SetFID(1234567890123)
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    ds = ogr.Open(fname)
    lyr = ds.GetLayerByName("test")
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1234567890123


###############################################################################
# Test not nullable fields


def test_ogr_gpkg_23(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_23.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.GeomFieldDefn("geomfield_not_nullable", ogr.wkbPoint)
    field_defn.SetNullable(0)
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

    # Nullable geometry field
    lyr = ds.CreateLayer("test2", geom_type=ogr.wkbPoint, options=["SPATIAL_INDEX=NO"])

    # Cannot add more than one geometry field
    with gdal.quiet_errors():
        ret = lyr.CreateGeomField(ogr.GeomFieldDefn("foo", ogr.wkbPoint))
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Not-nullable fields and geometry fields created after table creation
    lyr = ds.CreateLayer("test3", geom_type=ogr.wkbNone)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    lyr.CreateField(field_defn)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE data_type = 'features'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 2

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name='gpkg_extensions'")
    has_gpkg_extensions = sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    assert not has_gpkg_extensions

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 2

    field_defn = ogr.GeomFieldDefn("geomfield_not_nullable", ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE data_type = 'features'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 3

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name='gpkg_extensions'")
    has_gpkg_extensions = sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    assert not has_gpkg_extensions

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 3

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    f.SetGeomFieldDirectly(
        "geomfield_not_nullable", ogr.CreateGeometryFromWkt("POINT(0 0)")
    )
    lyr.CreateFeature(f)
    f = None

    # Not Nullable geometry field
    lyr = ds.CreateLayer(
        "test4", geom_type=ogr.wkbPoint, options=["GEOMETRY_NULLABLE=NO"]
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = None

    ds.CreateLayer("test5", geom_type=ogr.wkbNone)

    ds = None

    ds = ogr.Open(fname)

    lyr = ds.GetLayerByName("test5")
    field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    with gdal.quiet_errors():
        assert lyr.CreateGeomField(field_defn) != 0

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

    lyr = ds.GetLayerByName("test2")
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1

    lyr = ds.GetLayerByName("test3")
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

    lyr = ds.GetLayerByName("test4")
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0

    ds = None


###############################################################################
# Test unique constraints on fields


def test_ogr_gpkg_unique(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_unique.gpkg"

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(fname)
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

    f = ogr.Feature(layerDefinition)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    # Test adding columns after "crystallization"
    field_defn = ogr.FieldDefn("field_unique_failure", ogr.OFTString)
    field_defn.SetUnique(1)
    # Not allowed by sqlite3. Could potentially be improved
    with gdal.quiet_errors():
        assert lyr.CreateField(field_defn) == ogr.OGRERR_FAILURE

    # Create another layer from SQL to test quoting of fields
    # and indexes
    # Note: leave create table in a single line because of regex spaces testing
    sql = (
        'CREATE TABLE IF NOT EXISTS "test2" ( "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "field_default" TEXT, "field_no_unique" TEXT, "field_unique" TEXT UNIQUE,`field unique2` TEXT UNIQUE,field_unique3 TEXT UNIQUE, FIELD_UNIQUE_INDEX TEXT, `field unique index2` TEXT, "field_unique_index3" TEXT, NOT_UNIQUE TEXT);',
        "CREATE UNIQUE INDEX test2_unique_idx ON test2(field_unique_index);",  # field_unique_index in lowercase whereas in uppercase in CREATE TABLE statement
        "CREATE UNIQUE INDEX test2_unique_idx2 ON test2(`field unique index2`);",
        'CREATE UNIQUE INDEX test2_unique_idx3 ON test2("field_unique_index3");',
        "INSERT INTO gpkg_contents VALUES('test2','attributes','test2','','2020-05-27T12:27:30.136Z',NULL,NULL,NULL,NULL,0);",
        "INSERT INTO gpkg_ogr_contents VALUES('test2',NULL);",
    )

    for s in sql:
        ds.ExecuteSQL(s)

    ds = None

    # Reload
    ds = ogr.Open(fname)

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

    ds = None


###############################################################################
# Test unique constraints on fields


def test_ogr_gpkg_unique_many_layers(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_unique_many_layers.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    THRESHOLD = 10
    for i in range(THRESHOLD + 1):
        lyr_name = "test" + str(i)
        ds.ExecuteSQL(
            f'CREATE TABLE IF NOT EXISTS "{lyr_name}" ( "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, other_field TEXT, "field_unique" TEXT UNIQUE);'
        )
        ds.ExecuteSQL(
            f"CREATE UNIQUE INDEX {lyr_name}_unique_idx ON {lyr_name}(other_field);"
        )
    ds = None

    ds = ogr.Open(filename)
    for i in range(THRESHOLD + 1):
        lyr = ds.GetLayerByName("test" + str(i))
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldDefn(0).IsUnique()
        assert lyr_defn.GetFieldDefn(1).IsUnique()
    ds = None


###############################################################################
# Test default values


def test_ogr_gpkg_24(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_24.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
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

    # This will be translated as "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
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

    # field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    # field_defn.SetDefault("CURRENT_TIME")
    # lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Test adding columns after "crystallization"
    field_defn = ogr.FieldDefn("field_datetime5", ogr.OFTDateTime)
    field_defn.SetDefault("'2016/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_datetime6", ogr.OFTDateTime)
    field_defn.SetDefault("'2016/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn("field_string2", ogr.OFTString)
    field_defn.SetDefault("'X'")
    lyr.CreateField(field_defn)

    # Doesn't work currently. Would require rewriting the whole table
    # field_defn = ogr.FieldDefn( 'field_datetimeX', ogr.OFTDateTime )
    # field_defn.SetDefault("CURRENT_TIMESTAMP")
    # lyr.CreateField(field_defn)

    ds = None

    ds = ogr.Open(fname, update=1)
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
    # Translated from "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))" to CURRENT_TIMESTAMP
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
        == "CURRENT_TIMESTAMP"
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
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    f = lyr.GetNextFeature()
    if (
        f.GetField("field_string") != "a'b"
        or f.GetField("field_int") != 123
        or f.GetField("field_real") != 1.23
        or not f.IsFieldNull("field_nodefault")
        or not f.IsFieldSet("field_datetime")
        or f.GetField("field_datetime2") != "2015/06/30 12:34:56+00"
        or f.GetField("field_datetime4") != "2015/06/30 12:34:56.123+00"
        or not f.IsFieldSet("field_datetime3")
        or not f.IsFieldSet("field_date")
        or f.GetField("field_datetime5") != "2016/06/30 12:34:56.123+00"
        or f.GetField("field_datetime6") != "2016/06/30 12:34:56+00"
        or f.GetField("field_string2") != "X"
    ):
        f.DumpReadable()
        pytest.fail()

    ds = None


###############################################################################
# Test creating a field with the fid name


def test_ogr_gpkg_25(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_25.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
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

    ds = None


###############################################################################
# Test dataset transactions


def test_ogr_gpkg_26(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_26.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)

    assert ds.TestCapability(ogr.ODsCTransactions) == 1

    ret = ds.StartTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.StartTransaction()
    assert ret != 0

    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ret = ds.RollbackTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.RollbackTransaction()
    assert ret != 0
    ds = None

    ds = ogr.Open(fname, update=1)
    assert ds.GetLayerCount() == 0
    ret = ds.StartTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.StartTransaction()
    assert ret != 0

    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ret = ds.CommitTransaction()
    assert ret == 0
    with gdal.quiet_errors():
        ret = ds.CommitTransaction()
    assert ret != 0
    ds = None

    ds = ogr.Open(fname, update=1)
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

    if False:  # pylint: disable=using-constant-test
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
        # For some reason fails with SQLite 3.6.X with 'failed to execute insert : callback requested query abort'
        # but not with later versions...
        assert ret == 0

    ds = None


###############################################################################
# Test interface with Spatialite


def test_ogr_gpkg_27(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_27.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL("SELECT GeomFromGPB(null)")
    if sql_lyr is None:
        ds = None
        pytest.skip()
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 49)"))
    lyr.CreateFeature(f)
    sql_lyr = ds.ExecuteSQL("SELECT GeomFromGPB(geom) FROM test")
    f = sql_lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (2 49)":
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test ogr2ogr -a_srs (as the geopackage driver doesn't clone the passed SRS
# but inc/dec its ref count, which can exhibit issues in GDALVectorTanslate())


def test_ogr_gpkg_28(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        tmp_vsimem / "ogr_gpkg_28.gpkg", srcDS, format="GPKG", dstSRS="EPSG:4326"
    )
    assert str(ds.GetLayer(0).GetSpatialRef()).find("1984") != -1

    ds = None


###############################################################################
# Test XYM / XYZM support


def test_ogr_gpkg_29(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_29.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    assert ds.TestCapability(ogr.ODsCMeasuredGeometries) == 1
    lyr = ds.CreateLayer("pointm", geom_type=ogr.wkbPointM)
    assert lyr.TestCapability(ogr.OLCMeasuredGeometries) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT M (1 2 3)"))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer("pointzm", geom_type=ogr.wkbPointZM)
    assert lyr.TestCapability(ogr.OLCMeasuredGeometries) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT ZM (1 2 3 4)"))
    lyr.CreateFeature(f)
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(fname, update=1)
    lyr = ds.GetLayerByName("pointm")
    assert lyr.GetGeomType() == ogr.wkbPointM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != "POINT M (1 2 3)":
        f.DumpReadable()
        pytest.fail()

    # Generate a XYM envelope
    ds.ExecuteSQL(
        "UPDATE pointm SET geom = x'4750000700000000000000000000F03F000000000000F03F000000000000004000000000000000400000000000000840000000000000084001D1070000000000000000F03F00000000000000400000000000000840'"
    )

    lyr = ds.GetLayerByName("pointzm")
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != "POINT ZM (1 2 3 4)":
        f.DumpReadable()
        pytest.fail()

    # Generate a XYZM envelope
    ds.ExecuteSQL(
        "UPDATE pointzm SET geom = x'4750000900000000000000000000F03F000000000000F03F00000000000000400000000000000040000000000000084000000000000008400000000000001040000000000000104001B90B0000000000000000F03F000000000000004000000000000008400000000000001040'"
    )

    ds = None

    # Check again
    ds = ogr.Open(tmp_vsimem / "ogr_gpkg_29.gpkg")
    lyr = ds.GetLayerByName("pointm")
    assert lyr.GetGeomType() == ogr.wkbPointM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != "POINT M (1 2 3)":
        f.DumpReadable()
        pytest.fail()
    lyr = ds.GetLayerByName("pointzm")
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != "POINT ZM (1 2 3 4)":
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test non standard file extension (#6396)


def test_ogr_gpkg_30(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_30.geopkg"

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ""
    ds = None

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(fname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ""
    ds = None

    with gdal.quiet_errors():
        gdaltest.gpkg_dr.DeleteDataSource(fname)


###############################################################################
# Test CURVE and SURFACE types


def test_ogr_gpkg_31(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_31.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    lyr = ds.CreateLayer("curve", geom_type=ogr.wkbCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("COMPOUNDCURVE ((1 2,3 4))"))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer("surface", geom_type=ogr.wkbSurface)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("CURVEPOLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(fname)
    lyr = ds.GetLayerByName("curve")
    assert lyr.GetGeomType() == ogr.wkbCurve
    lyr = ds.GetLayerByName("surface")
    assert lyr.GetGeomType() == ogr.wkbSurface
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"


###############################################################################
# Run creating a non-spatial layer that isn't registered as 'aspatial' and
# read it back


def test_ogr_gpkg_32(tmp_vsimem, tmp_path):

    fname = tmp_vsimem / "ogr_gpkg_32.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(fname)
    ds.CreateLayer(
        "aspatial", geom_type=ogr.wkbNone, options=["ASPATIAL_VARIANT=NOT_REGISTERED"]
    )
    ds = None

    ds = ogr.Open(fname)
    assert ds.GetLayerCount() == 1
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert validate(fname, tmpdir=tmp_path), "validation failed"


###############################################################################
# Test OGR_CURRENT_DATE


def test_ogr_gpkg_33(tmp_vsimem):

    fname = tmp_vsimem / "ogr_gpkg_33.gpkg"

    with gdal.config_option("OGR_CURRENT_DATE", "2000-01-01T:00:00:00.000Z"):
        ds = gdaltest.gpkg_dr.CreateDataSource(fname)
        ds.CreateLayer("test", geom_type=ogr.wkbNone)
        ds = None

    ds = ogr.Open(fname)
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_contents WHERE last_change = '2000-01-01T:00:00:00.000Z'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test rename and delete a layer registered in extensions, metadata, spatial index etc


def test_ogr_gpkg_34(tmp_vsimem):

    layer_name = """weird'layer"name"""

    dbname = tmp_vsimem / "ogr_gpkg_34.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbCurvePolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("CURVEPOLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    lyr.SetMetadataItem("FOO", "BAR")
    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT gdc_tn UNIQUE (table_name, name)
)"""
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns VALUES('weird''layer\"name', 'foo', 'foo_constraints', NULL, NULL, NULL, NULL)"
    )

    # QGIS layer_styles extension: https://github.com/pka/qgpkg/blob/master/qgis_geopackage_extension.md
    ds.ExecuteSQL(
        """CREATE TABLE "layer_styles" ( "id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "f_table_catalog" TEXT(256), "f_table_schema" TEXT(256), "f_table_name" TEXT(256), "f_geometry_column" TEXT(256), "styleName" TEXT(30), "styleQML" TEXT, "styleSLD" TEXT, "useAsDefault" BOOLEAN, "description" TEXT, "owner" TEXT(30), "ui" TEXT(30), "update_time" DATETIME DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')))"""
    )
    ds.ExecuteSQL(
        "INSERT INTO layer_styles VALUES(1, NULL, NULL, 'weird''layer\"name', 'geom', 'styleName', 'styleQML', 'styleSLD', 0, 'description', 'owner', 'ui', NULL)"
    )
    ds = None

    # Check that there are reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert layer_name in content

    ds = ogr.Open(dbname, update=1)
    new_layer_name = """weird2'layer"name"""
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO gpkg_contents')
    assert gdal.GetLastErrorMsg() != ""
    gdal.ErrorReset()
    ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO "weird2\'layer""name"')
    ds = None

    ds = ogr.Open(dbname, update=1)
    gdal.ErrorReset()
    ds.ExecuteSQL('ALTER TABLE "weird2\'layer""name" RENAME COLUMN "foo" TO "bar"')
    assert gdal.GetLastErrorMsg() == ""
    lyr = ds.GetLayerByName(new_layer_name)
    assert lyr.GetLayerDefn().GetFieldIndex("bar") >= 0

    gdal.ErrorReset()
    ds.ExecuteSQL('ALTER TABLE "weird2\'layer""name" DROP COLUMN "bar"')
    assert gdal.GetLastErrorMsg() == ""
    assert lyr.GetLayerDefn().GetFieldIndex("bar") < 0

    ds.ExecuteSQL("VACUUM")
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert layer_name not in content
    layer_name = new_layer_name

    ds = ogr.Open(dbname, update=1)
    # currently we don't suppress rows from layer_styles
    ds.ExecuteSQL("DELETE FROM layer_styles")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.ExecuteSQL("DELLAYER:does_not_exist")
    assert gdal.GetLastErrorMsg() != ""
    gdal.ErrorReset()
    ds.ExecuteSQL("DELLAYER:" + layer_name)
    assert gdal.GetLastErrorMsg() == ""
    ds.ExecuteSQL("VACUUM")
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert layer_name not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    # Try again with DROP TABLE syntax
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbCurvePolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("CURVEPOLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    lyr.SetMetadataItem("FOO", "BAR")
    lyr = ds.CreateLayer("another_layer_name")
    ds = None

    ds = ogr.Open(dbname, update=1)
    gdal.ErrorReset()
    ds.ExecuteSQL('DROP TABLE "weird2\'layer""name"')
    assert gdal.GetLastErrorMsg() == ""
    ds.ExecuteSQL("DROP TABLE another_layer_name")
    assert gdal.GetLastErrorMsg() == ""
    with gdal.quiet_errors():
        ds.ExecuteSQL('DROP TABLE "foobar"')
    assert gdal.GetLastErrorMsg() != ""
    gdal.ErrorReset()
    ds.ExecuteSQL("VACUUM")
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert layer_name not in content

    assert "another_layer_name" not in content


###############################################################################
# Test DeleteField()


def test_ogr_gpkg_35(tmp_vsimem, tmp_path):

    dbname = tmp_vsimem / "ogr_gpkg_35.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("bar_i_will_disappear", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("baz", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField("foo", "fooval")
    f.SetField("bar_i_will_disappear", "barval")
    f.SetField("baz", "bazval")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)

    lyr_nonspatial = ds.CreateLayer("test_nonspatial", geom_type=ogr.wkbNone)
    lyr_nonspatial.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr_nonspatial.CreateField(ogr.FieldDefn("bar_i_will_disappear", ogr.OFTString))
    lyr_nonspatial.CreateField(ogr.FieldDefn("baz", ogr.OFTString))

    # Metadata
    lyr_nonspatial.SetMetadataItem("FOO", "BAR")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('column', 'test_nonspatial', 'bar_i_will_disappear', NULL, '2021-01-01T00:00:00.000Z', 1, NULL)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata VALUES (2, 'dataset','http://gdal.org','text/plain','bla')"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('column', 'test_nonspatial', 'bar_i_will_disappear', NULL, '2021-01-01T00:00:00.000Z', 2, NULL)"
    )

    f = ogr.Feature(lyr_nonspatial.GetLayerDefn())
    f.SetFID(10)
    f.SetField("foo", "fooval")
    f.SetField("bar_i_will_disappear", "barval")
    f.SetField("baz", "bazval")
    lyr_nonspatial.CreateFeature(f)

    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT gdc_tn UNIQUE (table_name, name)
)"""
    )
    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_column_constraints (
            constraint_name TEXT NOT NULL,
            constraint_type TEXT NOT NULL,
            value TEXT,
            min NUMERIC,
            min_is_inclusive BOOLEAN,
            max NUMERIC,
            max_is_inclusive BOOLEAN,
            description TEXT,
            CONSTRAINT gdcc_ntv UNIQUE (constraint_name,
            constraint_type, value))"""
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns VALUES('test', 'bar_i_will_disappear', 'bar_constraints', NULL, NULL, NULL, NULL)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('test', 'bar_i_will_disappear', 'extension_name', 'definition', 'scope')"
    )

    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    with gdal.quiet_errors():
        ret = lyr.DeleteField(-1)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    assert ret != 0

    assert lyr.DeleteField(1) == 0
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if (
        f.GetFID() != 10
        or f["foo"] != "fooval"
        or f["baz"] != "bazval"
        or f.GetGeometryRef().ExportToWkt() != "POLYGON ((0 0,0 1,1 1,0 0))"
    ):
        f.DumpReadable()
        pytest.fail()

    lyr.StartTransaction()
    ret = lyr_nonspatial.DeleteField(1)
    lyr.CommitTransaction()
    assert ret == 0
    lyr_nonspatial.ResetReading()
    f = lyr_nonspatial.GetNextFeature()
    if f.GetFID() != 10 or f["foo"] != "fooval" or f["baz"] != "bazval":
        f.DumpReadable()
        pytest.fail()

    ds.ExecuteSQL("VACUUM")

    ds = None

    assert validate(dbname, tmpdir=tmp_path)

    # Try on read-only dataset
    ds = ogr.Open(dbname)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_metadata WHERE id = 1")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_metadata WHERE id = 2")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName("test_nonspatial")
    assert lyr.GetMetadataItem("FOO") == "BAR"

    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        ret = lyr.DeleteField(0)
    assert ret != 0
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert "bar_i_will_disappear" not in content


###############################################################################
# Test AlterFieldDefn()


def test_ogr_gpkg_36(tmp_vsimem, tmp_path):

    dbname = tmp_vsimem / "ogr_gpkg_36.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("baz", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField("foo", "10.5")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    f = None

    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT gdc_tn UNIQUE (table_name, name)
)"""
    )
    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_column_constraints (
            constraint_name TEXT NOT NULL,
            constraint_type TEXT NOT NULL,
            value TEXT,
            min NUMERIC,
            min_is_inclusive BOOLEAN,
            max NUMERIC,
            max_is_inclusive BOOLEAN,
            description TEXT,
            CONSTRAINT gdcc_ntv UNIQUE (constraint_name,
            constraint_type, value))"""
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns VALUES('test', 'foo', 'constraint', NULL, NULL, NULL, NULL)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('test', 'foo', 'extension_name', 'definition', 'read-write')"
    )
    ds.ExecuteSQL("CREATE INDEX my_idx ON test(foo)")

    # Metadata
    lyr.SetMetadataItem("FOO", "BAR")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('column', 'test', 'foo', NULL, '2021-01-01T00:00:00.000Z', 1, NULL)"
    )

    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(-1, ogr.FieldDefn("foo"), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(1, ogr.FieldDefn("foo"), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(
            0, ogr.FieldDefn(lyr.GetGeometryColumn()), ogr.ALTER_ALL_FLAG
        )
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(
            0, ogr.FieldDefn(lyr.GetFIDColumn()), ogr.ALTER_ALL_FLAG
        )
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn("baz"), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    new_field_defn = ogr.FieldDefn("bar", ogr.OFTReal)
    new_field_defn.SetSubType(ogr.OFSTFloat32)
    new_field_defn.SetWidth(10)
    new_field_defn.SetDefault("5")

    # Schema only change
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0

    # Full table rewrite
    new_field_defn.SetNullable(False)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0

    # Full table rewrite
    new_field_defn.SetUnique(True)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0

    # Violation of not-null constraint
    new_field_defn = ogr.FieldDefn("baz", ogr.OFTString)
    new_field_defn.SetNullable(False)
    with gdal.quiet_errors():
        assert lyr.AlterFieldDefn(1, new_field_defn, ogr.ALTER_ALL_FLAG) != 0

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if (
        f.GetFID() != 10
        or f["bar"] != 10.5
        or f.GetGeometryRef().ExportToWkt() != "POLYGON ((0 0,0 1,1 1,0 0))"
    ):
        f.DumpReadable()
        pytest.fail()
    f = None

    # Just change the name, and run it outside an existing transaction
    lyr.StartTransaction()
    new_field_defn = ogr.FieldDefn("baw2", ogr.OFTString)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0
    lyr.CommitTransaction()

    # Just change the name, and run it under an existing transaction
    lyr.StartTransaction()
    new_field_defn = ogr.FieldDefn("baw", ogr.OFTString)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0
    lyr.CommitTransaction()

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if (
        f.GetFID() != 10
        or f["baw"] != "10.5"
        or f.GetGeometryRef().ExportToWkt() != "POLYGON ((0 0,0 1,1 1,0 0))"
    ):
        f.DumpReadable()
        pytest.fail()
    f = None

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'my_idx'")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    f = None
    ds.ReleaseResultSet(sql_lyr)

    ds.ExecuteSQL("VACUUM")

    ds = None

    assert validate(dbname, tmpdir=tmp_path)

    # Try on read-only dataset
    ds = ogr.Open(dbname)

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_data_columns WHERE table_name = 'test' AND column_name = 'baw'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_metadata_reference WHERE table_name = 'test' AND column_name = 'baw'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn("foo"), ogr.ALTER_ALL_FLAG)
    assert ret != 0
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert "foo" not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    # Test failed DB re-opening
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    # Unlink before AlterFieldDefn
    gdal.Unlink(dbname)
    with gdal.quiet_errors():
        new_field_defn = ogr.FieldDefn("bar")
        new_field_defn.SetNullable(False)
        ret = lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG)
    assert ret != 0
    with gdal.quiet_errors():
        ds = None


###############################################################################
# Test ReorderFields()


def test_ogr_gpkg_37(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_37.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("bar", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField("foo", "fooval")
    f.SetField("bar", "barval")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)

    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT gdc_tn UNIQUE (table_name, name)
)"""
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns VALUES('test', 'foo', 'constraint', NULL, NULL, NULL, NULL)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('test', 'foo', 'extension_name', 'definition', 'scope')"
    )
    ds.ExecuteSQL("CREATE INDEX my_idx_foo ON test(foo)")
    ds.ExecuteSQL("CREATE INDEX my_idx_bar ON test(bar)")

    assert lyr.TestCapability(ogr.OLCReorderFields) == 1

    with gdal.quiet_errors():
        ret = lyr.ReorderFields([-1, -1])
    assert ret != 0

    assert lyr.ReorderFields([1, 0]) == 0

    lyr.ResetReading()
    assert (
        lyr.GetLayerDefn().GetFieldIndex("foo") == 1
        and lyr.GetLayerDefn().GetFieldIndex("bar") == 0
    )
    f = lyr.GetNextFeature()
    if (
        f.GetFID() != 10
        or f["foo"] != "fooval"
        or f["bar"] != "barval"
        or f.GetGeometryRef().ExportToWkt() != "POLYGON ((0 0,0 1,1 1,0 0))"
    ):
        f.DumpReadable()
        pytest.fail()

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'my_idx_foo' OR name = 'my_idx_bar'"
    )
    assert sql_lyr.GetFeatureCount() == 2
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    # Try on read-only dataset
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        ret = lyr.ReorderFields([1, 0])
    assert ret != 0
    ds = None


###############################################################################
# Test GetExtent() and RECOMPUTE EXTENT ON


@pytest.mark.parametrize("spatial_index", ("YES", "NO"))
def test_ogr_gpkg_38(tmp_vsimem, spatial_index):

    dbname = tmp_vsimem / "ogr_gpkg_38.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbLineString, options={"SPATIAL_INDEX": spatial_index}
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"))
    lyr.CreateFeature(f)
    ds = None

    # Simulate that extent is not recorded
    ds = ogr.Open(dbname, update=1)
    ds.ExecuteSQL(
        "UPDATE gpkg_contents SET min_x = NULL, min_y = NULL, max_x = NULL, max_y = NULL"
    )
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    # Test that we can compute the extent of a layer that has none registered in gpkg_contents
    extent = lyr.GetExtent(force=1)
    assert extent == (1, 3, 2, 4)
    sql_lyr = ds.ExecuteSQL("SELECT min_x, min_y, max_x, max_y FROM gpkg_contents")
    f = sql_lyr.GetNextFeature()
    if f["min_x"] != 1 or f["min_y"] != 2 or f["max_x"] != 3 or f["max_y"] != 4:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    extent = lyr.GetExtent(force=0)
    assert extent == (1, 3, 2, 4)

    # Modify feature
    f = lyr.GetFeature(1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING (-1 -2,-3 -4)"))
    lyr.SetFeature(f)

    # The extent has grown
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, 3.0, -4.0, 4.0)

    ds.ExecuteSQL("RECOMPUTE EXTENT ON test")
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, -1.0, -4.0, -2.0)
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, -1.0, -4.0, -2.0)
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    # Delete last feature
    lyr.DeleteFeature(1)

    # This should cancel NULLify the extent in gpkg_contents
    ds.ExecuteSQL("RECOMPUTE EXTENT ON test")
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    ds = None


###############################################################################
# Test checking of IDENTIFIER unicity


def test_ogr_gpkg_39(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_39.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)

    ds.CreateLayer("test")

    lyr = ds.CreateLayer(
        "test_with_explicit_identifier", options=["IDENTIFIER=explicit_identifier"]
    )
    assert lyr is not None

    # Allow overwriting
    lyr = ds.CreateLayer(
        "test_with_explicit_identifier",
        options=["IDENTIFIER=explicit_identifier", "OVERWRITE=YES"],
    )
    assert lyr is not None

    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test2", options=["IDENTIFIER=test"])
    assert lyr is None

    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test2", options=["IDENTIFIER=explicit_identifier"])
    assert lyr is None

    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'some_table', 'another_identifier', 'some_data_type' )"
    )
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("test2", options=["IDENTIFIER=another_identifier"])
    assert lyr is None
    ds = None


###############################################################################
# Run creating a non-spatial layer that is registered as 'attributes' and
# read it back


def test_ogr_gpkg_40(tmp_vsimem, tmp_path):

    dbname = tmp_vsimem / "ogr_gpkg_40.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    ds.CreateLayer("aspatial", geom_type=ogr.wkbNone)
    ds = None

    ds = ogr.Open(dbname)
    assert ds.GetLayerCount() == 1
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert validate(dbname, tmpdir=tmp_path), "validation failed"


###############################################################################
# Test tables without integer primary key (#6799), and unrecognized column type


def test_ogr_gpkg_41(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_41.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    ds.ExecuteSQL("CREATE TABLE foo (mycol VARCHAR_ILLEGAL)")
    ds.ExecuteSQL("INSERT INTO foo VALUES ('myval')")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name,data_type,identifier,description,last_change,srs_id) VALUES ('foo','attributes','foo','','',0)"
    )
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    if f["mycol"] != "myval" or f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        f = lyr.GetFeature(1)
    if f["mycol"] != "myval" or f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test feature_count


def foo_has_trigger(ds):
    sql_lyr = ds.ExecuteSQL(
        "SELECT COUNT(*) FROM sqlite_master WHERE name = 'trigger_insert_feature_count_foo'",
        dialect="DEBUG",
    )
    f = sql_lyr.GetNextFeature()
    has_trigger = f.GetField(0) == 1
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return has_trigger


def get_feature_count_from_gpkg_contents(ds):
    sql_lyr = ds.ExecuteSQL(
        "SELECT feature_count FROM gpkg_ogr_contents", dialect="DEBUG"
    )
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return val


def test_ogr_gpkg_42(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_42.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("i", ogr.OFTInteger))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    assert get_feature_count_from_gpkg_contents(ds) == 5
    assert foo_has_trigger(ds)
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) != 0
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)

    # Has been invalidated for now
    assert get_feature_count_from_gpkg_contents(ds) is None

    assert not foo_has_trigger(ds)

    fc = lyr.GetFeatureCount()
    assert fc == 6

    ds.ExecuteSQL("DELETE FROM foo WHERE i = 1")

    assert foo_has_trigger(ds)

    assert get_feature_count_from_gpkg_contents(ds) == 5

    fc = lyr.GetFeatureCount()
    assert fc == 5

    assert get_feature_count_from_gpkg_contents(ds) == 5

    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert get_feature_count_from_gpkg_contents(ds) is None
    assert lyr.SyncToDisk() == ogr.OGRERR_NONE

    assert get_feature_count_from_gpkg_contents(ds) == 6

    assert lyr.DeleteFeature(f.GetFID()) == ogr.OGRERR_NONE
    assert get_feature_count_from_gpkg_contents(ds) is None

    ds.ExecuteSQL("INSERT OR REPLACE INTO foo (fid) VALUES (1)")

    assert get_feature_count_from_gpkg_contents(ds) == 5

    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds.ExecuteSQL("UPDATE gpkg_ogr_contents SET feature_count = NULL")
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    assert get_feature_count_from_gpkg_contents(ds) is None
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds = None

    ds = ogr.Open(dbname, update=1)
    assert get_feature_count_from_gpkg_contents(ds) == 5

    # So as to test that we really read from gpkg_ogr_contents
    ds.ExecuteSQL("UPDATE gpkg_ogr_contents SET feature_count = 5000")

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 5000

    # Test renaming
    assert lyr.TestCapability(ogr.OLCRename) == 1
    assert lyr.Rename("bar") == ogr.OGRERR_NONE
    assert lyr.GetDescription() == "bar"
    assert lyr.GetLayerDefn().GetName() == "bar"
    with gdal.quiet_errors():
        assert lyr.Rename("bar") != ogr.OGRERR_NONE
    with gdal.quiet_errors():
        assert lyr.Rename("gpkg_ogr_contents") != ogr.OGRERR_NONE
    assert lyr.GetDescription() == "bar"
    assert lyr.GetLayerDefn().GetName() == "bar"

    ds = None
    ds = ogr.Open(dbname, update=1)
    sql_lyr = ds.ExecuteSQL(
        "SELECT feature_count FROM gpkg_ogr_contents WHERE table_name = 'bar'",
        dialect="DEBUG",
    )
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert val == 5000

    # Test layer deletion
    ds.DeleteLayer(0)
    sql_lyr = ds.ExecuteSQL(
        "SELECT feature_count FROM gpkg_ogr_contents", dialect="DEBUG"
    )
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Test without feature_count column
    ds = gdaltest.gpkg_dr.CreateDataSource(
        dbname, options=["ADD_GPKG_OGR_CONTENTS=FALSE"]
    )
    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("i", ogr.OFTInteger))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(dbname, update=1)

    # Check that feature_count column is missing
    sql_lyr = ds.ExecuteSQL("PRAGMA table_info(gpkg_contents)")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 10

    assert not foo_has_trigger(ds)

    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    fc = lyr.GetFeatureCount()
    assert fc == 5
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 6
    ds.ExecuteSQL("DELETE FROM foo WHERE i = 1")
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds = None


###############################################################################
# Test limitations on number of tables


def test_ogr_gpkg_43(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_43.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    ds.StartTransaction()
    for i in range(1001):
        ds.ExecuteSQL(
            "INSERT INTO gpkg_contents (table_name, data_type, identifier) "
            + "VALUES ('tiles%d', 'tiles', 'tiles%d')" % (i + 1, i + 1)
        )
        ds.ExecuteSQL(
            "INSERT INTO gpkg_tile_matrix_set VALUES "
            + "('tiles%d', 0, 440720, 3750120, 441920, 3751320)" % (i + 1)
        )
    for i in range(1001):
        ds.ExecuteSQL(
            "INSERT INTO gpkg_contents (table_name, data_type, identifier) "
            + "VALUES ('attr%d', 'attributes', 'attr%d')" % (i + 1, i + 1)
        )
        ds.ExecuteSQL(
            "CREATE TABLE attr%d (id INTEGER PRIMARY KEY AUTOINCREMENT)" % (i + 1)
        )
    ds.CommitTransaction()
    ds = None

    ds = gdal.OpenEx(dbname)
    assert len(ds.GetMetadata_List("SUBDATASETS")) == 2 * 1001
    assert ds.GetLayerCount() == 1001

    with gdaltest.config_option("OGR_TABLE_LIMIT", "1000"):
        with gdal.quiet_errors():
            ds = gdal.OpenEx(dbname)
            assert len(ds.GetMetadata_List("SUBDATASETS")) == 2 * 1000
            assert ds.GetLayerCount() == 1000
    ds = None


###############################################################################
# Test GeoPackage without metadata table


def test_ogr_gpkg_METADATA_TABLES_NO(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_METADATA_TABLES_NO.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename, options=["METADATA_TABLES=NO"])
    ds.CreateLayer("foo")
    ds.SetMetadataItem("FOO", "BAR")  # will not be written
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    md = ds.GetMetadata()
    assert md == {}
    md = ds.GetLayer(0).GetMetadata()
    assert md == {}
    with ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'"
    ) as sql_lyr:
        fc = sql_lyr.GetFeatureCount()
    assert fc == 0
    ds = None

    ds = ogr.Open(filename, update=1)
    ds.SetMetadataItem("FOO", "BAR")
    ds = None

    ds = ogr.Open(filename)
    md = ds.GetMetadata()
    assert md == {"FOO": "BAR"}
    ds = None


###############################################################################
# Test GeoPackage with forced metadata table


def test_ogr_gpkg_METADATA_TABLES_YES(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_METADATA_TABLES_YES.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename, options=["METADATA_TABLES=YES"])
    ds.CreateLayer("foo")
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    with ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'"
    ) as sql_lyr:
        fc = sql_lyr.GetFeatureCount()
    assert fc == 1
    ds = None


###############################################################################
# Test GeoPackage with automatic metadata table creation


def test_ogr_gpkg_METADATA_TABLES_AUTO(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_METADATA_TABLES_AUTO.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    lyr.SetMetadataItem("foo", "bar")
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    with ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'"
    ) as sql_lyr:
        fc = sql_lyr.GetFeatureCount()
    assert fc == 1
    md = ds.GetLayer(0).GetMetadata()
    assert md == {"foo": "bar"}
    ds = None


###############################################################################
# Test GeoPackage with automatic metadata table creation


def test_ogr_gpkg_METADATA_TABLES_AUTO_not_needed(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_METADATA_TABLES_AUTO_not_needed.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.CreateLayer("foo")
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    with ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'"
    ) as sql_lyr:
        fc = sql_lyr.GetFeatureCount()
    assert fc == 0
    ds = None


###############################################################################
# Test non conformant GeoPackage: table with non INTEGER PRIMARY KEY


def test_ogr_gpkg_45(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_45.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    ds.ExecuteSQL(
        "CREATE TABLE test (a INTEGER, b INTEGER, CONSTRAINT pkid_constraint PRIMARY KEY (a, b))"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'test', 'test', 'attributes' )"
    )
    ds = None
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    assert lyr.GetFIDColumn() == ""
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    ds = None


###############################################################################
# Test spatial view and spatial index


def test_ogr_gpkg_46(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_46.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("foo")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    lyr.CreateFeature(f)
    # Note: this definition of a view is non conformant with GPKG 1.3 clarifications on views
    ds.ExecuteSQL(
        "CREATE VIEW my_view AS SELECT geom AS my_geom, fid AS my_fid FROM foo"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view', 'my_view', 'features', 0 )"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view', 'my_geom', 'GEOMETRY', 0, 0, 0)"
    )

    # Note: this definition of a view is non conformant with GPKG 1.3 clarifications on views
    ds.ExecuteSQL(
        "CREATE VIEW my_view2 AS SELECT geom, fid AS OGC_FID, 'bla' as another_column FROM foo"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view2', 'my_view2', 'features', 0 )"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view2', 'geom', 'GEOMETRY', 0, 0, 0)"
    )

    ds.ExecuteSQL(
        "CREATE VIEW my_view3 AS SELECT a.fid * 10000 + b.fid as my_fid, a.fid as fid1, a.geom, b.fid as fid2 FROM foo a, foo b"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view3', 'my_view3', 'features', 0 )"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view3', 'geom', 'GEOMETRY', 0, 0, 0)"
    )

    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayerByName("my_view")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == "my_geom"

    # Operations not valid on a view
    with gdal.quiet_errors():
        ds.ReleaseResultSet(
            ds.ExecuteSQL("SELECT CreateSpatialIndex('my_view', 'my_geom')")
        )
        ds.ReleaseResultSet(
            ds.ExecuteSQL("SELECT DisableSpatialIndex('my_view', 'my_geom')")
        )
        lyr.AlterFieldDefn(0, lyr.GetLayerDefn().GetFieldDefn(0), ogr.ALTER_ALL_FLAG)
        lyr.DeleteField(0)
        lyr.ReorderFields([0])
        lyr.CreateField(ogr.FieldDefn("bar"))

    # Check if spatial index is recognized
    sql_lyr = ds.ExecuteSQL("SELECT HasSpatialIndex('my_view', 'my_geom')")
    f = sql_lyr.GetNextFeature()
    has_spatial_index = f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    if not has_spatial_index:
        ds = None
        pytest.skip("SQLite likely built without SQLITE_HAS_COLUMN_METADATA")

    # Effectively test spatial index
    lyr.SetSpatialFilterRect(-0.5, -0.5, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is None

    # View with FID in non-first position
    lyr = ds.GetLayerByName("my_view2")
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetFIDColumn() == "OGC_FID"
    f = lyr.GetNextFeature()
    if f.GetFID() != 1 or f.GetField(0) != "bla":
        f.DumpReadable()
        pytest.fail()

    # View with FID in first position
    lyr = ds.GetLayerByName("my_view3")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10001
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10002
    f2 = lyr.GetFeature(10002)
    assert f.Equal(f2)
    ds = None


###############################################################################
# Test corner case of Identify()


def test_ogr_gpkg_47(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    gdaltest.gpkg_dr.CreateDataSource(dbname)
    # Set wrong application_id
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack("B" * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(dbname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.config_option("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "NO"):
        ogr.Open("/vsimem/ogr_gpkg_47.gpkg")
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_gpkg_47a(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    gdaltest.gpkg_dr.CreateDataSource(dbname, options={"VERSION": "1.2"})
    # Set wrong user_version
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack("B" * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(dbname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ""
    ds = None

    gdal.ErrorReset()
    with gdal.config_option("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "NO"):
        ogr.Open(dbname)
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_gpkg_47b(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    # Set GPKG 1.2.1
    gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=1.2"])
    # Set user_version
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 60, 0)
    assert struct.unpack(">I", gdal.VSIFReadL(4, 1, fp))[0] == 10200
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack(">I", 10201), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    gdal.ErrorReset()
    ds = ogr.Open(dbname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.ErrorReset()
    with gdal.config_option("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "NO"):
        ogr.Open(dbname)
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_gpkg_47c(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    # Set GPKG 1.3.0
    gdaltest.gpkg_dr.CreateDataSource(dbname, options={"VERSION": "1.3"})
    # Check user_version
    fp = gdal.VSIFOpenL(dbname, "rb")
    gdal.VSIFSeekL(fp, 60, 0)
    assert struct.unpack(">I", gdal.VSIFReadL(4, 1, fp))[0] == 10300
    gdal.VSIFCloseL(fp)

    gdal.ErrorReset()
    ds = ogr.Open(dbname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.ErrorReset()
    with gdal.config_option("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "NO"):
        ogr.Open(dbname)
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_gpkg_47d(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    # Set GPKG 1.99.0
    gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=1.2"])
    # Set user_version
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack(">I", 19900), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(dbname, update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.config_option("GPKG_WARN_UNRECOGNIZED_APPLICATION_ID", "NO"):
        ogr.Open(dbname)
    assert gdal.GetLastErrorMsg() == ""


def test_ogr_gpkg_47e(tmp_vsimem):

    dbname = tmp_vsimem / ".cur_input"

    # Just for the sake of coverage testing in DEBUG mode
    with gdal.quiet_errors():
        gdaltest.gpkg_dr.CreateDataSource(dbname)
    # Set wrong application_id
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack("B" * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)
    ds = ogr.Open(dbname)

    assert ds is not None


def test_ogr_gpkg_47f(tmp_vsimem):

    dbname = tmp_vsimem / ".cur_input"

    with gdal.quiet_errors():
        gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=1.2"])

    # Set wrong user_version
    fp = gdal.VSIFOpenL(dbname, "rb+")
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack("B" * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)
    ds = ogr.Open(dbname)

    assert ds is None


def test_ogr_gpkg_47g(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_47.gpkg"

    # Test reading in a zip
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    ds.CreateLayer("foo")
    ds = None
    fp = gdal.VSIFOpenL(dbname, "rb")
    content = gdal.VSIFReadL(1, 1000000, fp)
    gdal.VSIFCloseL(fp)
    fzip = gdal.VSIFOpenL(f"/vsizip/{tmp_vsimem}/ogr_gpkg_47.zip", "wb")
    fp = gdal.VSIFOpenL(f"/vsizip/{tmp_vsimem}/ogr_gpkg_47.zip/my.gpkg", "wb")
    gdal.VSIFWriteL(content, 1, len(content), fp)
    gdal.VSIFCloseL(fp)
    gdal.VSIFCloseL(fzip)
    ds = ogr.Open(f"/vsizip/{tmp_vsimem}/ogr_gpkg_47.zip")
    assert ds.GetDriver().GetName() == "GPKG"
    ds = None


###############################################################################
# Test insertion of features with unset fields


def test_ogr_gpkg_48(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_48.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("foo")
    lyr.CreateField(ogr.FieldDefn("a"))
    lyr.CreateField(ogr.FieldDefn("b"))
    lyr.CreateField(ogr.FieldDefn("c"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("a", "a")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("b", "b")
    f.SetField("c", "c")
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField("a") != "a" or f.GetField("b") is not None:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField("b") != "b" or f.GetField("c") != "c" or f.GetField("a") is not None:
        f.DumpReadable()
        pytest.fail()

    # No geom field, one single field with default value
    lyr = ds.CreateLayer("default_field_no_geom", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetDefault("'x'")
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == 0
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField("foo") != "x":
        f.DumpReadable()
        pytest.fail()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    assert lyr.SetFeature(f) == 0
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField("foo") != "x":
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test CreateGeomField() on a attributes layer


def test_ogr_gpkg_49(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_49.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)

    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbNone, options=["ASPATIAL_VARIANT=GPKG_ATTRIBUTES"]
    )

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    assert lyr.CreateGeomField(field_defn) == 0
    ds = None


###############################################################################
# Test CRS_WKT_EXTENSION creation option


@pytest.mark.parametrize("gpkg_version", ["1.2", "1.4"])
def test_ogr_gpkg_CRS_WKT_EXTENSION(tmp_vsimem, tmp_path, gpkg_version):

    dbname = tmp_vsimem / "ogr_gpkg_50.gpkg"

    gdaltest.gpkg_dr.CreateDataSource(
        dbname,
        options=["CRS_WKT_EXTENSION=YES", "VERSION=" + gpkg_version],
    )

    ds = ogr.Open(dbname, update=1)
    srs32631 = osr.SpatialReference()
    srs32631.ImportFromEPSG(32631)
    ds.CreateLayer("test", srs=srs32631)

    # No authority node
    srs_without_org = osr.SpatialReference()
    srs_without_org.SetFromUserInput(
        """GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]"""
    )
    lyr = ds.CreateLayer("without_org", srs=srs_without_org)

    ds = None

    assert validate(dbname, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer("test")
    assert lyr.GetSpatialRef().IsSame(srs32631)
    lyr = ds.GetLayer("without_org")
    assert lyr.GetSpatialRef().IsSame(srs_without_org)
    with ds.ExecuteSQL(
        "SELECT definition_12_063 FROM gpkg_spatial_ref_sys WHERE srs_id = 32631"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
    assert f.GetField(0).startswith('PROJCRS["WGS 84 / UTM zone 31N"')

    with ds.ExecuteSQL("PRAGMA table_info(gpkg_spatial_ref_sys)") as sql_lyr:
        has_epoch = False
        for f in sql_lyr:
            if f["name"] == "epoch":
                has_epoch = True
        if gpkg_version == "1.2":
            assert not has_epoch
        else:
            assert has_epoch
    ds = None


###############################################################################
# Test opening a .gpkg.sql file


def test_ogr_gpkg_51():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != "YES":
        pytest.skip()

    ds = ogr.Open("data/gpkg/poly.gpkg.sql")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None


###############################################################################
# Test opening a .gpkg file


def test_ogr_gpkg_52():

    ds = ogr.Open("data/gpkg/poly_non_conformant.gpkg")
    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert f is not None


###############################################################################
# Test opening a .gpkg file with inconsistency regarding table case (#6916)


def test_ogr_gpkg_53():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != "YES":
        pytest.skip()

    ds = ogr.Open("data/gpkg/poly_inconsistent_case.gpkg.sql")
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(
            test_cli_utilities.get_test_ogrsf_path()
            + " data/gpkg/poly_inconsistent_case.gpkg.sql"
        )

        assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test editing of a database with 2 layers (https://issues.qgis.org/issues/17034)


def test_ogr_gpkg_54(tmp_path):

    # Must be on a real file system to demonstrate potential locking
    # issue
    tmpfile = tmp_path / "ogr_gpkg_54.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmpfile)
    lyr = ds.CreateLayer("layer1", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = None
    lyr = ds.CreateLayer("layer2", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds1 = ogr.Open(tmpfile, update=1)
    ds2 = ogr.Open(tmpfile, update=1)

    lyr1 = ds1.GetLayer(0)
    lyr2 = ds2.GetLayer(1)

    f1 = lyr1.GetFeature(1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr1.SetFeature(f1)

    f2 = lyr2.GetFeature(1)
    f2.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    lyr2.SetFeature(f2)

    f1 = lyr1.GetFeature(1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt("POINT (5 6)"))
    lyr1.SetFeature(f1)

    f2 = lyr2.GetFeature(1)
    f2.SetGeometry(ogr.CreateGeometryFromWkt("POINT (7 8)"))
    lyr2.SetFeature(f2)

    ds1 = None
    ds2 = None

    ds = ogr.Open(tmpfile)
    lyr1 = ds.GetLayer(0)
    f = lyr1.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (5 6)":
        f.DumpReadable()
        pytest.fail()
    lyr2 = ds.GetLayer(1)
    f = lyr2.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (7 8)":
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test inserting geometries incompatible with declared layer geometry type


def test_ogr_gpkg_55(tmp_vsimem):

    tmpfile = tmp_vsimem / "ogr_gpkg_55.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmpfile)
    lyr = ds.CreateLayer("layer1", geom_type=ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != "", "should have warned"
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() == "", "should NOT have warned"
    f = None
    ds = None


###############################################################################
# Test FID identification on SQL result layer


def test_ogr_gpkg_56(tmp_vsimem):

    ds = gdal.VectorTranslate(
        tmp_vsimem / "ogr_gpkg_56.gpkg", "data/poly.shp", format="GPKG"
    )
    lyr = ds.ExecuteSQL(
        "select a.fid as fid1, b.fid as fid2 from poly a, poly b order by fid1, fid2"
    )
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f.GetField("fid1") != 1 or f.GetField("fid2") != 2:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(lyr)
    ds = None


###############################################################################
# Test creation of a field which is the same as the FID column


def test_ogr_gpkg_creation_fid(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_creation_fid.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)

    lyr = ds.CreateLayer("fid_integer")
    assert lyr.CreateField(ogr.FieldDefn("fid", ogr.OFTInteger)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 12
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 12
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 13
    f.SetFID(13)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 13
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    lyr = ds.CreateLayer("fid_integer64")
    assert lyr.CreateField(ogr.FieldDefn("fid", ogr.OFTInteger64)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890123
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890123
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890124
    f.SetFID(1234567890124)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890124
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890125
    f.SetFID(1)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    # Simulates the situation of GeoPackage ---QGIS---> Shapefile --> GeoPackage
    # See https://github.com/qgis/QGIS/pull/43118
    lyr = ds.CreateLayer("fid_real")
    fld_defn = ogr.FieldDefn("fid", ogr.OFTReal)
    fld_defn.SetWidth(20)
    fld_defn.SetPrecision(0)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890123
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890123
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890124
    f.SetFID(1234567890124)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890124
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890123.5
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["fid"] = 1234567890125
    f.SetFID(1)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    ds = None


###############################################################################
# Test opening a corrupted gpkg with duplicated layer names


def test_ogr_gpkg_57(tmp_vsimem):

    out_filename = tmp_vsimem / "test_ogr_gpkg_57.gpkg"
    ogr.GetDriverByName("GPKG").CreateDataSource(out_filename)

    ds = ogr.Open(out_filename, update=1)
    ds.ExecuteSQL("DROP TABLE gpkg_contents")
    ds.ExecuteSQL(
        "CREATE TABLE gpkg_contents (table_name,data_type,identifier,description,last_change,min_x, min_y,max_x, max_y,srs_id)"
    )
    ds.ExecuteSQL(
        """INSERT INTO "gpkg_contents" VALUES('poly','features','poly','','',NULL,NULL,NULL,NULL,0)"""
    )
    ds.ExecuteSQL(
        """INSERT INTO "gpkg_contents" VALUES('poly','features','poly','','',NULL,NULL,NULL,NULL,0)"""
    )
    ds.ExecuteSQL(
        """INSERT INTO "gpkg_geometry_columns" VALUES('poly','geom','POLYGON',0,0,0)"""
    )
    ds.ExecuteSQL("""CREATE TABLE "poly"("fid" INTEGER PRIMARY KEY, "geom" POLYGON)""")
    ds = None

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(out_filename)
    assert ds.GetLayerCount() == 1, "bad layer count"
    assert gdal.GetLastErrorMsg() != ""
    ds = None


###############################################################################
# Test opening a non-standard GeoPackage with multiple geometry columns


def test_ogr_gpkg_multiple_geom_columns(tmp_vsimem):

    out_filename = tmp_vsimem / "test_ogr_gpkg_multiple_geom_columns.gpkg"
    ogr.GetDriverByName("GPKG").CreateDataSource(out_filename)

    ds = ogr.Open(out_filename, update=1)
    ds.ExecuteSQL("DROP TABLE gpkg_geometry_columns")
    # Modified gpkg_geometry_columns definition with a UNIQUE constraint on both (table_name, column_name)
    ds.ExecuteSQL(
        """CREATE TABLE gpkg_geometry_columns (table_name TEXT NOT NULL,column_name TEXT NOT NULL,geometry_type_name TEXT NOT NULL,srs_id INTEGER NOT NULL,z TINYINT NOT NULL,m TINYINT NOT NULL,CONSTRAINT pk_geom_cols PRIMARY KEY (table_name, column_name),CONSTRAINT uk_gc_table_name_column_name UNIQUE (table_name, column_name),CONSTRAINT fk_gc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name),CONSTRAINT fk_gc_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys (srs_id));"""
    )
    ds.ExecuteSQL(
        """CREATE TABLE "test" ( "ogc_fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "poly" POLYGON, "pt" POINT, "area" REAL, "eas_id" INTEGER, "prfedea" TEXT(16))"""
    )
    ds.ExecuteSQL(
        "INSERT INTO test VALUES(1,X'4750000300000000000000401f401d41000000e05e511d4100000080322c5241000000001d2d52410103000000010000001b000000000000c01a481d4100000080072d5241000000e0814b1d41000000001d2d524100000040c44b1d41000000000f2d5241000000002c4c1d41000000a0002d524100000000774d1d41000000c0072d5241000000a0c44e1d4100000080112d52410000002008501d41000000c0172d5241000000e05e511d4100000020dd2c5241000000405e511d4100000040cf2c524100000000f0501d41000000c0ba2c52410000008084501d4100000020af2c524100000040a94f1d4100000000a42c524100000080744e1d41000000a09a2c524100000040014f1d41000000c0852c5241000000e0e04d1d4100000020872c524100000040f8441d41000000e0432c5241000000c012441d4100000080322c524100000000ff431d4100000020362c5241000000004b431d4100000080552c52410000000030431d41000000c05d2c5241000000c09b421d4100000000712c524100000080d6411d4100000080912c52410000008027411d4100000040b22c5241000000401f401d4100000040d62c5241000000a043441d4100000060f02c524100000060aa461d4100000080ff2c5241000000c01a481d4100000080072d5241',X'4750000100000000010100000000000000804b1d41000000001d2d5241',NULL,170,NULL);"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents VALUES('test','features','test',NULL,'2023-04-21T13:53:59.009Z',478315.53124999999998,4762880.5,481645.3125,4765610.4999999999998,0);"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns VALUES('test','poly','POLYGON',-1,0,0);"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns VALUES('test','pt','POINT',4326,0,0);"
    )
    ds = None

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = ogr.Open(out_filename)
    assert gdal.GetLastErrorMsg() != ""
    assert ds.GetLayerCount() == 2

    lyr = ds.GetLayerByName("test (poly)")
    assert lyr.GetGeomType() == ogr.wkbPolygon
    assert lyr.GetGeometryColumn() == "poly"
    assert lyr.GetSpatialRef().GetName() == "Undefined Cartesian SRS"
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert f["eas_id"] == 170
    assert f.GetGeometryRef().ExportToWkt().startswith("POLYGON ((479750")

    lyr = ds.GetLayerByName("test (pt)")
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert lyr.GetGeometryColumn() == "pt"
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert f["eas_id"] == 170
    assert f.GetGeometryRef().ExportToWkt() == "POINT (479968 4764788)"

    ds = None

    gdal.Unlink(out_filename)


###############################################################################
# Test overwriting a layer


def test_ogr_gpkg_58(tmp_vsimem):

    out_filename = tmp_vsimem / "ogr_gpkg_58.gpkg"
    gdal.VectorTranslate(out_filename, "data/poly.shp", format="GPKG")
    gdal.VectorTranslate(
        out_filename, "data/poly.shp", format="GPKG", accessMode="overwrite"
    )

    ds = ogr.Open(out_filename)
    sql_lyr = ds.ExecuteSQL("SELECT HasSpatialIndex('poly', 'geom')")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(out_filename)


###############################################################################
# Test CreateSpatialIndex()


def test_ogr_gpkg_59(tmp_vsimem):

    out_filename = tmp_vsimem / "ogr_gpkg_59.gpkg"
    gdal.VectorTranslate(
        out_filename,
        "data/poly.shp",
        format="GPKG",
        layerCreationOptions=["SPATIAL_INDEX=NO"],
    )

    ds = ogr.Open(out_filename, update=1)
    sql_lyr = ds.ExecuteSQL("SELECT CreateSpatialIndex('poly', 'geom')")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test savepoints


def test_ogr_gpkg_savepoint(tmp_vsimem):

    filename = tmp_vsimem / "ogr_gpkg_savepoint.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "foo"
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    ds.StartTransaction()
    ds.ExecuteSQL("SAVEPOINT pt")
    lyr.DeleteFeature(1)
    ds.ExecuteSQL("ROLLBACK TO SAVEPOINT pt")
    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "bar"
    lyr.CreateFeature(f)
    ds.CommitTransaction()
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    ds = None


###############################################################################
# Test that we don't open file handles behind the back of sqlite3


def test_ogr_gpkg_wal(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    # needs to be a real file
    filename = tmp_path / "ogr_gpkg_wal.gpkg"

    with gdaltest.config_option("OGR_SQLITE_JOURNAL", "WAL"):
        ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.CreateLayer("foo")
    ds = None

    ds = ogr.Open(filename, update=1)
    assert filename.with_suffix(".gpkg-wal").exists()

    # Re-open in read-only mode
    ds_ro = ogr.Open(filename)
    ds_ro.GetName()
    assert filename.with_suffix(".gpkg-wal").exists()

    # Test external process to read the file
    gdaltest.runexternal(f"{test_cli_utilities.get_ogrinfo_path()} {filename}")

    # The file must still exist
    assert filename.with_suffix(".gpkg-wal").exists()

    ds = None
    ds_ro = None


###############################################################################
# Test NOLOCK open option


def test_ogr_gpkg_nolock(tmp_path):
    def get_nolock(ds):
        sql_lyr = ds.ExecuteSQL("SELECT nolock", dialect="DEBUG")
        f = sql_lyr.GetNextFeature()
        res = True if f[0] == 1 else False
        ds.ReleaseResultSet(sql_lyr)
        return res

    # needs to be a real file
    filename = tmp_path / "test_ogr_gpkg_#_nolock.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None
    ds = None

    # Special case on Windows for files that start with drive letters
    full_filename = (tmp_path / "test_ogr_gpkg_#_nolock.gpkg").absolute()
    ds = gdal.OpenEx(full_filename, gdal.OF_VECTOR, open_options=["NOLOCK=YES"])
    assert ds
    ds = None

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR, open_options=["NOLOCK=YES"])
    assert ds
    assert get_nolock(ds)

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ds2 = ogr.Open(filename, update=1)
    lyr2 = ds2.GetLayer(0)
    f = ogr.Feature(lyr2.GetLayerDefn())
    # Without lockless mode on ds, this would timeout and fail
    assert lyr2.CreateFeature(f) == ogr.OGRERR_NONE
    f = None
    ds2 = None
    ds = None

    # Lockless mode should NOT be honored by GDAL in update mode
    ds = gdal.OpenEx(
        filename, gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=["NOLOCK=YES"]
    )
    assert ds
    assert not get_nolock(ds)
    ds = None

    # Now turn on WAL
    ds = ogr.Open(filename, update=1)
    ds.ReleaseResultSet(ds.ExecuteSQL("PRAGMA journal_mode = WAL"))
    ds = None

    # Lockless mode should NOT be honored by GDAL on a WAL enabled file
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR, open_options=["NOLOCK=YES"])
    assert ds
    assert not get_nolock(ds)
    ds = None

    ds = gdal.OpenEx(
        "/vsizip/data/gpkg/poly.gpkg.zip/poly.gpkg",
        gdal.OF_VECTOR,
        open_options=["NOLOCK=YES"],
    )
    assert ds

    ds = gdal.OpenEx(
        "/vsizip/" + os.getcwd() + "/data/gpkg/poly.gpkg.zip/poly.gpkg",
        gdal.OF_VECTOR,
        open_options=["NOLOCK=YES"],
    )
    assert ds


###############################################################################
# Run test_ogrsf


@pytest.mark.usefixtures("tpoly", "tbl_linestring")
def test_ogr_gpkg_test_ogrsf(gpkg_ds):

    dbname = gpkg_ds.GetDescription()

    # Do integrity check first
    gpkg_ds = ogr.Open(dbname)
    sql_lyr = gpkg_ds.ExecuteSQL("PRAGMA integrity_check")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField(0) == "ok", "integrity check failed"
    gpkg_ds.ReleaseResultSet(sql_lyr)

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gpkg_ds.Close()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" {dbname} --config OGR_SQLITE_SYNCHRONOUS OFF"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f' {dbname} -sql "select * from tbl_linestring" --config OGR_SQLITE_SYNCHRONOUS OFF'
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test JSon subtype support


def test_ogr_gpkg_json(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "ogr_gpkg_json.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("test")

    fld_defn = ogr.FieldDefn("test_json", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON

    ds.ReleaseResultSet(ds.ExecuteSQL("SELECT 1 FROM test"))  # will crystalize

    fld_defn = ogr.FieldDefn("test2_json", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTJSON

    fld_defn = ogr.FieldDefn("test_string", ogr.OFTString)
    lyr.CreateField(fld_defn)

    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTJSON

    # Demote field from JSON
    new_defn = ogr.FieldDefn("test_was_json_now_string", ogr.OFTString)
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("test2_json"), new_defn, ogr.ALTER_ALL_FLAG
        )
        == 0
    )

    # Alter field to JSON
    new_defn = ogr.FieldDefn("test_was_string_now_json", ogr.OFTString)
    new_defn.SetSubType(ogr.OFSTJSON)
    assert (
        lyr.AlterFieldDefn(
            lyr.GetLayerDefn().GetFieldIndex("test_string"),
            new_defn,
            ogr.ALTER_ALL_FLAG,
        )
        == 0
    )

    # Delete JSON field
    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex("test_json")) == 0

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("test_was_json_now_string"))
        .GetSubType()
        == ogr.OFSTNone
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("test_was_string_now_json"))
        .GetSubType()
        == ogr.OFSTJSON
    )

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM gpkg_data_columns WHERE table_name = 'test'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    ds = None


###############################################################################
# Test invalid/non-standard content in records


def test_ogr_gpkg_invalid_values_in_records(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_invalid_date_content.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("test")

    fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("d", ogr.OFTDate)
    lyr.CreateField(fld_defn)

    for i in range(6):
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
    ds.ExecuteSQL("UPDATE test SET dt = 'foo' WHERE fid = 1")
    ds.ExecuteSQL("UPDATE test SET d = 'bar' WHERE fid = 2")
    ds.ExecuteSQL("UPDATE test SET dt = 3 WHERE fid = 3")
    ds.ExecuteSQL("UPDATE test SET d = 4 WHERE fid = 4")
    ds.ExecuteSQL("UPDATE test SET dt = '2020/01/21 12:34:56+01' WHERE fid = 5")
    ds.ExecuteSQL("UPDATE test SET d = '2020/01/21' WHERE fid = 6")

    lyr.ResetReading()

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == "Invalid content for record 1 in column dt: foo"
    assert not f.IsFieldSet("dt")

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == "Invalid content for record 2 in column d: bar"
    assert not f.IsFieldSet("d")

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == "Unexpected data type for record 3 in column dt"
    assert not f.IsFieldSet("dt")

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == "Unexpected data type for record 4 in column d"
    assert not f.IsFieldSet("d")

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert (
        gdal.GetLastErrorMsg()
        == "Non-conformant content for record 5 in column dt, 2020/01/21 12:34:56+01, successfully parsed"
    )
    assert f.IsFieldSet("dt")
    assert f["dt"] == "2020/01/21 12:34:56+01"

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert (
        gdal.GetLastErrorMsg()
        == "Non-conformant content for record 6 in column d, 2020/01/21, successfully parsed"
    )
    assert f.IsFieldSet("d")
    assert f["d"] == "2020/01/21"

    ds = None


###############################################################################
# Test creating a table with layer geometry type unknown/GEOMETRY and
# geometries of mixed dimensionality


def test_ogr_gpkg_mixed_dimensionality_unknown_layer_geometry_type(
    tmp_vsimem, tmp_path
):

    filename = (
        tmp_vsimem
        / "test_ogr_gpkg_mixed_dimensionality_unknown_layer_geometry_type.gpkg"
    )
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("test")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2 3)"))
    lyr.CreateFeature(f)

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbUnknown

    sql_lyr = ds.ExecuteSQL("SELECT z FROM gpkg_geometry_columns")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 2
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test fixing up wrong RTree update3 trigger from GeoPackage < 1.2.1


def test_ogr_gpkg_fixup_wrong_rtree_trigger():

    filename = "/vsimem/test_ogr_gpkg_fixup_wrong_rtree_trigger.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    ds.CreateLayer("test-with-dash")
    ds.CreateLayer("test2")
    ds = None
    with gdal.quiet_errors():
        ds = ogr.Open(filename, update=1)
        # inject wrong trigger on purpose with the wrong 'OF "geometry" ' part
        ds.ExecuteSQL('DROP TRIGGER "rtree_test-with-dash_geometry_update3"')
        wrong_trigger = 'CREATE TRIGGER "rtree_test-with-dash_geometry_update3" AFTER UPDATE OF "geometry" ON "test-with-dash" WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
        ds.ExecuteSQL(wrong_trigger)

        ds.ExecuteSQL("DROP TRIGGER rtree_test2_geometry_update3")
        # Test another potential variant (although not generated by OGR)
        wrong_trigger2 = 'CREATE TRIGGER "rtree_test2_geometry_update3" AFTER UPDATE OF   geometry    ON test2 WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
        ds.ExecuteSQL(wrong_trigger2)

        ds = None

    # Open in read-only mode
    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test-with-dash_geometry_update3'"
    )
    f = sql_lyr.GetNextFeature()
    sql = f["sql"]
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    assert sql == wrong_trigger

    # Open in update mode
    ds = ogr.Open(filename, update=1)
    sql_lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test-with-dash_geometry_update3'"
    )
    f = sql_lyr.GetNextFeature()
    sql = f["sql"]
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test2_geometry_update3'"
    )
    f = sql_lyr.GetNextFeature()
    sql2 = f["sql"]
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)
    assert (
        sql
        == 'CREATE TRIGGER "rtree_test-with-dash_geometry_update3" AFTER UPDATE ON "test-with-dash" WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
    )
    assert (
        sql2
        == 'CREATE TRIGGER "rtree_test2_geometry_update3" AFTER UPDATE    ON test2 WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
    )


###############################################################################
# Test PRELUDE_STATEMENTS open option


def test_ogr_gpkg_prelude_statements(tmp_vsimem):

    gdal.VectorTranslate(tmp_vsimem / "test.gpkg", "data/poly.shp", format="GPKG")
    ds = gdal.OpenEx(
        tmp_vsimem / "test.gpkg",
        open_options=[
            f"PRELUDE_STATEMENTS=ATTACH DATABASE '{tmp_vsimem}/test.gpkg' AS other"
        ],
    )
    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly JOIN other.poly USING (eas_id)")
    assert sql_lyr.GetFeatureCount() == 10
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test DATETIME_FORMAT


def test_ogr_gpkg_datetime_timezones(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_datetime_timezones.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename, options=["DATETIME_FORMAT=UTC"])
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    for val in [
        "2020/01/01 01:34:56",
        "2020/01/01 01:34:56+00",
        "2020/01/01 01:34:56.789+02",
    ]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("dt", val)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField("dt") == "2020/01/01 01:34:56+00"
    f = lyr.GetNextFeature()
    assert f.GetField("dt") == "2020/01/01 01:34:56+00"
    f = lyr.GetNextFeature()
    assert f.GetField("dt") == "2019/12/31 23:34:56.789+00"

    sql_lyr = ds.ExecuteSQL("SELECT dt || '' FROM test")
    f = sql_lyr.GetNextFeature()
    # check that milliseconds are written to be strictly compliant with the GPKG spec
    assert f.GetField(0) == "2020-01-01T01:34:56.000Z"
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test AbortSQL


def test_abort_sql():

    filename = "data/gpkg/poly_non_conformant.gpkg"
    ds = ogr.Open(filename)

    def abortAfterDelay():
        # print("Aborting SQL...")
        assert ds.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay)
    t.start()

    start = time.time()

    # Long running query
    sql = """
        WITH RECURSIVE r(i) AS (
            VALUES(0)
            UNION ALL
            SELECT i FROM r
            LIMIT 100000000
            )
        SELECT i FROM r WHERE i = 1;"""

    with gdal.quiet_errors():
        ds.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 2

    # Same test with a GDAL dataset
    ds2 = gdal.OpenEx(filename, gdal.OF_VECTOR)

    def abortAfterDelay2():
        # print("Aborting SQL...")
        assert ds2.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay2)
    t.start()

    start = time.time()

    # Long running query
    with gdal.quiet_errors():
        ds2.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 2


###############################################################################
# Test ST_Transform() with no record in gpkg_spatial_ref_sys and thus we
# fallback to EPSG


@pytest.mark.skipif(
    sys.platform == "win32",
    reason="f.GetGeometryRef() returns None on the current Windows CI",
)
def test_ogr_gpkg_st_transform_no_record_spatial_ref_sys(tmp_vsimem):

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmp_vsimem / "test.gpkg")
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (500000 0)"))
    lyr.CreateFeature(f)
    f = None

    if not _has_spatialite_4_3_or_later(ds):
        ds = None
        pytest.skip("Spatialite missing or too old")

    sql_lyr = ds.ExecuteSQL(
        "SELECT ST_Transform(SetSRID(geom, 32631), 32731) FROM test"
    )
    # Fails on a number of configs
    # assert sql_lyr.GetSpatialRef().GetAuthorityCode(None) == '32731'
    f = sql_lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (500000 10000000)"
    f = None
    ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test deferred spatial index creation


def test_ogr_gpkg_deferred_spi_creation(tmp_vsimem):
    def has_spi(ds):
        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_test_geom'",
            dialect="DEBUG",
        )
        res = sql_lyr.GetNextFeature() is not None
        ds.ReleaseResultSet(sql_lyr)
        return res

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmp_vsimem / "test.gpkg")

    lyr = ds.CreateLayer("test")
    assert not has_spi(ds)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    fid = f.GetFID()
    f = None
    assert not has_spi(ds)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is not None
    assert not has_spi(ds)

    assert lyr.GetFeature(fid) is not None
    assert not has_spi(ds)

    assert lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString)) == ogr.OGRERR_NONE
    assert not has_spi(ds)

    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    assert not has_spi(ds)

    ds.ReleaseResultSet(ds.ExecuteSQL("SELECT 1"))
    assert has_spi(ds)

    # GetNextFeature() with spatial filter should cause SPI creation
    ds.ExecuteSQL("DELLAYER:test")
    lyr = ds.CreateLayer("test")
    assert not has_spi(ds)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    fid = f.GetFID()
    f = None
    assert not has_spi(ds)

    lyr.SetSpatialFilterRect(-1, -1, 1, 1)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is not None
    assert has_spi(ds)

    ds = None


###############################################################################
# Test deferred spatial index update


@pytest.mark.parametrize("gpkg_version", ["1.2", "1.4"])
def test_ogr_gpkg_deferred_spi_update(tmp_vsimem, gpkg_version):
    def has_spi_triggers(ds):
        sql_lyr = ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE type = 'trigger' AND name LIKE 'rtree_test_geom%'",
            dialect="DEBUG",
        )
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        return res == 6 or res == 7

    filename = tmp_vsimem / "test.gpkg"

    # Basic test
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(
        filename, options=["VERSION=" + gpkg_version]
    )
    ds.CreateLayer("test")
    ds = None
    with gdaltest.config_option("OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD", "2"):
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
        lyr.CreateFeature(f)
        assert has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 1)"))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (2 2)"))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect="DEBUG")
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 2

        ds.CommitTransaction()
        assert has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect="DEBUG")
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 3

        ds = None

    # Check effect of RollbackTransaction()
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(
        filename, options=["VERSION=" + gpkg_version]
    )
    ds.CreateLayer("test")
    ds = None
    with gdaltest.config_option("OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD", "1"):
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 1)"))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect="DEBUG")
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 1

        ds.RollbackTransaction()
        assert has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect="DEBUG")
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 0

        ds = None

    # Check that GetNextFeature() with a spatial filter causes flushing of
    # deferred SPI values
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(
        filename, options=["VERSION=" + gpkg_version]
    )
    ds.CreateLayer("test")
    ds = None
    with gdaltest.config_option("OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD", "1"):
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 1)"))
        lyr.CreateFeature(f)

        lyr.SetSpatialFilterRect(0, 0, 1, 1)
        lyr.ResetReading()
        assert lyr.GetNextFeature() is not None
        assert has_spi_triggers(ds)
        assert lyr.GetNextFeature() is not None
        assert lyr.GetNextFeature() is None
        ds = None


###############################################################################
# Test field domains


def test_ogr_gpkg_field_domains(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test.gpkg"

    # Test write support
    ds = gdal.GetDriverByName("GPKG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)

    assert ds.TestCapability(ogr.ODsCAddFieldDomain)

    assert ds.GetFieldDomainNames() is None

    assert ds.GetFieldDomain("does_not_exist") is None

    assert ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_int",
            "my desc",
            ogr.OFTInteger,
            ogr.OFSTNone,
            1,
            True,
            2,
            False,
        )
    )
    assert ds.GetFieldDomain("range_domain_int") is not None

    assert set(ds.GetFieldDomainNames()) == {"range_domain_int"}

    assert not ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_int",
            "my desc",
            ogr.OFTInteger,
            ogr.OFSTNone,
            1,
            True,
            2,
            True,
        )
    )

    assert ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_int64",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            -1234567890123,
            False,
            1234567890123,
            True,
        )
    )
    assert ds.GetFieldDomain("range_domain_int64") is not None

    assert ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_real", "", ogr.OFTReal, ogr.OFSTNone, 1.5, True, 2.5, True
        )
    )
    assert ds.GetFieldDomain("range_domain_real") is not None

    assert ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_real_inf",
            "",
            ogr.OFTReal,
            ogr.OFSTNone,
            -math.inf,
            True,
            math.inf,
            True,
        )
    )
    assert ds.GetFieldDomain("range_domain_real_inf") is not None

    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain(
            "glob_domain", "my desc", ogr.OFTString, ogr.OFSTNone, "*"
        )
    )
    assert ds.GetFieldDomain("glob_domain") is not None

    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain", "", ogr.OFTInteger64, ogr.OFSTNone, {1: "one", "2": None}
        )
    )
    assert ds.GetFieldDomain("enum_domain") is not None

    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_int_single",
            "my desc",
            ogr.OFTInteger,
            ogr.OFSTNone,
            {1: "one"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_int",
            "",
            ogr.OFTInteger,
            ogr.OFSTNone,
            {1: "one", 2: "two"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_int64_single_1",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {1234567890123: "1234567890123"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_int64_single_2",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {-1234567890123: "-1234567890123"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_int64",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {1: "one", 1234567890123: "1234567890123", 3: "three"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_real_single",
            "",
            ogr.OFTReal,
            ogr.OFSTNone,
            {1.5: "one dot five"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_real",
            "",
            ogr.OFTReal,
            ogr.OFSTNone,
            {1: "one", 1.5: "one dot five", 1234567890123: "1234567890123", 3: "three"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_string_single",
            "",
            ogr.OFTString,
            ogr.OFSTNone,
            {"three": "three"},
        )
    )
    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain_guess_string",
            "",
            ogr.OFTString,
            ogr.OFSTNone,
            {1: "one", 1.5: "one dot five", "three": "three", 4: "four"},
        )
    )

    assert len(ds.GetFieldDomainNames()) == len(set(ds.GetFieldDomainNames()))
    assert set(ds.GetFieldDomainNames()) == {
        "enum_domain",
        "enum_domain_guess_int",
        "enum_domain_guess_int64",
        "enum_domain_guess_int64_single_1",
        "enum_domain_guess_int64_single_2",
        "enum_domain_guess_int_single",
        "enum_domain_guess_real",
        "enum_domain_guess_real_single",
        "enum_domain_guess_string",
        "enum_domain_guess_string_single",
        "glob_domain",
        "range_domain_int",
        "range_domain_int64",
        "range_domain_real",
        "range_domain_real_inf",
    }

    lyr = ds.CreateLayer("test")

    fld_defn = ogr.FieldDefn("with_range_domain_int", ogr.OFTInteger)
    fld_defn.SetDomainName("range_domain_int")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("with_range_domain_int64", ogr.OFTInteger64)
    fld_defn.SetDomainName("range_domain_int64")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("with_range_domain_real", ogr.OFTReal)
    fld_defn.SetDomainName("range_domain_real")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("with_glob_domain", ogr.OFTString)
    fld_defn.SetDomainName("glob_domain")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("with_enum_domain", ogr.OFTInteger64)
    fld_defn.SetDomainName("enum_domain")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("without_domain_initially", ogr.OFTInteger)
    lyr.CreateField(fld_defn)

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    # Test read support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_data_column_constraints")
    assert sql_lyr is not None
    ds.ReleaseResultSet(sql_lyr)

    assert set(ds.GetFieldDomainNames()) == {
        "enum_domain",
        "enum_domain_guess_int",
        "enum_domain_guess_int64",
        "enum_domain_guess_int64_single_1",
        "enum_domain_guess_int64_single_2",
        "enum_domain_guess_int_single",
        "enum_domain_guess_real",
        "enum_domain_guess_real_single",
        "enum_domain_guess_string",
        "enum_domain_guess_string_single",
        "glob_domain",
        "range_domain_int",
        "range_domain_int64",
        "range_domain_real",
        "range_domain_real_inf",
    }

    domain = ds.GetFieldDomain("range_domain_int")
    assert domain is not None
    assert domain.GetName() == "range_domain_int"
    assert domain.GetDescription() == "my desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger
    assert domain.GetMinAsDouble() == 1.0
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.0
    assert not domain.IsMaxInclusive()

    domain = ds.GetFieldDomain("range_domain_int64")
    assert domain is not None
    assert domain.GetName() == "range_domain_int64"
    assert domain.GetDescription() == ""
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger64
    assert domain.GetMinAsDouble() == -1234567890123
    assert not domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 1234567890123
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain("range_domain_real")
    assert domain is not None
    assert domain.GetName() == "range_domain_real"
    assert domain.GetDescription() == ""
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetMinAsDouble() == 1.5
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.5
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain("range_domain_real_inf")
    assert domain is not None
    assert domain.GetName() == "range_domain_real_inf"
    assert domain.GetDescription() == ""
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetMinAsDouble() == -math.inf
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == math.inf
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain("glob_domain")
    assert domain is not None
    assert domain.GetName() == "glob_domain"
    assert domain.GetDescription() == "my desc"
    assert domain.GetDomainType() == ogr.OFDT_GLOB
    assert domain.GetFieldType() == ogr.OFTString
    assert domain.GetGlob() == "*"

    domain = ds.GetFieldDomain("enum_domain")
    assert domain is not None
    assert domain.GetName() == "enum_domain"
    assert domain.GetDescription() == ""
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == ogr.OFTInteger64
    assert domain.GetEnumeration() == {"1": "one", "2": None}

    domain = ds.GetFieldDomain("enum_domain_guess_int_single")
    assert domain.GetDescription() == "my desc"
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = ds.GetFieldDomain("enum_domain_guess_int")
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = ds.GetFieldDomain("enum_domain_guess_int64_single_1")
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain("enum_domain_guess_int64_single_2")
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain("enum_domain_guess_int64")
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain("enum_domain_guess_real_single")
    assert domain.GetFieldType() == ogr.OFTReal

    domain = ds.GetFieldDomain("enum_domain_guess_real")
    assert domain.GetFieldType() == ogr.OFTReal

    domain = ds.GetFieldDomain("enum_domain_guess_string_single")
    assert domain.GetFieldType() == ogr.OFTString

    domain = ds.GetFieldDomain("enum_domain_guess_string")
    assert domain.GetFieldType() == ogr.OFTString

    lyr = ds.GetLayerByName("test")
    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("with_range_domain_int"))
    assert fld_defn.GetDomainName() == "range_domain_int"

    if gdal.GetDriverByName("GPKG").GetMetadataItem("SQLITE_HAS_COLUMN_METADATA"):
        sql_lyr = ds.ExecuteSQL("SELECT with_range_domain_int FROM test")
        assert (
            sql_lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "range_domain_int"
        )
        ds.ReleaseResultSet(sql_lyr)

    ds = None

    # Test AlterFieldDefn() support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    # Unset domain name
    idx = lyr_defn.GetFieldIndex("with_range_domain_int")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName("")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Change domain name
    idx = lyr_defn.GetFieldIndex("with_range_domain_int64")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName("with_enum_domain")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Set domain name
    idx = lyr_defn.GetFieldIndex("without_domain_initially")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName("range_domain_int")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Don't change anything
    idx = lyr_defn.GetFieldIndex("with_glob_domain")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    # Test read support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    idx = lyr_defn.GetFieldIndex("with_range_domain_int")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == ""

    idx = lyr_defn.GetFieldIndex("with_range_domain_int64")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == "with_enum_domain"

    idx = lyr_defn.GetFieldIndex("without_domain_initially")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == "range_domain_int"

    idx = lyr_defn.GetFieldIndex("with_glob_domain")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == "glob_domain"

    assert set(ds.GetFieldDomainNames()) == {
        "enum_domain",
        "enum_domain_guess_int",
        "enum_domain_guess_int64",
        "enum_domain_guess_int64_single_1",
        "enum_domain_guess_int64_single_2",
        "enum_domain_guess_int_single",
        "enum_domain_guess_real",
        "enum_domain_guess_real_single",
        "enum_domain_guess_string",
        "enum_domain_guess_string_single",
        "glob_domain",
        "range_domain_int",
        "range_domain_int64",
        "range_domain_real",
        "range_domain_real_inf",
    }

    ds = None


###############################################################################
# Test error cases in field domains


def test_ogr_gpkg_field_domains_errors(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test.gpkg"

    ds = gdal.GetDriverByName("GPKG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    ds.CreateLayer("test")
    # The DDL lacks on purpose the NOT NULL constraints on constraint_name and constraint_type
    ds.ExecuteSQL(
        "CREATE TABLE gpkg_data_column_constraints ("
        + "constraint_name TEXT,constraint_type TEXT,value TEXT,"
        + "min NUMERIC,min_is_inclusive BOOLEAN,"
        + "max NUMERIC,max_is_inclusive BOOLEAN,description TEXT)"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('null_constraint_type', NULL, NULL, NULL, NULL, NULL, NULL, NULL)"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('invalid_constraint_type', 'invalid', NULL, NULL, NULL, NULL, NULL, NULL)"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('mix_glob_enum', 'glob', '*', NULL, NULL, NULL, NULL, NULL)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('mix_glob_enum', 'enum', 'foo', NULL, NULL, NULL, NULL, 'bar')"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('null_in_enum_code', 'enum', NULL, NULL, NULL, NULL, NULL, 'bar')"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('null_in_glob_value', 'glob', NULL, NULL, NULL, NULL, NULL, NULL)"
    )

    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_column_constraints VALUES "
        + "('null_in_range', 'range', NULL, NULL, NULL, NULL, NULL, NULL)"
    )
    ds = None

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)

    assert ds.GetFieldDomain("null_constraint_type") is None

    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert ds.GetFieldDomain("invalid_constraint_type") is None
        assert gdal.GetLastErrorMsg() != ""

    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert ds.GetFieldDomain("mix_glob_enum") is None
        assert gdal.GetLastErrorMsg() != ""

    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert ds.GetFieldDomain("null_in_enum_code") is None
        assert gdal.GetLastErrorMsg() != ""

    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert ds.GetFieldDomain("null_in_glob_value") is None
        assert gdal.GetLastErrorMsg() != ""

    # This is non conformant, but we accept it
    domain = ds.GetFieldDomain("null_in_range")
    assert domain is not None
    assert domain.GetMinAsDouble() == -math.inf
    assert domain.GetMaxAsDouble() == math.inf

    ds = None


###############################################################################
# Test gpkg_data_column_constraints of GPKG 1.0


def test_ogr_gpkg_field_domain_gpkg_1_0(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test.gpkg"

    ds = gdal.GetDriverByName("GPKG").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown, options=["VERSION=1.0"]
    )
    ds.CreateLayer("test")
    assert ds.AddFieldDomain(
        ogr.CreateRangeFieldDomain(
            "range_domain_int",
            "my desc",
            ogr.OFTReal,
            ogr.OFSTNone,
            1.5,
            True,
            2.5,
            False,
        )
    )
    ds = None

    assert validate(filename, tmpdir=tmp_path)

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)

    gdal.ErrorReset()
    domain = ds.GetFieldDomain("range_domain_int")
    assert gdal.GetLastErrorMsg() == ""
    assert domain is not None
    assert domain.GetName() == "range_domain_int"
    assert domain.GetDescription() == "my desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetMinAsDouble() == 1.5
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.5
    assert not domain.IsMaxInclusive()

    ds = None


###############################################################################
# Test attribute and spatial views


def test_ogr_gpkg_views(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_views.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    lyr.CreateFeature(f)

    ds.ExecuteSQL(
        "CREATE VIEW geom_view AS SELECT fid AS my_fid, geom AS my_geom FROM foo"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'geom_view', 'geom_view', 'features', 0 )"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('geom_view', 'my_geom', 'POINT', 0, 0, 0)"
    )

    ds.ExecuteSQL("CREATE VIEW attr_view AS SELECT fid AS my_fid FROM foo")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type) VALUES ( 'attr_view', 'attr_view', 'attributes' )"
    )

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 3

    lyr = ds.GetLayerByName("geom_view")
    assert lyr.GetGeomType() == ogr.wkbPoint

    lyr = ds.GetLayerByName("attr_view")
    assert lyr.GetGeomType() == ogr.wkbNone

    ds = None


###############################################################################
# Test a spatial view where the geometry column is computed with a
# Spatialite function


def test_ogr_gpkg_spatial_view_computed_geom_column(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_spatial_view_computed_geom_column.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)

    if not _has_spatialite_4_3_or_later(ds):
        ds = None
        pytest.skip("spatialite missing")

    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)

    ds.ExecuteSQL(
        "CREATE VIEW geom_view AS SELECT fid AS my_fid, AsGPB(ST_Multi(geom)) AS my_geom FROM foo"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'geom_view', 'geom_view', 'features', 4326 )"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('geom_view', 'my_geom', 'MULTIPOINT', 4326, 0, 0)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('geom_view', 'my_geom', 'gdal_spatialite_computed_geom_column', 'https://gdal.org/drivers/vector/gpkg_spatialite_computed_column.html', 'read-write')"
    )

    ds = None

    import sqlite3

    conn = sqlite3.connect(":memory:")
    can_use_validate = False
    try:
        conn.enable_load_extension(True)
        conn.execute('SELECT load_extension("mod_spatialite")')
        can_use_validate = True
    except Exception:
        pass
    conn.close()
    if can_use_validate:
        assert validate(filename, tmpdir=tmp_path), "validation failed"
    else:
        print("Cannot validate() due to mod_spatialite not being loadable")

    ds = ogr.Open(filename)

    lyr = ds.GetLayerByName("geom_view")
    assert lyr.GetGeomType() == ogr.wkbMultiPoint
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOINT (1 2)"

    ds = None


###############################################################################
# Test read support for legacy gdal_aspatial extension


def test_ogr_gpkg_read_deprecated_gdal_aspatial(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_aspatial.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.ExecuteSQL(
        "CREATE TABLE gpkg_extensions ("
        "table_name TEXT,"
        "column_name TEXT,"
        "extension_name TEXT NOT NULL,"
        "definition TEXT NOT NULL,"
        "scope TEXT NOT NULL,"
        "CONSTRAINT ge_tce UNIQUE (table_name, column_name, extension_name)"
        ")"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')"
    )
    ds.ExecuteSQL("CREATE TABLE aspatial_layer(fid INTEGER PRIMARY KEY,bar TEXT)")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, data_type) VALUES ('aspatial_layer', 'aspatial')"
    )
    ds.CreateLayer("spatial_layer", options=["SPATIAL_INDEX=NO"])
    ds = None

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 2
    ds = None


###############################################################################
# Test fixing up wrong gpkg_metadata_reference_column_name_update trigger (GDAL < 2.4.0)


def test_ogr_gpkg_fixup_wrong_mr_column_name_update_trigger(tmp_vsimem):

    filename = (
        tmp_vsimem / "test_ogr_gpkg_fixup_wrong_mr_column_name_update_trigger.gpkg"
    )
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    ds.SetMetadata("FOO", "BAR")
    ds = None

    ds = ogr.Open(filename, update=1)
    # inject wrong trigger on purpose
    wrong_trigger = (
        "CREATE TRIGGER 'gpkg_metadata_reference_column_name_update' "
        + "BEFORE UPDATE OF column_name ON 'gpkg_metadata_reference' "
        + "FOR EACH ROW BEGIN "
        + "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        + "violates constraint: column name must be NULL when reference_scope "
        + 'is "geopackage", "table" or "row"\') '
        + "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
        + "AND NEW.column_nameIS NOT NULL); END;"
    )
    ds.ExecuteSQL(wrong_trigger)
    ds = None

    # Open in update mode
    ds = ogr.Open(filename, update=1)
    sql_lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' "
        + "AND name = 'gpkg_metadata_reference_column_name_update'"
    )
    f = sql_lyr.GetNextFeature()
    sql = f["sql"]
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert "column_nameIS" not in sql


###############################################################################
# Test support for CRS coordinate_epoch


def test_ogr_gpkg_crs_coordinate_epoch(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_crs_coordinate_epoch.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=0,0,0")
    srs.SetCoordinateEpoch(2021.3)
    ds.CreateLayer("lyr_with_coordinate_epoch_unknown_srs", srs=srs)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(7665)  # WGS 84 (G1762) (3D)
    srs.SetCoordinateEpoch(2021.3)
    ds.CreateLayer("lyr_with_coordinate_epoch", srs=srs)

    srs.SetCoordinateEpoch(2021.3)
    ds.CreateLayer("lyr_with_same_coordinate_epoch", srs=srs)

    srs.SetCoordinateEpoch(2021.2)
    ds.CreateLayer("lyr_with_different_coordinate_epoch", srs=srs)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258)  # ETRS89
    ds.CreateLayer("lyr_without_coordinate_epoch", srs=srs)

    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys ORDER BY srs_id")
    assert sql_lyr.GetFeatureCount() == 7

    sql_lyr.GetNextFeature()
    sql_lyr.GetNextFeature()

    f = sql_lyr.GetNextFeature()
    assert f
    assert f["srs_id"] == 4258
    assert f["organization"] == "EPSG"
    assert f["organization_coordsys_id"] == 4258
    assert f["epoch"] is None

    f = sql_lyr.GetNextFeature()
    assert f
    assert f["srs_id"] == 4326
    assert f["organization"] == "EPSG"
    assert f["organization_coordsys_id"] == 4326
    assert f["epoch"] is None

    f = sql_lyr.GetNextFeature()
    assert f
    assert f["srs_id"] == 100000
    assert f["organization"] == "NONE"
    assert f["organization_coordsys_id"] == 100000
    assert f["epoch"] == 2021.3

    f = sql_lyr.GetNextFeature()
    assert f
    assert f["srs_id"] == 100001
    assert f["organization"] == "EPSG"
    assert f["organization_coordsys_id"] == 7665
    assert f["epoch"] == 2021.3

    f = sql_lyr.GetNextFeature()
    assert f
    assert f["srs_id"] == 100002
    assert f["organization"] == "EPSG"
    assert f["organization_coordsys_id"] == 7665
    assert f["epoch"] == 2021.2
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName("lyr_with_coordinate_epoch_unknown_srs")
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3

    lyr = ds.GetLayerByName("lyr_with_coordinate_epoch")
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3

    lyr = ds.GetLayerByName("lyr_with_same_coordinate_epoch")
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3

    lyr = ds.GetLayerByName("lyr_with_different_coordinate_epoch")
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.2

    lyr = ds.GetLayerByName("lyr_without_coordinate_epoch")
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 0

    ds = None


###############################################################################
# Test CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE


def test_ogr_gpkg_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE_a(tmp_vsimem):

    # First check that CPL_TMPDIR is ignored for regular files
    filename = tmp_vsimem / "test_ogr_gpkg_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE.gpkg"
    with gdaltest.config_option("CPL_TMPDIR", "/i_do/not/exist"):
        ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    assert ds is not None
    ds = None


def test_ogr_gpkg_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE_b(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE.gpkg"

    # Now check that CPL_TMPDIR is honored for CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=FORCED
    gdal.Mkdir(tmp_vsimem / "temporary_location", 0o755)
    with gdaltest.config_options(
        {
            "CPL_TMPDIR": str(tmp_vsimem / "temporary_location"),
            "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE": "FORCED",
        }
    ):
        ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    assert ds is not None
    assert gdal.VSIStatL(filename) is None
    assert len(gdal.ReadDir(tmp_vsimem / "temporary_location")) != 0
    ds = None
    assert gdal.VSIStatL(filename) is not None
    assert gdal.ReadDir(tmp_vsimem / "temporary_location") is None


###############################################################################
# Test support for related tables extension


def test_ogr_gpkg_relations(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_relations.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("a")
    lyr.CreateField(ogr.FieldDefn("some_id", ogr.OFTInteger))
    lyr = ds.CreateLayer("b")
    lyr.CreateField(ogr.FieldDefn("other_id", ogr.OFTInteger))
    ds = None

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() is None

    ds.ExecuteSQL(
        """CREATE TABLE 'gpkgext_relations' (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          base_table_name TEXT NOT NULL,
          base_primary_column TEXT NOT NULL DEFAULT 'id',
          related_table_name TEXT NOT NULL,
          related_primary_column TEXT NOT NULL DEFAULT 'id',
          relation_name TEXT NOT NULL,
          mapping_table_name TEXT NOT NULL UNIQUE
         );"""
    )

    # not yet valid...
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() is None

    ds.ExecuteSQL(
        "INSERT INTO gpkgext_relations VALUES(1, 'a', 'some_id', 'b', 'other_id', 'attributes', 'my_mapping_table')"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('gpkgext_relations',NULL,'gpkg_related_tables','http://www.geopackage.org/18-000.html','read-write');"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('my_mapping_table',NULL,'gpkg_related_tables','http://www.geopackage.org/18-000.html','read-write');"
    )
    ds = None

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() is None
    ds.ExecuteSQL(
        """CREATE TABLE my_mapping_table(base_id INTEGER NOT NULL, related_id INTEGER NOT NULL);"""
    )

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert ds.GetRelationshipNames() == ["a_b_attributes"]
    assert ds.GetRelationship("xxx") is None
    rel = ds.GetRelationship("a_b_attributes")
    assert rel is not None
    assert rel.GetName() == "a_b_attributes"
    assert rel.GetLeftTableName() == "a"
    assert rel.GetRightTableName() == "b"
    assert rel.GetMappingTableName() == "my_mapping_table"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["some_id"]
    assert rel.GetRightTableFields() == ["other_id"]
    assert rel.GetLeftMappingTableFields() == ["base_id"]
    assert rel.GetRightMappingTableFields() == ["related_id"]
    assert rel.GetRelatedTableType() == "attributes"

    lyr = ds.GetLayer("a")
    lyr.Rename("a_renamed")
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("some_id"),
        ogr.FieldDefn("some_id_renamed", ogr.OFTInteger),
        ogr.ALTER_ALL_FLAG,
    )
    lyr = ds.GetLayer("b")
    lyr.Rename("b_renamed")
    lyr.AlterFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex("other_id"),
        ogr.FieldDefn("other_id_renamed", ogr.OFTInteger),
        ogr.ALTER_ALL_FLAG,
    )

    assert ds.GetRelationshipNames() == ["a_renamed_b_renamed_attributes"]
    assert ds.GetRelationship("xxx") is None
    rel = ds.GetRelationship("a_renamed_b_renamed_attributes")
    assert rel is not None
    assert rel.GetName() == "a_renamed_b_renamed_attributes"
    assert rel.GetLeftTableName() == "a_renamed"
    assert rel.GetRightTableName() == "b_renamed"
    assert rel.GetMappingTableName() == "my_mapping_table"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["some_id_renamed"]
    assert rel.GetRightTableFields() == ["other_id_renamed"]
    assert rel.GetLeftMappingTableFields() == ["base_id"]
    assert rel.GetRightMappingTableFields() == ["related_id"]
    assert rel.GetRelatedTableType() == "attributes"

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    ds.ExecuteSQL("DELLAYER:a_renamed")
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE extension_name IN ('related_tables', 'gpkg_related_tables')"
    )
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    assert ds.GetRelationshipNames() is None
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    # user defined relation
    ds = ogr.Open(filename, update=1)
    lyr = ds.CreateLayer("a")
    lyr.CreateField(ogr.FieldDefn("some_id", ogr.OFTInteger))
    ds.ExecuteSQL(
        "INSERT INTO gpkgext_relations VALUES(1, 'a', 'some_id', 'b', 'other_id', 'custom_type', 'my_mapping_table')"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('gpkgext_relations',NULL,'gpkg_related_tables','http://www.geopackage.org/18-000.html','read-write');"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions VALUES('my_mapping_table',NULL,'gpkg_related_tables','http://www.geopackage.org/18-000.html','read-write');"
    )

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)
    assert ds.GetRelationshipNames() == ["custom_type"]
    assert ds.GetRelationship("xxx") is None
    rel = ds.GetRelationship("custom_type")
    assert rel is not None
    assert rel.GetName() == "custom_type"
    assert rel.GetLeftTableName() == "a"
    assert rel.GetRightTableName() == "b"
    assert rel.GetMappingTableName() == "my_mapping_table"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["some_id"]
    assert rel.GetRightTableFields() == ["other_id"]
    assert rel.GetLeftMappingTableFields() == ["base_id"]
    assert rel.GetRightMappingTableFields() == ["related_id"]
    assert rel.GetRelatedTableType() == "features"

    ds = None


###############################################################################
# Test support for relations taken from sqlite foreign keys when related tables
# extension is not used


def test_ogr_gpkg_relations_sqlite_foreign_keys(tmp_vsimem):
    tmpfilename = tmp_vsimem / "test_ogr_gpkg_relations_sqlite.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(tmpfilename)
    lyr = ds.CreateLayer("a")
    lyr.CreateField(ogr.FieldDefn("some_id", ogr.OFTInteger))
    lyr = ds.CreateLayer("b")
    lyr.CreateField(ogr.FieldDefn("other_id", ogr.OFTInteger))
    ds = None

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


###############################################################################
# Test support for altering relationships


def test_ogr_gpkg_alter_relations(tmp_vsimem, tmp_path):
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

    filename = tmp_vsimem / "test_ogr_gpkg_relation_create.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)

    def get_query_row_count(query):
        sql_lyr = ds.ExecuteSQL(query)
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        return res

    relationship = gdal.Relationship(
        "my_relationship", "origin_table", "dest_table", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("media")

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    # no tables yet
    assert not ds.AddRelationship(relationship)

    lyr = ds.CreateLayer("origin_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("o_pkey2", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    ds.ExecuteSQL("CREATE UNIQUE INDEX origin_table_o_pkey_idx ON origin_table(o_pkey)")

    assert not ds.AddRelationship(relationship)

    lyr = ds.CreateLayer("dest_table", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("dest_pkey2", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    ds.ExecuteSQL(
        "CREATE UNIQUE INDEX dest_table_dest_pkey_idx ON dest_table(dest_pkey)"
    )

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

    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {"origin_table_dest_table_media"}
    retrieved_rel = ds.GetRelationship("origin_table_dest_table_media")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "media"
    assert retrieved_rel.GetMappingTableName() == "origin_table_dest_table"
    assert retrieved_rel.GetLeftMappingTableFields() == ["base_id"]
    assert retrieved_rel.GetRightMappingTableFields() == ["related_id"]

    # try again, should fail because relationship already exists
    assert not ds.AddRelationship(relationship)

    # validate that extensions table exists and is correctly populated
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'gpkgext_relations' AND extension_name = 'gpkg_related_tables'"
        )
        == 1
    )
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'origin_table_dest_table' AND extension_name = 'gpkg_related_tables'"
        )
        == 1
    )

    # validate gpkgext_relations has been populated correctly
    assert (
        get_query_row_count(
            "SELECT * FROM gpkgext_relations WHERE base_table_name = 'origin_table' AND "
            "base_primary_column = 'o_pkey' AND "
            "related_table_name = 'dest_table' AND "
            "related_primary_column = 'dest_pkey' AND "
            "relation_name = 'media' AND "
            "mapping_table_name = 'origin_table_dest_table'"
        )
        == 1
    )

    lyr = ds.CreateLayer("origin_table2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table2", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    # only many-to-many relationships are supported
    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_ONE_TO_ONE
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("features")
    assert not ds.AddRelationship(relationship)

    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_ONE_TO_MANY
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

    # only features/media/simple_attributes/attributes/tiles related table type are supported
    relationship = gdal.Relationship(
        "my_relationship", "origin_table2", "dest_table2", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetRelatedTableType("something else")
    assert not ds.AddRelationship(relationship)

    # should default to "features" related table type if nothing explicitly specified
    relationship.SetRelatedTableType("")
    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_media",
        "origin_table2_dest_table2_features",
    }
    retrieved_rel = ds.GetRelationship("origin_table2_dest_table2_features")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table2"
    assert retrieved_rel.GetRightTableName() == "dest_table2"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "features"

    # validate that extensions table exists is correctly populated
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'origin_table2_dest_table2' AND extension_name = 'gpkg_related_tables'"
        )
        == 1
    )
    # validate gpkgext_relations has been populated correctly
    assert (
        get_query_row_count(
            "SELECT * FROM gpkgext_relations WHERE base_table_name = 'origin_table2' AND "
            "base_primary_column = 'o_pkey' AND "
            "related_table_name = 'dest_table2' AND "
            "related_primary_column = 'dest_pkey' AND "
            "relation_name = 'features' AND "
            "mapping_table_name = 'origin_table2_dest_table2'"
        )
        == 1
    )

    # try with an existing mapping table
    lyr = ds.CreateLayer("origin_table3", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("dest_table3", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    lyr = ds.CreateLayer("origin_table3_to_dest_table_3_mapping", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("base_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("related_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert (
        get_query_row_count(
            "SELECT 1 FROM sqlite_master WHERE name = 'origin_table3_to_dest_table_3_mapping' AND type in ('table', 'view')"
        )
        == 1
    )

    relationship = gdal.Relationship(
        "my_relationship", "origin_table3", "dest_table3", gdal.GRC_MANY_TO_MANY
    )
    # fid fields should be permitted for relationship use
    relationship.SetLeftTableFields(["fid"])
    relationship.SetRightTableFields(["fid"])
    relationship.SetMappingTableName("nope")
    assert not ds.AddRelationship(relationship)

    relationship.SetMappingTableName("origin_table3_to_dest_table_3_mapping")
    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_media",
        "origin_table2_dest_table2_features",
        "origin_table3_dest_table3_features",
    }
    retrieved_rel = ds.GetRelationship("origin_table3_dest_table3_features")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table3"
    assert retrieved_rel.GetRightTableName() == "dest_table3"
    assert retrieved_rel.GetLeftTableFields() == ["fid"]
    assert retrieved_rel.GetRightTableFields() == ["fid"]
    assert retrieved_rel.GetRelatedTableType() == "features"
    assert (
        retrieved_rel.GetMappingTableName() == "origin_table3_to_dest_table_3_mapping"
    )

    # validate that extensions table exists is correctly populated
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'origin_table3_to_dest_table_3_mapping' AND extension_name = 'gpkg_related_tables'"
        )
        == 1
    )

    # validate gpkgext_relations has been populated correctly
    assert (
        get_query_row_count(
            "SELECT * FROM gpkgext_relations WHERE base_table_name = 'origin_table3' AND "
            "base_primary_column = 'fid' AND "
            "related_table_name = 'dest_table3' AND "
            "related_primary_column = 'fid' AND "
            "relation_name = 'features' AND "
            "mapping_table_name = 'origin_table3_to_dest_table_3_mapping'"
        )
        == 1
    )

    # try again, with a mapping table which doesn't match requirements
    lyr = ds.CreateLayer("origin_table4", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    lyr = ds.CreateLayer("dest_table4", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("dest_pkey", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    lyr = ds.CreateLayer("origin_table4_to_dest_table_4_mapping", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("not_base_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("related_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert (
        get_query_row_count(
            "SELECT 1 FROM sqlite_master WHERE name = 'origin_table4_to_dest_table_4_mapping' AND type in ('table', 'view')"
        )
        == 1
    )
    relationship = gdal.Relationship(
        "my_relationship", "origin_table4", "dest_table4", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetLeftTableFields(["o_pkey"])
    relationship.SetRightTableFields(["dest_pkey"])
    relationship.SetMappingTableName("origin_table4_to_dest_table_4_mapping")
    assert not ds.AddRelationship(relationship)
    lyr = ds.CreateLayer(
        "origin_table4_to_dest_table_4_mappingv2", geom_type=ogr.wkbNone
    )
    fld_defn = ogr.FieldDefn("base_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    fld_defn = ogr.FieldDefn("not_related_id", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert (
        get_query_row_count(
            "SELECT 1 FROM sqlite_master WHERE name = 'origin_table4_to_dest_table_4_mappingv2' AND type in ('table', 'view')"
        )
        == 1
    )
    relationship.SetMappingTableName("origin_table4_to_dest_table_4_mappingv2")
    assert not ds.AddRelationship(relationship)

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    # delete relationship
    assert not ds.DeleteRelationship("nope")

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_media",
        "origin_table2_dest_table2_features",
        "origin_table3_dest_table3_features",
    }

    assert ds.DeleteRelationship("origin_table2_dest_table2_features")

    # validate that extensions table was correctly updated
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'origin_table2_dest_table2' AND extension_name = 'gpkg_related_tables'"
        )
        == 0
    )
    # validate gpkgext_relations has been updated correctly
    assert (
        get_query_row_count(
            "SELECT * FROM gpkgext_relations WHERE base_table_name = 'origin_table2' AND "
            "base_primary_column = 'o_pkey' AND "
            "related_table_name = 'dest_table2' AND "
            "related_primary_column = 'dest_pkey' AND "
            "relation_name = 'features' AND "
            "mapping_table_name = 'origin_table2_dest_table2'"
        )
        == 0
    )
    # validate that mapping table was deleted
    assert (
        get_query_row_count(
            "SELECT 1 FROM sqlite_master WHERE name = 'origin_table2_dest_table2' AND type in ('table', 'view')"
        )
        == 0
    )

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_media",
        "origin_table3_dest_table3_features",
    }

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    # update relationship
    retrieved_rel = ds.GetRelationship("origin_table_dest_table_media")

    # can't update a relationship which doesn't exit
    relationship = gdal.Relationship(
        "nope",
        retrieved_rel.GetLeftTableName(),
        retrieved_rel.GetRightTableName(),
        gdal.GRC_MANY_TO_MANY,
    )
    relationship.SetLeftTableFields(retrieved_rel.GetLeftTableFields())
    relationship.SetRightTableFields(retrieved_rel.GetRightTableFields())
    relationship.SetMappingTableName(retrieved_rel.GetMappingTableName())
    relationship.SetLeftMappingTableFields(retrieved_rel.GetLeftMappingTableFields())
    relationship.SetRightMappingTableFields(retrieved_rel.GetRightMappingTableFields())

    assert not ds.UpdateRelationship(clone_relationship(relationship))

    retrieved_rel = ds.GetRelationship("origin_table_dest_table_media")
    retrieved_rel.SetRelatedTableType("nope")
    # relationship will be validated before updates
    assert not ds.UpdateRelationship(clone_relationship(retrieved_rel))

    # change related table type
    retrieved_rel = ds.GetRelationship("origin_table_dest_table_media")
    retrieved_rel.SetRelatedTableType("attributes")
    assert ds.UpdateRelationship(clone_relationship(retrieved_rel))

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_attributes",
        "origin_table3_dest_table3_features",
    }

    retrieved_rel = ds.GetRelationship("origin_table_dest_table_attributes")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "attributes"

    # change base table field
    retrieved_rel.SetLeftTableFields(["o_pkey2"])
    assert ds.UpdateRelationship(clone_relationship(retrieved_rel))

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_attributes",
        "origin_table3_dest_table3_features",
    }

    retrieved_rel = ds.GetRelationship("origin_table_dest_table_attributes")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey2"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey"]
    assert retrieved_rel.GetRelatedTableType() == "attributes"

    retrieved_rel.SetRightTableFields(["dest_pkey2"])
    assert ds.UpdateRelationship(clone_relationship(retrieved_rel))

    assert set(ds.GetRelationshipNames()) == {
        "origin_table_dest_table_attributes",
        "origin_table3_dest_table3_features",
    }

    retrieved_rel = ds.GetRelationship("origin_table_dest_table_attributes")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "origin_table"
    assert retrieved_rel.GetRightTableName() == "dest_table"
    assert retrieved_rel.GetLeftTableFields() == ["o_pkey2"]
    assert retrieved_rel.GetRightTableFields() == ["dest_pkey2"]
    assert retrieved_rel.GetRelatedTableType() == "attributes"

    # try updating to field which doesn't exist
    retrieved_rel.SetRightTableFields(["dest_pkey2xxx"])
    assert not ds.UpdateRelationship(clone_relationship(retrieved_rel))

    # delete all relationships

    assert ds.DeleteRelationship("origin_table_dest_table_attributes")

    # validate that extensions table was correctly updated
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE table_name = 'origin_table_dest_table' AND extension_name = 'gpkg_related_tables'"
        )
        == 0
    )
    # validate gpkgext_relations has been updated correctly
    assert (
        get_query_row_count(
            "SELECT * FROM gpkgext_relations WHERE base_table_name = 'origin_table' AND "
            "base_primary_column = 'o_pkey2' AND "
            "related_table_name = 'dest_table' AND "
            "related_primary_column = 'dest_pkey2' AND "
            "relation_name = 'attributes' AND "
            "mapping_table_name = 'origin_table_dest_table'"
        )
        == 0
    )
    # validate that mapping table was deleted
    assert (
        get_query_row_count(
            "SELECT 1 FROM sqlite_master WHERE name = 'origin_table_dest_table' AND type in ('table', 'view')"
        )
        == 0
    )

    assert set(ds.GetRelationshipNames()) == {"origin_table3_dest_table3_features"}
    # should still be two extension rows for gpkg_related_tables: one for the remaining relationship, one for the extension itself
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE extension_name = 'gpkg_related_tables'"
        )
        == 2
    )

    assert ds.DeleteRelationship("origin_table3_dest_table3_features")

    # should be no remaining gpkg_related_tables extension records
    assert (
        get_query_row_count(
            "SELECT * FROM gpkg_extensions WHERE extension_name = 'gpkg_related_tables'"
        )
        == 0
    )
    # validate gpkgext_relations has been updated correctly
    assert get_query_row_count("SELECT * FROM gpkgext_relations") == 0

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"


###############################################################################
# Test creating relationships with complex names


def test_ogr_gpkg_add_relationship_complex_names(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_relation_create_complex.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)

    def get_query_row_count(query):
        sql_lyr = ds.ExecuteSQL(query)
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        return res

    relationship = gdal.Relationship(
        "my_relationship", "Origin' [tàble!", "dést ]table$", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetLeftTableFields(["o pkéy"])
    relationship.SetRightTableFields(["Dest pkéy"])
    relationship.SetRelatedTableType("media")

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    lyr = ds.CreateLayer("Origin' [tàble!", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("o pkéy", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    ds.ExecuteSQL(
        'CREATE UNIQUE INDEX origin_table_o_pkey_idx ON "Origin\' [tàble!"("o pkéy")'
    )

    lyr = ds.CreateLayer("dést ]table$", geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn("Dest pkéy", ogr.OFTInteger)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    ds.ExecuteSQL(
        'CREATE UNIQUE INDEX dest_table_dest_pkey_idx ON "dést ]table$"("Dest pkéy")'
    )

    relationship.SetLeftTableFields(["o pkéy"])
    relationship.SetRightTableFields(["Dest pkéy"])

    assert ds.AddRelationship(relationship)

    assert set(ds.GetRelationshipNames()) == {"Origin' [tàble!_dést ]table$_media"}
    retrieved_rel = ds.GetRelationship("Origin' [tàble!_dést ]table$_media")
    assert retrieved_rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert retrieved_rel.GetType() == gdal.GRT_ASSOCIATION
    assert retrieved_rel.GetLeftTableName() == "Origin' [tàble!"
    assert retrieved_rel.GetRightTableName() == "dést ]table$"
    assert retrieved_rel.GetLeftTableFields() == ["o pkéy"]
    assert retrieved_rel.GetRightTableFields() == ["Dest pkéy"]
    assert retrieved_rel.GetRelatedTableType() == "media"
    assert retrieved_rel.GetMappingTableName() == "Origin' [tàble!_dést ]table$"
    assert retrieved_rel.GetLeftMappingTableFields() == ["base_id"]
    assert retrieved_rel.GetRightMappingTableFields() == ["related_id"]

    ds = None
    assert validate(filename, tmpdir=tmp_path), "validation failed"


###############################################################################
# Test AlterGeomFieldDefn()


def test_ogr_gpkg_alter_geom_field_defn(tmp_vsimem, tmp_path):

    filename = tmp_vsimem / "test_ogr_gpkg_alter_geom_field_defn.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    srs_4326 = osr.SpatialReference()
    srs_4326.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, srs=srs_4326)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    # Test renaming column (only supported for SQLite >= 3.26)
    if get_sqlite_version() >= (3, 26, 0):
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

        new_geom_field_defn = ogr.GeomFieldDefn("new_geom_name", ogr.wkbNone)
        assert (
            lyr.AlterGeomFieldDefn(
                0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_NAME_FLAG
            )
            == ogr.OGRERR_NONE
        )
        assert lyr.GetGeometryColumn() == "new_geom_name"

        ds = None

        assert validate(filename, tmpdir=tmp_path), "validation failed"

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetGeometryColumn() == "new_geom_name"
        srs = lyr.GetSpatialRef()
        assert srs is not None
        assert srs.GetAuthorityCode(None) == "4326"
        ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbNone)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG
        )
        == ogr.OGRERR_NONE
    )
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetName() == "Undefined geographic SRS"

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbNone)
    new_geom_field_defn.SetSpatialRef(srs_4326)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG
        )
        == ogr.OGRERR_NONE
    )
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbNone)
    other_srs = osr.SpatialReference()
    other_srs.ImportFromEPSG(4269)
    new_geom_field_defn.SetSpatialRef(other_srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG
        )
        == ogr.OGRERR_NONE
    )
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4269"

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbNone)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4269)
    srs.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG
        )
        == ogr.OGRERR_NONE
    )
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4269"
    assert srs.GetCoordinateEpoch() == 2022
    ds = None


###############################################################################
# Test GetArrowStreamAsNumPy()


def test_ogr_gpkg_arrow_stream_numpy(tmp_vsimem):
    pytest.importorskip("osgeo.gdal_array")
    numpy = pytest.importorskip("numpy")

    filename = tmp_vsimem / "test.gpkg"

    ds = gdal.GetDriverByName("GPKG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    field = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(field)

    field = ogr.FieldDefn("bool", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    assert ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "enum_domain", "", ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None}
        )
    )
    field = ogr.FieldDefn("int32", ogr.OFTInteger)
    field.SetDomainName("enum_domain")
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64", ogr.OFTInteger64)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32", ogr.OFTReal)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(field)

    field = ogr.FieldDefn("date", ogr.OFTDate)
    lyr.CreateField(field)

    field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("binary", ogr.OFTBinary)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("bool", 1)
    f.SetField("int16", -12345)
    f.SetField("int32", 12345678)
    f.SetField("int64", 12345678901234)
    f.SetField("float32", 1.25)
    f.SetField("float64", 1.250123)
    f.SetField("str", "abc")
    f.SetField("date", "2022-05-31")
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    f.SetField("binary", b"\xDE\xAD")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)

    f2 = ogr.Feature(lyr.GetLayerDefn())
    f2.SetField("bool", 0)
    lyr.CreateFeature(f2)

    f3 = ogr.Feature(lyr.GetLayerDefn())
    f3.SetField("int16", 123)
    f3.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(-1 2)"))
    lyr.CreateFeature(f3)

    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    stream = lyr.GetArrowStream()
    array = stream.GetNextRecordBatch()
    assert array.GetChildrenCount() == 12
    del array
    del stream
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)

    try:
        import pyarrow

        pyarrow.__version__
        has_pyarrow = True
    except ImportError:
        has_pyarrow = False
    if has_pyarrow:
        stream = lyr.GetArrowStreamAsPyArrow()
        batches = [batch for batch in stream]
        # print(batches)

    for i in range(2):
        with gdaltest.config_options(
            {"OGR_GPKG_STREAM_BASE_IMPL": "YES"} if i == 1 else {}
        ):
            stream = lyr.GetArrowStreamAsNumPy(
                options=["USE_MASKED_ARRAYS=NO", "MAX_FEATURES_IN_BATCH=2"]
            )
            batches = [batch for batch in stream]
            assert len(batches) == 2
            batch = batches[0]

            assert batch.keys() == {
                "fid",
                "str",
                "bool",
                "int16",
                "int32",
                "int64",
                "float32",
                "float64",
                "date",
                "datetime",
                "binary",
                "geom",
            }

            assert batch["fid"][0] == 1
            assert len(batch["fid"]) == 2
            for fieldname in ("bool", "int16", "int32", "int64", "float32", "float64"):
                assert batch[fieldname][0] == f.GetField(fieldname)
            assert batch["str"][0] == f.GetField("str").encode("utf-8")
            assert batch["date"][0] == numpy.datetime64("2022-05-31")
            assert batch["datetime"][0] == numpy.datetime64("2022-05-31T12:34:56.789")
            assert bytes(batch["binary"][0]) == b"\xDE\xAD"
            assert len(bytes(batch["geom"][0])) == 21

            assert batch["fid"][1] == 2
            assert batch["bool"][1] == False
            assert batch["geom"][1] is None

            batch = batches[1]
            assert batch.keys() == {
                "fid",
                "str",
                "bool",
                "int16",
                "int32",
                "int64",
                "float32",
                "float64",
                "date",
                "datetime",
                "binary",
                "geom",
            }

            assert batch["fid"][0] == 3
            assert batch["int16"][0] == 123
            assert len(batch["fid"]) == 1

            assert lyr.SetNextByIndex(1) == ogr.OGRERR_NONE
            stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
            batches = [batch for batch in stream]
            assert len(batches) == 1
            assert list(batches[0]["fid"]) == [2, 3]

    with ds.ExecuteSQL("SELECT * FROM test") as sql_lyr:
        assert sql_lyr.SetNextByIndex(1) == ogr.OGRERR_NONE
        stream = sql_lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]

    with lyr.GetArrowStreamAsNumPy(options=["MAX_FEATURES_IN_BATCH=1"]) as stream:
        batches = [batch for batch in stream]
        assert len(batches) == 3
        assert len(batches[0]["fid"]) == 1
        assert batches[0]["fid"][0] == 1
        assert len(batches[1]["fid"]) == 1
        assert batches[1]["fid"][0] == 2
        assert len(batches[2]["fid"]) == 1
        assert batches[2]["fid"][0] == 3

    for i in range(2):
        with lyr.GetArrowStreamAsNumPy(options=["MAX_FEATURES_IN_BATCH=1"]) as stream:
            batch = stream.GetNextRecordBatch()
            assert len(batch["fid"]) == 1, i
            assert batch["fid"][0] == 1, i

    lyr.SetIgnoredFields(
        [
            lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef()
            for i in range(lyr.GetLayerDefn().GetFieldCount())
        ]
    )
    with lyr.GetArrowStreamAsNumPy(options=["INCLUDE_FID=NO"]) as stream:
        batch = stream.GetNextRecordBatch()
        assert len(batch["geom"]) == 3, i
        assert len(batch["geom"][0]) > 0, i
    lyr.SetIgnoredFields([])

    # Test attribute filter
    lyr.SetAttributeFilter("int16 = 123")
    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert len(batches) == 1
    assert len(batches[0]["fid"]) == 1
    assert batches[0]["fid"][0] == 3

    for i in range(2):
        lyr.SetAttributeFilter("1 = 1")
        with lyr.GetArrowStreamAsNumPy(options=["MAX_FEATURES_IN_BATCH=1"]) as stream:
            batch = stream.GetNextRecordBatch()
            assert len(batch["fid"]) == 1, i
            assert batch["fid"][0] == 1, i
            batch = stream.GetNextRecordBatch()
            assert len(batch["fid"]) == 1, i
            assert batch["fid"][0] == 2, i
        lyr.SetAttributeFilter(None)

    # Test spatial filter
    lyr.SetSpatialFilterRect(0, 0, 10, 10)
    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    lyr.SetSpatialFilter(None)
    assert len(batches) == 1
    assert len(batches[0]["fid"]) == 1
    assert batches[0]["fid"][0] == 1

    # Test ignored fields
    assert lyr.SetIgnoredFields(["geom", "int16"]) == ogr.OGRERR_NONE
    stream = lyr.GetArrowStreamAsNumPy(options=["INCLUDE_FID=NO"])
    batches = [batch for batch in stream]
    lyr.SetIgnoredFields([])
    batch = batches[0]
    assert batch.keys() == {
        "str",
        "bool",
        "int32",
        "int64",
        "float32",
        "float64",
        "date",
        "datetime",
        "binary",
    }

    # Check that OGR_GPKG_FillArrowArray_INTERNAL() function is no longer
    # registered
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM pragma_function_list WHERE name=lower('OGR_GPKG_FillArrowArray_INTERNAL')"
        )
    if sql_lyr:
        fc = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert fc == 0

    ds = None

    ogr.GetDriverByName("GPKG").DeleteDataSource(filename)


###############################################################################


@pytest.mark.parametrize("layer_type", ["direct", "sql"])
def test_ogr_gpkg_arrow_stream_numpy_detailed_spatial_filter(tmp_vsimem, layer_type):
    pytest.importorskip("osgeo.gdal_array")
    pytest.importorskip("numpy")

    filename = str(
        tmp_vsimem / "test_ogr_parquet_arrow_stream_numpy_detailed_spatial_filter.gpkg"
    )
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("test", options=["FID=fid"])
    for idx, wkt in enumerate(
        [
            "POINT(1 2)",
            "MULTIPOINT(0 0,1 2)",
            "LINESTRING(3 4,5 6)",
            "MULTILINESTRING((7 8,7.5 8.5),(3 4,5 6))",
            "POLYGON((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21))",
            "MULTIPOLYGON(((100 100,100 200,200 200,100 100)),((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21)))",
            "LINESTRING EMPTY",
            "MULTILINESTRING EMPTY",
            "POLYGON EMPTY",
            "MULTIPOLYGON EMPTY",
            "GEOMETRYCOLLECTION EMPTY",
        ]
    ):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(idx)
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    if layer_type == "direct":
        lyr = ds.GetLayer(0)
    else:
        lyr = ds.ExecuteSQL("SELECT * FROM test")

    eps = 1e-1

    # Select nothing
    with ogrtest.spatial_filter(lyr, 6, 0, 8, 1):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert list(batches[0]["fid"]) == []

    # Select POINT and MULTIPOINT
    with ogrtest.spatial_filter(lyr, 1 - eps, 2 - eps, 1 + eps, 2 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [0, 1]
        assert [f.GetFID() for f in lyr] == [0, 1]

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 3 - eps, 4 - eps, 3 + eps, 4 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 5 - eps, 6 - eps, 5 + eps, 6 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select LINESTRING and MULTILINESTRING due to more generic intersection
    with ogrtest.spatial_filter(lyr, 4 - eps, 5 - eps, 4 + eps, 5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [2, 3]
        assert [f.GetFID() for f in lyr] == [2, 3]

    # Select POLYGON and MULTIPOLYGON due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 10 - eps, 20 - eps, 10 + eps, 20 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == [4, 5]
        assert [f.GetFID() for f in lyr] == [4, 5]

    # bbox with polygon hole
    with ogrtest.spatial_filter(lyr, 12 - eps, 20.5 - eps, 12 + eps, 20.5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        if ogrtest.have_geos():
            assert list(batches[0]["fid"]) == []
        else:
            assert len(batches) == 1
            assert list(batches[0]["fid"]) == [4, 5]
            assert [f.GetFID() for f in lyr] == [4, 5]

    if layer_type != "direct":
        ds.ReleaseResultSet(lyr)

    ds = None


###############################################################################
# Test opening a file in WAL mode on a read-only storage


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
def test_ogr_gpkg_immutable(tmp_path):

    if os.getuid() == 0:
        pytest.skip("running as root... skipping")

    os.mkdir(tmp_path / "read_only_test_ogr_gpkg_immutable", 0o755)

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(
        tmp_path / "read_only_test_ogr_gpkg_immutable/test.gpkg"
    )
    ds.CreateLayer("foo")
    ds.ExecuteSQL("PRAGMA journal_mode = WAL")
    ds = None

    # Turn directory in read-only mode
    os.chmod(tmp_path / "read_only_test_ogr_gpkg_immutable", 0o555)

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx(
                tmp_path / "read_only_test_ogr_gpkg_immutable/test.gpkg",
                gdal.OF_VECTOR | gdal.OF_UPDATE,
            )
            is None
        )
        assert (
            gdal.OpenEx(
                tmp_path / "read_only_test_ogr_gpkg_immutable/test.gpkg",
                gdal.OF_VECTOR,
                open_options=["IMMUTABLE=NO"],
            )
            is None
        )

    gdal.ErrorReset()
    assert (
        gdal.OpenEx(
            tmp_path / "read_only_test_ogr_gpkg_immutable/test.gpkg",
            gdal.OF_VECTOR,
            open_options=["IMMUTABLE=YES"],
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            ogr.Open(tmp_path / "read_only_test_ogr_gpkg_immutable/test.gpkg")
            is not None
        )
    assert gdal.GetLastErrorMsg() != ""


###############################################################################


@pytest.mark.skipif(
    get_sqlite_version() < (3, 24, 0),
    reason="sqlite >= 3.24 needed",
)
@pytest.mark.parametrize("with_geom", [True, False])
@pytest.mark.parametrize("gpkg_version", ["1.2", "1.4"])
def test_ogr_gpkg_upsert_without_fid(tmp_vsimem, tmp_path, with_geom, gpkg_version):

    filename = tmp_vsimem / "test_ogr_gpkg_upsert_without_fid.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(
        filename, options=["VERSION=" + gpkg_version]
    )
    lyr = ds.CreateLayer(
        "foo", geom_type=(ogr.wkbUnknown if with_geom else ogr.wkbNone)
    )
    assert lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString)) == ogr.OGRERR_NONE
    unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
    unique_field.SetUnique(True)
    assert lyr.CreateField(unique_field) == ogr.OGRERR_NONE
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("unique_field", i + 1)
        if i < 4 and with_geom:
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("unique_field", "2")
    f.SetField("other", "foo")
    if with_geom:
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (10 10)"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE

    if get_sqlite_version() >= (3, 35, 0):
        assert f.GetFID() == 2

    if with_geom and gpkg_version == "1.2":
        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update1'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 0
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update6'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 1
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update7'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 1
        ds.ReleaseResultSet(sql_lyr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("unique_field", "3")
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("unique_field", "4")
    if with_geom:
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (20 20)"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE

    ds = None

    assert validate(filename, tmpdir=tmp_path)

    ds = ogr.Open(filename)

    if with_geom and gpkg_version == "1.2":
        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update1'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 1
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update6'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 0
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL(
            "SELECT 1 FROM sqlite_master WHERE name = 'rtree_foo_geom_update7'",
            dialect="DEBUG",
        )
        assert sql_lyr.GetFeatureCount() == 0
        ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(2)
    assert f["unique_field"] == "2"
    assert f["other"] == "foo"
    if with_geom:
        assert f.GetGeometryRef().ExportToWkt() == "POINT (10 10)"

    f = lyr.GetFeature(3)
    assert f.GetGeometryRef() is None

    f = lyr.GetFeature(4)
    if with_geom:
        assert f.GetGeometryRef().ExportToWkt() == "POINT (20 20)"

    ds = None


###############################################################################


def test_ogr_gpkg_get_geometry_types(tmp_vsimem):
    """Test Layer.GetGeometryTypes()"""

    filename = tmp_vsimem / "test_ogr_gpkg_get_geometry_types.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("layer")

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
    lyr.SetAttributeFilter("1")
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 2, ogr.wkbPoint: 1}
    lyr.SetAttributeFilter("0")
    assert lyr.GetGeometryTypes() == {}
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
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0,1 1)"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
    }
    assert lyr.GetGeometryTypes(geom_field=0, flags=ogr.GGT_STOP_IF_MIXED) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION Z(TIN Z(((0 0 0,0 1 0,1 1 0,0 0 0))))"
        )
    )
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
        ogr.wkbGeometryCollection25D: 1,
    }
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
        ogr.wkbTINZ: 1,
    }

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            lyr.GetGeometryTypes(geom_field=1)

    lyr.StartTransaction()
    for _ in range(
        1000
    ):  # 1000 because COUNT_VM_INSTRUCTIONS = 1000 in OGRGeoPackageTableLayer::GetGeometryTypes()
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
        lyr.CreateFeature(f)
    lyr.CommitTransaction()
    with gdal.quiet_errors():
        with pytest.raises(Exception):
            lyr.GetGeometryTypes(callback=lambda x, y, z: 0)

    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1001,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
        ogr.wkbGeometryCollection25D: 1,
    }

    ds = None


###############################################################################


@pytest.mark.parametrize("write_to_disk", (True, False), ids=["on_disk", "in_memory"])
def test_ogr_gpkg_background_rtree_build(tmp_path, tmp_vsimem, write_to_disk):

    if write_to_disk:
        filename = tmp_path / "test_ogr_gpkg_background_rtree_build.gpkg"
    else:
        filename = tmp_vsimem / "test_ogr_gpkg_background_rtree_build.gpkg"

    # Batch insertion only

    gdal.ErrorReset()
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    with gdaltest.config_option("OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "YES"):
        lyr = ds.CreateLayer("foo")
    assert lyr.StartTransaction() == ogr.OGRERR_NONE
    for i in range(1000):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(%d %d)" % (i, i)))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        if i == 500:
            assert lyr.CommitTransaction() == ogr.OGRERR_NONE
            assert lyr.StartTransaction() == ogr.OGRERR_NONE
    assert lyr.CommitTransaction() == ogr.OGRERR_NONE
    assert gdal.GetLastErrorMsg() == ""

    with gdaltest.config_option("OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "YES"):
        lyr = ds.CreateLayer("bar")
    assert lyr.StartTransaction() == ogr.OGRERR_NONE
    for i in range(900):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(%d %d)" % (-i, -i)))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        if i == 500:
            assert lyr.CommitTransaction() == ogr.OGRERR_NONE
            assert lyr.StartTransaction() == ogr.OGRERR_NONE
    assert lyr.CommitTransaction() == ogr.OGRERR_NONE
    assert gdal.GetLastErrorMsg() == ""

    ds = None
    assert gdal.VSIStatL(filename.with_suffix(".gpkg.tmp_rtree_foo.db")) is None
    assert gdal.VSIStatL(filename.with_suffix(".gpkg.tmp_rtree_bar.db")) is None

    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_foo_geom")
    assert sql_lyr.GetFeatureCount() == 1000
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_bar_geom")
    assert sql_lyr.GetFeatureCount() == 900
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)

    # Test SetFeature() after batch insertion
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    with gdaltest.config_option("OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "YES"):
        lyr = ds.CreateLayer("footoooooooooooooooooooooooooooooooooooooooooolong")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM rtree_footoooooooooooooooooooooooooooooooooooooooooolong_geom"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(0.5, 0.5, 1.5, 1.5)
    assert lyr.GetFeatureCount() == 1
    ds = None

    gdal.Unlink(filename)

    # Test DeleteFeature() after batch insertion
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    with gdaltest.config_option("OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "YES"):
        lyr = ds.CreateLayer("foo with space")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    assert lyr.DeleteFeature(f.GetFID()) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM "rtree_foo with space_geom"')
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Test RollbackTransaction() after batch insertion
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    with gdaltest.config_option("OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "YES"):
        lyr = ds.CreateLayer("foo")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    lyr.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    lyr.RollbackTransaction()
    ds = None

    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_foo_geom")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(-0.5, -0.5, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################


def test_ogr_gpkg_detect_broken_rtree_gdal_3_6_0(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_detect_broken_rtree_gdal_3_6_0.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    for i in range(100):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(
            ogr.CreateGeometryFromWkt("POINT(%d %d)" % (i % 10, i // 10))
        )
        lyr.CreateFeature(f)
    ds = None

    # Voluntary corrupt the RTree by removing the entry for the last feature
    ds = ogr.Open(filename, update=1)
    sql_lyr = ds.ExecuteSQL("DELETE FROM rtree_foo_geom WHERE id = 100")
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    with gdaltest.config_option("OGR_GPKG_THRESHOLD_DETECT_BROKEN_RTREE", "100"):
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        with gdal.quiet_errors():
            gdal.ErrorReset()
            lyr.SetSpatialFilterRect(8.5, 8.5, 9.5, 9.5)
            assert (
                "Spatial index (perhaps created with GDAL 3.6.0) of table foo is corrupted"
                in gdal.GetLastErrorMsg()
            )
        assert lyr.GetFeatureCount() == 1
        ds = None


###############################################################################
# Test ST_Area()


@pytest.mark.parametrize(
    "wkt_or_binary,area",
    [
        (None, None),
        ("X'0001'", None),
        ("POINT EMPTY", 0),
        ("LINESTRING(1 2,3 4)", 0),
        ("POLYGON EMPTY", 0),
        ("POLYGON ((0 0,0 1,1 1,0 0))", 0.5),
        ("POLYGON Z ((0 0 100,0 1 100,1 1 100,0 0 100))", 0.5),
        ("POLYGON M ((0 0 100,0 1 100,1 1 100,0 0 100))", 0.5),
        ("POLYGON ZM ((0 0 100 200,0 1 100 200,1 1 100 200,0 0 100 200))", 0.5),
        (
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))",
            0.75,
        ),
        ("MULTIPOLYGON EMPTY", 0),
        ("MULTIPOLYGON (((0 0,0 1,1 1,0 0)))", 0.5),
        ("MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))", 1),
        ("MULTIPOLYGON Z (((0 0 100,0 1 100,1 1 100,0 0 100)))", 0.5),
        ("MULTIPOLYGON M (((0 0 100,0 1 100,1 1 100,0 0 100)))", 0.5),
        ("MULTIPOLYGON ZM (((0 0 100 200,0 1 100 200,1 1 100 200,0 0 100 200)))", 0.5),
        ("CURVEPOLYGON ((0 0,0 1,1 1,0 0))", 0.5),
        ("MULTISURFACE (((0 0,0 1,1 1,0 0)))", 0.5),
        # Below is Spatialite encoding of POLYGON((0 0,0 1,1 1,0 0))
        (
            "X'00010000000000000000000000000000000000000000000000000000f03f000000000000f03f7c030000000100000004000000000000000000000000000000000000000000000000000000000000000000f03f000000000000f03f000000000000f03f00000000000000000000000000000000fe'",
            0.5,
        ),
    ],
)
def test_ogr_gpkg_st_area(tmp_vsimem, wkt_or_binary, area):

    filename = tmp_vsimem / "test_ogr_gpkg_st_area.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("test")
    if wkt_or_binary and wkt_or_binary.startswith("X'"):
        sql = "INSERT INTO test(geom) VALUES (" + wkt_or_binary + ")"
        ds.ExecuteSQL(sql)
    else:
        f = ogr.Feature(lyr.GetLayerDefn())
        if wkt_or_binary:
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt_or_binary))
        lyr.CreateFeature(f)
    sql_lyr = ds.ExecuteSQL("SELECT ST_Area(geom) FROM test")
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink(filename)
    assert f.GetField(0) == area


###############################################################################
# Test reading a layer with a generated column


@pytest.mark.skipif(
    get_sqlite_version() < (3, 31, 0),
    reason="sqlite >= 3.31 needed",
)
def test_ogr_gpkg_read_generated_column(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_read_generated_column.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    ds.ExecuteSQL(
        "CREATE TABLE test (fid INTEGER PRIMARY KEY NOT NULL,unused TEXT,strfield TEXT,strfield_generated TEXT GENERATED ALWAYS AS (strfield || '_generated'),intfield_generated_stored INTEGER GENERATED ALWAYS AS (5) STORED)"
    )
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name,data_type,identifier,description,last_change,srs_id) VALUES ('test','attributes','test','','',0)"
    )
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 4
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetName() == "strfield_generated"
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetName() == "intfield_generated_stored"
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTInteger64

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("strfield", "foo")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["strfield"] == "foo"
    assert f["strfield_generated"] == "foo_generated"
    assert f["intfield_generated_stored"] == 5

    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["strfield"] == "foo"
    assert f["strfield_generated"] == "foo_generated"

    f.SetField("strfield", "bar")
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["strfield"] == "bar"
    assert f["strfield_generated"] == "bar_generated"

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("strfield", "foo2")
    f.SetField("strfield_generated", "ignored")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = lyr.GetFeature(2)
    assert f["strfield"] == "foo2"
    assert f["strfield_generated"] == "foo2_generated"
    f = None

    assert lyr.DeleteField(0) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("strfield", "foo3")
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = lyr.GetFeature(3)
    assert f["strfield"] == "foo3"
    # None for sqlite < 3.35.5 that uses table recreation for DeleteField() implementation
    # and thus for now the generated column expression is lost
    assert (
        f["strfield_generated"] == "foo3_generated" or f["strfield_generated"] is None
    )

    ds = None


###############################################################################
# Test gdal_get_pixel_value() function


def test_ogr_gpkg_sql_gdal_get_pixel_value(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_sql_gdal_get_pixel_value.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)

    with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'georef', 440780, 3751080)"
        )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 156

    with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'pixel', 1, 4)"
        )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 156

    with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_pixel_value('../gcore/data/float64.tif', 1, 'pixel', 0, 1)"
        )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 115.0

    # Invalid column
    with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'pixel', -1, 0)"
        )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] is None

    # Missing OGR_SQLITE_ALLOW_EXTERNAL_ACCESS
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'georef', 440720, 3751320)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 1st arg
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value(NULL, 1, 'pixel', 0, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 2nd arg
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', NULL, 'pixel', 0, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 3rd arg
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, NULL, 0, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 4th arg
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'pixel', NULL, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 5th arg
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'pixel', 0, NULL)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # Invalid band number
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', 0, 'pixel', 0, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # Invalid value for 3rd argument
    with gdal.quiet_errors():
        with gdaltest.config_option("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "YES"):
            sql_lyr = ds.ExecuteSQL(
                "select gdal_get_pixel_value('../gcore/data/byte.tif', 1, 'invalid', 0, 0)"
            )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None


###############################################################################
# Test SOZip writing and reading


def test_ogr_gpkg_sozip(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_sozip.gpkg.zip"

    with gdaltest.config_options(
        {"CPL_SOZIP_MIN_FILE_SIZE": "256", "CPL_VSIL_DEFLATE_CHUNK_SIZE": "128"}
    ):
        ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
        ds.CreateLayer("foo")
        ds = None

    md = gdal.GetFileMetadata(f"/vsizip/{filename}/test_ogr_gpkg_sozip.gpkg", "ZIP")
    assert md["SOZIP_VALID"] == "YES"

    ds = ogr.Open(filename)
    assert ds
    assert ds.GetLayer(0).GetName() == "foo"
    ds = None


###############################################################################
# Test inserting a non-spatial layer into a database that has non-spatial
# layers which are not registered in gpkg_contents
# Cf https://github.com/qgis/QGIS/issues/51721


@pytest.mark.parametrize("with_gpkg_ogr_contents", [True, False])
def test_ogr_gpkg_add_non_spatial_layer_in_existing_database_with_unregistered(
    tmp_vsimem,
    tmp_path,
    with_gpkg_ogr_contents,
):

    filename = (
        tmp_vsimem
        / "ogr_gpkg_add_non_spatial_layer_in_existing_database_with_unregistered.gpkg"
    )
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.CreateLayer("point", geom_type=ogr.wkbPoint)
    ds.ExecuteSQL(
        "CREATE TABLE non_spatial(fid INTEGER PRIMARY KEY AUTOINCREMENT, str TEXT)"
    )
    ds = None

    if not with_gpkg_ogr_contents:
        ds = ogr.Open(filename, update=1)
        ds.ExecuteSQL("DROP TABLE gpkg_ogr_contents")
        ds = None

    ds = ogr.Open(filename, update=1)
    assert ds.GetLayerCount() == 2
    assert set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())) == set(
        ["point", "non_spatial"]
    )
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    assert ds.CreateLayer("non_spatial2", geom_type=ogr.wkbNone) is not None
    ds = None

    assert validate(filename, tmpdir=tmp_path), "validation failed"

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 3
    assert set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())) == set(
        ["point", "non_spatial", "non_spatial2"]
    )
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    if with_gpkg_ogr_contents:
        sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_ogr_contents")
        assert sql_lyr.GetFeatureCount() == 2
        ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test UpdateFeature()


def test_ogr_gpkg_update_feature(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_update_feature.gpkg.zip"

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    f["int_field"] = 1
    f["str_field"] = "foo"
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.TestCapability(ogr.OLCUpdateFeature) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f["int_field"] = 123  # will be ignored
    f["str_field"] = "bar"
    assert lyr.UpdateFeature(f, [1], [], False) == ogr.OGRERR_NONE
    # Check recycling of existing statement
    f["str_field"] = "baz"
    assert lyr.UpdateFeature(f, [1], [], False) == ogr.OGRERR_NONE
    f = lyr.GetFeature(1)
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    assert f["int_field"] == 1
    assert f["str_field"] == "baz"

    # Do not modify unset fields
    f.UnsetField(1)
    assert lyr.UpdateFeature(f, [1], [], False) == ogr.OGRERR_NONE
    f = lyr.GetFeature(1)
    assert f["str_field"] == "baz"

    # Nullify geometry
    f.SetGeometry(None)
    assert lyr.UpdateFeature(f, [], [0], False) == ogr.OGRERR_NONE
    f = lyr.GetFeature(1)
    assert f.GetGeometryRef() is None

    # Set non null geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.UpdateFeature(f, [], [0], False) == ogr.OGRERR_NONE
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

    ds = None


###############################################################################
# Test ogr_layer_Extent()


def test_ogr_gpkg_ogr_layer_Extent(tmp_vsimem):

    tmpfilename = tmp_vsimem / "test_ogr_gpkg_ogr_layer_Extent.gpkg"

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmpfilename)
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


###############################################################################
# Test field alternative names and comments


def test_ogr_gpkg_field_alternative_names_comment(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_alternative_names.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("baz", ogr.OFTString))

    # with no gpkg_data_columns table
    lyr = ds.GetLayer("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == ""

    ds.ExecuteSQL(
        """CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT gdc_tn UNIQUE (table_name, name)
)"""
    )
    # name same as column name, won't be used as alternative name
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns('table_name', 'column_name', 'name', 'description') VALUES ('test', 'foo', 'foo', 'my description')"
    )
    ds = None

    ds = gdal.OpenEx(dbname, gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == "my description"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == ""

    # name different from column name, should be used as alternative names
    ds.ExecuteSQL("DELETE FROM gpkg_data_columns")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_data_columns('table_name', 'column_name', 'name') VALUES ('test', 'foo', 'Foo field')"
    )
    ds = None

    ds = gdaltest.gpkg_dr.Open(dbname)
    lyr = ds.GetLayer("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "Foo field"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    ds = None


###############################################################################
# Test altering field definition to add alternative names and comments


def test_ogr_gpkg_field_alter_field_defn_alternative_names_comment(tmp_vsimem):

    dbname = tmp_vsimem / "ogr_gpkg_alternative_names_alter_defn.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("baz", ogr.OFTString))

    # with no gpkg_data_columns table
    lyr = ds.GetLayer("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == ""

    foo_with_alternative_name = ogr.FieldDefn("foo")
    foo_with_alternative_name.SetAlternativeName("alt foo name")

    ret = lyr.AlterFieldDefn(0, foo_with_alternative_name, ogr.ALTER_ALL_FLAG)
    assert ret == 0

    baz_with_comment = ogr.FieldDefn("baz")
    baz_with_comment.SetComment("baz comment")

    ret = lyr.AlterFieldDefn(1, baz_with_comment, ogr.ALTER_ALL_FLAG)
    assert ret == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "alt foo name"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == "baz comment"

    del lyr
    ds = None

    ds = gdal.OpenEx(dbname, gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer("test")
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "alt foo name"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == "baz comment"

    # create field
    field_defn = ogr.FieldDefn("third", ogr.OFTString)
    field_defn.SetAlternativeName("third alias")
    field_defn.SetComment("third comment")
    assert lyr.CreateField(field_defn) == 0

    del lyr
    ds = None

    ds = gdal.OpenEx(dbname, gdal.OF_VECTOR)
    lyr = ds.GetLayer("test")

    assert lyr.GetLayerDefn().GetFieldCount() == 3
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "alt foo name"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "baz"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetAlternativeName() == ""
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == "baz comment"
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetName() == "third"
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetAlternativeName() == "third alias"
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetComment() == "third comment"
    ds = None


###############################################################################
# Test RTree triggers


@pytest.mark.parametrize("gpkg_version", ["1.2", "1.4"])
def test_ogr_gpkg_rtree_triggers(tmp_vsimem, gpkg_version):
    def get_rtree_entry_count(ds):
        with ds.ExecuteSQL("SELECT * FROM rtree_test_geom") as sql_lyr:
            return sql_lyr.GetFeatureCount()

    dbname = tmp_vsimem / "test_ogr_gpkg_rtree_triggers.gpkg"

    ds = gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=" + gpkg_version])
    ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)

    # Create a feature without geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Update the feature with a geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(1, 2, 1, 2)
    assert lyr.GetFeatureCount() == 1

    # Update the feature with another geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(3, 4, 3, 4)
    assert lyr.GetFeatureCount() == 1

    # Upsert the feature with another geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (5 6)"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(5, 6, 5, 6)
    assert lyr.GetFeatureCount() == 1

    # Upsert the feature without geometry
    f.SetGeometry(None)
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Upsert the feature with another geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (7 8)"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(7, 8, 7, 8)
    assert lyr.GetFeatureCount() == 1

    # Upsert the feature with empty geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Upsert the feature with a geometry
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (7 8)"))
    assert lyr.UpsertFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(7, 8, 7, 8)
    assert lyr.GetFeatureCount() == 1

    # Remove the geometry
    f.SetGeometry(None)
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Delete the geometry
    assert lyr.DeleteFeature(f.GetFID()) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Create a feature with a geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (9 10)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 1
    lyr.SetSpatialFilterRect(9, 10, 9, 10)
    assert lyr.GetFeatureCount() == 1

    # Delete the geometry
    assert lyr.DeleteFeature(f.GetFID()) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Create a feature with a empty geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    # Delete the geometry
    assert lyr.DeleteFeature(f.GetFID()) == ogr.OGRERR_NONE
    assert get_rtree_entry_count(ds) == 0

    ds = None


###############################################################################
# Test relaxed DATETIME format for GeoPackage 1.4
# (https://github.com/OSGeo/gdal/issues/8037)


def test_ogr_gpkg_1_4_relaxed_datetime_format(tmp_vsimem, tmp_path):
    dbname = tmp_vsimem / "test_ogr_gpkg_1_4_relaxed_datetime_format.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=1.4"])
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("dt", "2023-11-07T16:03:34Z")
    lyr.CreateFeature(f)
    f = None

    # Check we have written without milliseconds
    with ds.ExecuteSQL("SELECT CAST(dt AS VARCHAR) AS dt FROM test") as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f["dt"] == "2023-11-07T16:03:34Z"

    # Test datetime without seconds
    ds.ExecuteSQL("INSERT INTO test (dt) VALUES ('2023-11-07T16:03Z')")
    ds = None

    validate(dbname, tmpdir=tmp_path)

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["dt"] == "2023/11/07 16:03:34+00"
    f = lyr.GetNextFeature()
    assert f["dt"] == "2023/11/07 16:03:00+00"
    ds = None


###############################################################################
# Test relaxed DATETIME format for GeoPackage 1.4
# (https://github.com/OSGeo/gdal/issues/8037)


@pytest.mark.parametrize(
    "version,datetime_precision,input,output",
    [
        ("1.4", "AUTO", "2023-11-07T16:03:34.123Z", "2023-11-07T16:03:34.123Z"),
        ("1.4", "AUTO", "2023-11-07T16:03:34Z", "2023-11-07T16:03:34Z"),
        ("1.3", "AUTO", "2023-11-07T16:03:34.123Z", "2023-11-07T16:03:34.123Z"),
        ("1.3", "AUTO", "2023-11-07T16:03:34Z", "2023-11-07T16:03:34.000Z"),
        ("1.4", "MILLISECOND", "2023-11-07T16:03:34.123Z", "2023-11-07T16:03:34.123Z"),
        ("1.4", "MILLISECOND", "2023-11-07T16:03:34Z", "2023-11-07T16:03:34.000Z"),
        ("1.4", "SECOND", "2023-11-07T16:03:34.123Z", "2023-11-07T16:03:34Z"),
        ("1.4", "MINUTE", "2023-11-07T16:03:34.123Z", "2023-11-07T16:03Z"),
        ("1.4", "INVALID", None, None),
    ],
)
@gdaltest.enable_exceptions()
def test_ogr_gpkg_1_4_DATETIME_PRECISION(
    tmp_vsimem, version, datetime_precision, input, output
):
    dbname = tmp_vsimem / "test_ogr_gpkg_1_4_DATETIME_PRECISION.gpkg"
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname, options=["VERSION=" + version])
    if datetime_precision == "INVALID":
        with pytest.raises(Exception), gdaltest.error_handler():
            lyr = ds.CreateLayer(
                "test", options=["DATETIME_PRECISION=" + datetime_precision]
            )
    else:
        lyr = ds.CreateLayer(
            "test", options=["DATETIME_PRECISION=" + datetime_precision]
        )
        lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("dt", input)
        lyr.CreateFeature(f)
        f = None

        # Check we have written what we expected
        with ds.ExecuteSQL("SELECT CAST(dt AS VARCHAR) AS dt FROM test") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f["dt"] == output

    ds = None


###############################################################################
# Test FlushCache()


def test_ogr_gpkg_write_flushcache(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_gpkg_write_flushcache.gpkg"

    ds = gdal.GetDriverByName("GPKG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr1 = ds.CreateLayer(
        "test1",
    )
    lyr2 = ds.CreateLayer("test2")
    f = ogr.Feature(lyr1.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr1.CreateFeature(f)
    f = ogr.Feature(lyr2.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    lyr2.CreateFeature(f)
    ds.FlushCache()

    ds2 = ogr.Open(filename)
    assert ds2.GetLayer(0).GetFeatureCount() == 1
    assert ds2.GetLayer(1).GetFeatureCount() == 1
    ds2 = None

    f = ogr.Feature(lyr1.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    lyr1.CreateFeature(f)
    ds = None

    ds2 = ogr.Open(filename)
    assert ds2.GetLayer(0).GetFeatureCount() == 2
    assert ds2.GetLayer(1).GetFeatureCount() == 1
    ds2 = None
