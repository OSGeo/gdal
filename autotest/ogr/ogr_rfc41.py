#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC41
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest
from ogr.ogr_sql_sqlite import require_ogr_sql_sqlite  # noqa

from osgeo import gdal, ogr, osr

require_ogr_sql_sqlite
# to make pyflakes happy


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test OGRGeomFieldDefn class


def test_ogr_rfc41_1():

    gfld_defn = ogr.GeomFieldDefn()

    # Check default values
    assert gfld_defn.GetName() == ""
    assert gfld_defn.GetType() == ogr.wkbUnknown
    assert gfld_defn.GetSpatialRef() is None
    assert gfld_defn.IsIgnored() == 0

    # Test SetName() / GetName()
    gfld_defn.SetName("foo")
    assert gfld_defn.GetName() == "foo"

    # Test SetType() / GetType()
    gfld_defn.SetType(ogr.wkbPoint)
    assert gfld_defn.GetType() == ogr.wkbPoint

    # Test SetSpatialRef() / GetSpatialRef()
    sr = osr.SpatialReference()
    gfld_defn.SetSpatialRef(sr)
    got_sr = gfld_defn.GetSpatialRef()
    assert got_sr.IsSame(sr) != 0

    gfld_defn.SetSpatialRef(None)
    assert gfld_defn.GetSpatialRef() is None

    gfld_defn.SetSpatialRef(sr)

    # Test SetIgnored() / IsIgnored()
    gfld_defn.SetIgnored(1)
    assert gfld_defn.IsIgnored() == 1

    # Test setting invalid value
    old_val = gfld_defn.GetType()
    with gdal.quiet_errors():
        gfld_defn.SetType(-3)
    assert gfld_defn.GetType() == old_val

    gfld_defn = None


###############################################################################
# Test OGRFeatureDefn methods related to OGRGeomFieldDefn class


def test_ogr_rfc41_2():

    # Check implicit geometry field creation
    feature_defn = ogr.FeatureDefn()
    assert feature_defn.GetGeomFieldCount() == 1
    assert feature_defn.GetGeomType() == ogr.wkbUnknown

    # Test IsSame()
    assert feature_defn.IsSame(feature_defn) == 1
    other_feature_defn = ogr.FeatureDefn()
    assert feature_defn.IsSame(other_feature_defn) == 1
    other_feature_defn.GetGeomFieldDefn(0).SetSpatialRef(osr.SpatialReference())
    assert feature_defn.IsSame(other_feature_defn) == 0
    feature_defn.GetGeomFieldDefn(0).SetSpatialRef(osr.SpatialReference())
    assert feature_defn.IsSame(other_feature_defn) == 1
    other_feature_defn.GetGeomFieldDefn(0).SetSpatialRef(None)
    assert feature_defn.IsSame(other_feature_defn) == 0

    feature_defn = None
    feature_defn = ogr.FeatureDefn()

    # Check changing geometry type
    feature_defn.SetGeomType(ogr.wkbPoint)
    assert feature_defn.GetGeomType() == ogr.wkbPoint
    assert feature_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbPoint

    # Check setting to wkbNone and implicitly destroying the field.
    for _ in range(2):
        feature_defn.SetGeomType(ogr.wkbNone)
        assert feature_defn.GetGeomFieldCount() == 0
        assert feature_defn.GetGeomType() == ogr.wkbNone

    # Recreate the field
    for t in [ogr.wkbPoint, ogr.wkbLineString]:
        feature_defn.SetGeomType(t)
        assert feature_defn.GetGeomFieldCount() == 1
        assert feature_defn.GetGeomType() == t
        assert feature_defn.GetGeomFieldDefn(0).GetType() == t

    # Test setting invalid value
    old_val = feature_defn.GetGeomType()
    with gdal.quiet_errors():
        feature_defn.SetGeomType(-3)
    assert feature_defn.GetGeomType() == old_val

    # Test SetIgnored() / IsIgnored()
    assert feature_defn.IsGeometryIgnored() == 0
    assert feature_defn.GetGeomFieldDefn(0).IsIgnored() == 0
    feature_defn.SetGeometryIgnored(1)
    assert feature_defn.IsGeometryIgnored() == 1
    assert feature_defn.GetGeomFieldDefn(0).IsIgnored() == 1

    # Test wrong index values for GetGeomFieldDefn()
    for idx in [-1, 1]:
        with gdal.quiet_errors():
            ret = feature_defn.GetGeomFieldDefn(idx)
        assert ret is None

    # Test GetGeomFieldIndex()
    assert feature_defn.GetGeomFieldIndex("") == 0
    assert feature_defn.GetGeomFieldIndex("invalid") == -1

    # Test AddGeomFieldDefn()
    gfld_defn = ogr.GeomFieldDefn("polygon_field", ogr.wkbPolygon)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    assert feature_defn.GetGeomFieldCount() == 2
    assert feature_defn.GetGeomFieldIndex("polygon_field") == 1
    assert feature_defn.GetGeomFieldDefn(1).GetName() == "polygon_field"

    # Test DeleteGeomFieldDefn() : error cases
    assert feature_defn.DeleteGeomFieldDefn(-1) != 0
    assert feature_defn.DeleteGeomFieldDefn(2) != 0
    assert feature_defn.GetGeomFieldCount() == 2

    # Test DeleteGeomFieldDefn() : valid cases
    assert feature_defn.DeleteGeomFieldDefn(0) == 0
    assert feature_defn.GetGeomFieldCount() == 1
    assert feature_defn.GetGeomFieldIndex("polygon_field") == 0

    assert feature_defn.DeleteGeomFieldDefn(0) == 0
    assert feature_defn.GetGeomFieldCount() == 0

    assert feature_defn.IsSame(feature_defn) == 1
    assert feature_defn.IsSame(ogr.FeatureDefn()) == 0

    feature_defn = None


###############################################################################
# Test OGRFeature methods


def test_ogr_rfc41_3():

    # Test with just one geometry field
    feature_defn = ogr.FeatureDefn()
    feature = ogr.Feature(feature_defn)
    assert feature.GetGeomFieldCount() == 1
    assert feature.GetGeomFieldDefnRef(0).GetName() == ""
    assert feature.GetGeomFieldDefnRef(0).GetType() == ogr.wkbUnknown
    assert feature.GetGeomFieldIndex("") == 0
    assert feature.GetGeomFieldIndex("non_existing") == -1
    assert feature.GetGeomFieldRef(-1) is None
    assert feature.GetGeomFieldRef(0) is None
    assert feature.GetGeomFieldRef(1) is None
    feature_clone_without_geom = feature.Clone()
    assert feature.Equal(feature_clone_without_geom)
    assert feature.SetGeomField(0, ogr.Geometry(ogr.wkbPoint)) == 0
    assert feature.GetGeomFieldRef(0).ExportToWkt() == "POINT EMPTY"
    assert feature.Equal(feature.Clone())
    assert not feature.Equal(feature_clone_without_geom)
    feature_clone_with_other_geom = feature.Clone()
    feature_clone_with_other_geom.SetGeometry(ogr.Geometry(ogr.wkbLineString))
    assert not feature.Equal(feature_clone_with_other_geom)
    assert feature.SetGeomFieldDirectly(-1, None) != 0
    assert feature.SetGeomFieldDirectly(0, ogr.Geometry(ogr.wkbLineString)) == 0
    assert feature.GetGeomFieldRef(0).ExportToWkt() == "LINESTRING EMPTY"
    feature_clone_with_geom = feature.Clone()
    assert feature.SetGeomFieldDirectly(0, None) == 0
    assert feature.GetGeomFieldRef(0) is None
    assert not feature.Equal(feature_clone_with_geom)
    feature = None

    # Test one a feature with 0 geometry field
    feature_defn = ogr.FeatureDefn()
    feature_defn.SetGeomType(ogr.wkbNone)
    feature = ogr.Feature(feature_defn)
    assert feature.Equal(feature.Clone())
    assert feature.GetGeomFieldCount() == 0
    # This used to work before RFC 41, but it no longer will
    assert feature.SetGeometry(ogr.Geometry(ogr.wkbPoint)) != 0
    assert feature.SetGeomField(0, ogr.Geometry(ogr.wkbPoint)) != 0
    assert feature.GetGeometryRef() is None
    assert feature.GetGeomFieldRef(0) is None
    assert feature.SetGeometryDirectly(ogr.Geometry(ogr.wkbPoint)) != 0
    assert feature.SetGeomFieldDirectly(0, ogr.Geometry(ogr.wkbPoint)) != 0
    feature = None

    # Test one a feature with several geometry fields
    feature_defn = ogr.FeatureDefn()
    feature_defn.SetGeomType(ogr.wkbNone)
    gfld_defn = ogr.GeomFieldDefn("polygon_field", ogr.wkbPolygon)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    gfld_defn = ogr.GeomFieldDefn("point_field", ogr.wkbPoint)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    feature = ogr.Feature(feature_defn)
    feature.SetGeomField(0, ogr.Geometry(ogr.wkbPolygon))
    feature.SetGeomField(1, ogr.Geometry(ogr.wkbPoint))
    assert feature.GetGeomFieldRef(0).ExportToWkt() == "POLYGON EMPTY"
    assert feature.GetGeomFieldRef(1).ExportToWkt() == "POINT EMPTY"
    assert feature.Equal(feature.Clone())
    other_feature = ogr.Feature(feature_defn)
    assert not feature.Equal(other_feature)
    other_feature.SetFrom(feature)
    assert feature.Equal(other_feature)

    # Test that in SetFrom() where target has a single geometry field,
    # we get the first geometry of the source even if we cannot find a
    # source geometry field with the right name.
    feature_defn_default = ogr.FeatureDefn()
    feature_default = ogr.Feature(feature_defn_default)
    feature_default.SetFrom(feature)
    assert feature_default.GetGeomFieldRef(0).ExportToWkt() == "POLYGON EMPTY"


###############################################################################
# Test OGRLayer methods


def test_ogr_rfc41_4():

    ds = ogr.GetDriverByName("memory").CreateDataSource("")
    assert ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) != 0
    sr = osr.SpatialReference()
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, srs=sr)
    assert lyr.TestCapability(ogr.OLCCreateGeomField) != 0
    assert lyr.GetSpatialRef().IsSame(sr) != 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().IsSame(sr) != 0
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(feat)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.GetSpatialReference().IsSame(sr) != 0
    feat = None
    lyr.CreateGeomField(ogr.GeomFieldDefn("another_geom_field", ogr.wkbPolygon))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetGeomField(
        1, ogr.CreateGeometryFromWkt("POLYGON ((10 10,10 11,11 11,11 10,10 10))")
    )
    lyr.SetFeature(feat)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef(0)
    assert geom.ExportToWkt() == "POINT (1 2)"
    geom = feat.GetGeomFieldRef("another_geom_field")
    assert geom.ExportToWkt() == "POLYGON ((10 10,10 11,11 11,11 10,10 10))"

    # Test GetExtent()
    got_extent = lyr.GetExtent(geom_field=1)
    assert got_extent == (10.0, 11.0, 10.0, 11.0)
    # Test invalid geometry field index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        got_extent = lyr.GetExtent(geom_field=2)
    assert gdal.GetLastErrorMsg() != ""

    # Test SetSpatialFilter()
    lyr.SetSpatialFilter(
        1, ogr.CreateGeometryFromWkt("POLYGON ((-10 10,-10 11,-11 11,-11 10,-10 10))")
    )
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None
    lyr.SetSpatialFilter(
        1, ogr.CreateGeometryFromWkt("POLYGON ((10 10,10 11,11 11,11 10,10 10))")
    )
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None
    lyr.SetSpatialFilterRect(1, 10, 10, 11, 11)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None
    # Test invalid spatial filter index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.SetSpatialFilterRect(2, 0, 0, 0, 0)
    assert gdal.GetLastErrorMsg() != ""

    lyr.SetSpatialFilter(None)
    another_lyr = ds.CopyLayer(lyr, "dup_test")
    dup_feat = another_lyr.GetNextFeature()
    geom = dup_feat.GetGeomFieldRef("")
    assert geom.ExportToWkt() == "POINT (1 2)"
    geom = dup_feat.GetGeomFieldRef("another_geom_field")
    assert geom.ExportToWkt() == "POLYGON ((10 10,10 11,11 11,11 10,10 10))"


###############################################################################
# Test Python field accessors facilities


def test_ogr_rfc41_5():

    feature_defn = ogr.FeatureDefn()
    field_defn = ogr.FieldDefn("strfield", ogr.OFTString)
    feature_defn.AddFieldDefn(field_defn)
    feature_defn.GetGeomFieldDefn(0).SetName("geomfield")

    f = ogr.Feature(feature_defn)

    assert f["strfield"] is None
    assert f.strfield is None

    assert f["geomfield"] is None
    assert f.geomfield is None

    with pytest.raises(KeyError):
        f["nonexistent_field"]

    with pytest.raises(AttributeError):
        f.nonexistent_field

    with pytest.raises(KeyError):
        f["nonexistent_field"] = "foo"

    # This works.  Default Python behaviour. Stored in a dictionary
    f.nonexistent_field = "bar"
    assert f.nonexistent_field == "bar"

    f["strfield"] = "foo"
    assert f["strfield"] == "foo"
    assert f.strfield == "foo"

    f.strfield = "bar"
    assert f["strfield"] == "bar"
    assert f.strfield == "bar"

    wkt = "POINT EMPTY"
    f["geomfield"] = ogr.CreateGeometryFromWkt(wkt)
    assert f["geomfield"].ExportToWkt() == wkt
    assert f.geomfield.ExportToWkt() == wkt

    wkt2 = "POLYGON EMPTY"
    f.geomfield = ogr.CreateGeometryFromWkt(wkt2)
    assert f["geomfield"].ExportToWkt() == wkt2
    assert f.geomfield.ExportToWkt() == wkt2


###############################################################################
# Test OGRSQL with geometries


def test_ogr_rfc41_6():

    ds = ogr.GetDriverByName("memory").CreateDataSource("")
    sr = osr.SpatialReference()
    lyr = ds.CreateLayer("poly", geom_type=ogr.wkbNone)
    geomfield = ogr.GeomFieldDefn("geomfield", ogr.wkbPolygon)
    geomfield.SetSpatialRef(sr)
    lyr.CreateGeomField(geomfield)
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("wkt", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("intfield", 1)
    feat.SetField("wkt", "POINT (0 0)")
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    # Test implicit geometry column (since poly has one single geometry column)
    # then explicit geometry column
    for sql in [
        "SELECT intfield FROM poly",
        "SELECT * FROM poly",
        "SELECT intfield, geomfield FROM poly",
        "SELECT geomfield, intfield FROM poly",
    ]:
        sql_lyr = ds.ExecuteSQL(sql)
        assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon
        assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is not None
        feat = sql_lyr.GetNextFeature()
        assert feat.GetField("intfield") == 1
        assert feat.GetGeomFieldRef("geomfield") is not None
        feat = sql_lyr.GetNextFeature()
        assert feat.GetGeomFieldRef("geomfield") is None
        feat = None
        ds.ReleaseResultSet(sql_lyr)

    # Test CAST(geometry_field AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST(geomfield AS GEOMETRY) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY) IS NOT NULL"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is None
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeomFieldRef("mygeom") is not None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(xxx AS GEOMETRY(POLYGON))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST(geomfield AS GEOMETRY(POLYGON)) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY(POLYGON)) IS NOT NULL"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is None
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeomFieldRef("mygeom") is not None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(xxx AS GEOMETRY(POLYGON,4326))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST(geomfield AS GEOMETRY(POLYGON,4326)) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY(POLYGON,4326)) IS NOT NULL"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon
    assert (
        sql_lyr.GetLayerDefn()
        .GetGeomFieldDefn(0)
        .GetSpatialRef()
        .ExportToWkt()
        .find("4326")
        >= 0
    )
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeomFieldRef("mygeom") is not None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_multipolygon AS GEOMETRY(POLYGON))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST('MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))' AS GEOMETRY(POLYGON)) AS mygeom FROM poly"
    )
    feat = sql_lyr.GetNextFeature()
    assert (
        feat.GetGeomFieldRef("mygeom").ExportToWkt()
        == "POLYGON ((0 0,0 1,1 1,1 0,0 0))"
    )
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_polygon AS GEOMETRY(MULTIPOLYGON))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST('POLYGON ((0 0,0 1,1 1,1 0,0 0))' AS GEOMETRY(MULTIPOLYGON)) AS mygeom FROM poly"
    )
    feat = sql_lyr.GetNextFeature()
    assert (
        feat.GetGeomFieldRef("mygeom").ExportToWkt()
        == "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))"
    )
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_multilinestring AS GEOMETRY(LINESTRING))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST('MULTILINESTRING ((0 0,0 1,1 1,1 0,0 0))' AS GEOMETRY(LINESTRING)) AS mygeom FROM poly"
    )
    feat = sql_lyr.GetNextFeature()
    assert (
        feat.GetGeomFieldRef("mygeom").ExportToWkt()
        == "LINESTRING (0 0,0 1,1 1,1 0,0 0)"
    )
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_linestring AS GEOMETRY(MULTILINESTRING))
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST('LINESTRING (0 0,0 1,1 1,1 0,0 0)' AS GEOMETRY(MULTILINESTRING)) AS mygeom FROM poly"
    )
    feat = sql_lyr.GetNextFeature()
    assert (
        feat.GetGeomFieldRef("mygeom").ExportToWkt()
        == "MULTILINESTRING ((0 0,0 1,1 1,1 0,0 0))"
    )
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test expression with cast CHARACTER <--> GEOMETRY
    sql_lyr = ds.ExecuteSQL(
        "SELECT CAST(CAST(geomfield AS CHARACTER) AS GEOMETRY) AS mygeom, intfield FROM poly"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is None
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField("intfield") == 1
    assert feat.GetGeomFieldRef("mygeom") is not None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(NULL AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL("SELECT CAST(NULL AS GEOMETRY) FROM poly")
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeomFieldRef("") is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(stringfield AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL("SELECT CAST(wkt AS GEOMETRY) FROM poly")
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeomFieldRef("wkt").ExportToWkt() == "POINT (0 0)"
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test COUNT(geometry)
    sql_lyr = ds.ExecuteSQL("SELECT COUNT(geomfield) FROM poly")
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.GetField(0) == 1
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    wrong_sql_list = [
        (
            "SELECT DISTINCT geomfield FROM poly",
            "SELECT DISTINCT on a geometry not supported",
        ),
        (
            "SELECT COUNT(DISTINCT geomfield) FROM poly",
            "SELECT COUNT DISTINCT on a geometry not supported",
        ),
        (
            "SELECT MAX(geomfield) FROM poly",
            "Use of field function MAX() on geometry field",
        ),
        ("SELECT CAST(5 AS GEOMETRY) FROM poly", "Cannot cast integer to geometry"),
        (
            "SELECT CAST(geomfield AS integer) FROM poly",
            "Cannot cast geometry to integer",
        ),
        (
            "SELECT CAST(geomfield AS GEOMETRY(2)) FROM poly",
            "First argument of CAST operator should be a geometry type identifier",
        ),
        (
            "SELECT CAST(geomfield AS GEOMETRY(UNSUPPORTED_TYPE)) FROM poly",
            "SQL Expression Parsing Error: syntax error",
        ),
        (
            "SELECT CAST(geomfield AS GEOMETRY(UNSUPPORTED_TYPE,5)) FROM poly",
            "SQL Expression Parsing Error: syntax error",
        ),
    ]

    for sql, error_msg in wrong_sql_list:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            sql_lyr = ds.ExecuteSQL(sql)
        assert (
            gdal.GetLastErrorMsg().find(error_msg) == 0
        ), "For %s, expected error %s, got %s" % (
            sql,
            error_msg,
            gdal.GetLastErrorMsg(),
        )
        assert sql_lyr is None

    # Test invalid expressions with geometry
    for sql in [
        "SELECT geomfield + 'a' FROM poly",
        "SELECT geomfield * 'a' FROM poly",
        "SELECT geomfield + 'a' FROM poly",
        "SELECT geomfield - 'a' FROM poly",
        "SELECT geomfield % 'a' FROM poly",
        "SELECT CONCAT(geomfield, 'a') FROM poly",
        "SELECT SUBSTR(geomfield, 0, 1) FROM poly",
        "SELECT * FROM poly WHERE geomfield = CAST('POINT EMPTY' AS GEOMETRY)",
        "SELECT * FROM poly WHERE geomfield LIKE 'a'",
        "SELECT * FROM poly WHERE geomfield IN( 'a' )",
    ]:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            sql_lyr = ds.ExecuteSQL(sql)
        assert (
            gdal.GetLastErrorMsg().find("Cannot use geometry field in this operation")
            == 0
        )
        assert sql_lyr is None

    # Test expression with geometry in WHERE
    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly WHERE geomfield IS NOT NULL")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField("intfield") == 1
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly WHERE geomfield IS NULL")
    feat = sql_lyr.GetNextFeature()
    assert not feat.IsFieldSet(0)
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM poly WHERE CAST(geomfield AS CHARACTER) = 'POLYGON EMPTY'"
    )
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT count(*) FROM poly WHERE geomfield IS NULL")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField(0) == 1
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT count(*) FROM poly WHERE geomfield IS NOT NULL")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField(0) == 1
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test spatial filter
    feat = lyr.GetFeature(0)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.SetFeature(feat)
    feat = None

    lyr.DeleteFeature(1)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly")
    sql_lyr.SetSpatialFilterRect(0, 0, 0, 0)
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None

    sql_lyr.SetSpatialFilterRect(0, 1, 2, 1, 2)
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    feat = None

    # Test invalid spatial filter index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        sql_lyr.SetSpatialFilterRect(2, 0, 0, 0, 0)
    assert gdal.GetLastErrorMsg() != ""

    # Test invalid geometry field index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        sql_lyr.GetExtent(geom_field=2)
    assert gdal.GetLastErrorMsg() != ""

    ds.ReleaseResultSet(sql_lyr)

    # Test querying several geometry fields
    sql_lyr = ds.ExecuteSQL("SELECT geomfield as geom1, geomfield as geom2 FROM poly")
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.GetGeomFieldRef("geom1") is not None
    assert feat.GetGeomFieldRef("geom2") is not None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test querying a layer with several geometry fields
    lyr.CreateGeomField(ogr.GeomFieldDefn("secondarygeom", ogr.wkbPoint))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetGeomField("secondarygeom", ogr.CreateGeometryFromWkt("POINT (10 100)"))
    lyr.SetFeature(feat)
    feat = None

    for sql in [
        "SELECT * FROM poly",
        "SELECT geomfield, secondarygeom FROM poly",
        "SELECT secondarygeom, geomfield FROM poly",
    ]:
        sql_lyr = ds.ExecuteSQL(sql)
        feat = sql_lyr.GetNextFeature()
        assert feat.GetGeomFieldRef("geomfield").ExportToWkt() == "POINT (1 2)"
        assert feat.GetGeomFieldRef("secondarygeom").ExportToWkt() == "POINT (10 100)"
        feat = None
        ds.ReleaseResultSet(sql_lyr)

    # Check that we don't get an implicit geometry field
    sql_lyr = ds.ExecuteSQL("SELECT intfield FROM poly")
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    # Check GetExtent() and SetSpatialFilter()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly")
    assert sql_lyr.GetExtent(geom_field=0) == (1.0, 1.0, 2.0, 2.0)
    assert sql_lyr.GetExtent(geom_field=1) == (10.0, 10.0, 100.0, 100.0)
    sql_lyr.SetSpatialFilterRect(0, 0.5, 1.5, 1.5, 2.5)
    assert sql_lyr.GetFeatureCount() == 1
    sql_lyr.SetSpatialFilterRect(0, 0, 0, 0.5, 0.5)
    assert sql_lyr.GetFeatureCount() == 0
    sql_lyr.SetSpatialFilterRect(1, 9, 99, 11, 101)
    assert sql_lyr.GetFeatureCount() == 1
    sql_lyr.SetSpatialFilterRect(1, 0, 0, 0.5, 0.5)
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test crazy OGRSQL


def test_ogr_rfc41_7():

    ds = ogr.Open("data")
    sql = (
        'select eas_id, "_ogr_geometry_" as geom1, cast(null as geometry) as geom2, '
        + "'a', cast('POINT(3 4)' as geometry) as geom3, fid, \"_ogr_geometry_\" as geom4, "
        + "'c', p.eas_id, cast(area as integer) as area_int, \"_ogr_geometry_\", area from "
        + 'poly join "data".poly p on poly.eas_id = p.eas_id'
    )
    sql_lyr = ds.ExecuteSQL(sql)
    feat = sql_lyr.GetNextFeature()
    if (
        feat.eas_id != 168
        or feat.FIELD_2 != "a"
        or feat.fid != 0
        or feat.FIELD_4 != "c"
        or feat["p.eas_id"] != 168
        or feat.area_int != 215229
        or feat.area != pytest.approx(215229.266, abs=1e-5)
        or feat.geom1.GetGeometryType() != ogr.wkbPolygon
        or feat.geom2 is not None
        or feat.geom3.GetGeometryType() != ogr.wkbPoint
        or feat.geom4.GetGeometryType() != ogr.wkbPolygon
        or feat["_ogr_geometry_"].GetGeometryType() != ogr.wkbPolygon
    ):
        feat.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None


###############################################################################
# Test SQLite dialect


def test_ogr_rfc41_8(require_ogr_sql_sqlite):  # noqa
    ds = ogr.GetDriverByName("memory").CreateDataSource("")
    lyr = ds.CreateLayer("mytable", geom_type=ogr.wkbNone)
    gfld_defn = ogr.GeomFieldDefn("geomfield", ogr.wkbPolygon)
    lyr.CreateGeomField(gfld_defn)
    gfld_defn = ogr.GeomFieldDefn("geomfield2", ogr.wkbPoint25D)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(sr)
    lyr.CreateGeomField(gfld_defn)

    # Check that we get the geometry columns, even with no features
    sql_lyr = ds.ExecuteSQL("SELECT * FROM mytable", dialect="SQLite")
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 2
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is None
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbPoint25D
    srs = sql_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    ds.ReleaseResultSet(sql_lyr)

    # Test INSERT INTO request
    ds.ExecuteSQL(
        "INSERT INTO mytable (geomfield, geomfield2) VALUES ("
        + "GeomFromText('POLYGON ((0 0,0 1,1 1,1 0,0 0))'), "
        + "GeomFromText('POINT Z(0 1 2)') )",
        dialect="SQLite",
    )

    # Check output
    sql_lyr = ds.ExecuteSQL(
        "SELECT geomfield2, geomfield FROM mytable", dialect="SQLite"
    )
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef("geomfield")
    if geom.ExportToWkt() != "POLYGON ((0 0,0 1,1 1,1 0,0 0))":
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeomFieldRef("geomfield2")
    if geom.ExportToWkt() != "POINT (0 1 2)":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test UPDATE
    ds.ExecuteSQL(
        "UPDATE mytable SET geomfield2 = " + "GeomFromText('POINT Z(3 4 5)')",
        dialect="SQLite",
    )

    # Check output
    sql_lyr = ds.ExecuteSQL(
        "SELECT geomfield2, geomfield FROM mytable", dialect="SQLite"
    )
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef("geomfield")
    if geom.ExportToWkt() != "POLYGON ((0 0,0 1,1 1,1 0,0 0))":
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeomFieldRef("geomfield2")
    if geom.ExportToWkt() != "POINT (3 4 5)":
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)
