#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Shapefile driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

import os
import shutil
import struct
import sys
import time

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Create table from data/poly.shp


@pytest.fixture()
def shape_ds(poly_feat, tmp_path):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_path)

    #######################################################
    # Create shape Layer
    shape_lyr = ds.CreateLayer("tpoly")

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        shape_lyr,
        [("AREA", ogr.OFTReal), ("EAS_ID", ogr.OFTInteger), ("PRFEDEA", ogr.OFTString)],
    )

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())

    for feat in poly_feat:

        dst_feat.SetFrom(feat)
        shape_lyr.CreateFeature(dst_feat)

    shape_lyr = None

    return ds


###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_shape_3(shape_ds, poly_feat):

    shape_lyr = shape_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(shape_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(shape_lyr, "eas_id", expect)

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = shape_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.000000001
        )

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )


###############################################################################
# Write a feature without a geometry, and verify that it works OK.


def test_ogr_shape_4(shape_ds):

    shape_lyr = shape_ds.GetLayer(0)

    ######################################################################
    # Create feature without geometry.

    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetField("PRFEDEA", "nulled")
    shape_lyr.CreateFeature(dst_feat)

    ######################################################################
    # Read back the feature and get the geometry.

    with ogrtest.attribute_filter(shape_lyr, "PRFEDEA = 'nulled'"):
        feat_read = shape_lyr.GetNextFeature()
    assert feat_read is not None, "Didn't get feature with null geometry back."

    assert feat_read.GetGeometryRef() is None, "Didn't get null geometry as expected."


###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_shape_5(shape_ds):

    shape_lyr = shape_ds.GetLayer(0)

    # add two empty features to test DISTINCT
    for _ in range(2):
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        shape_lyr.CreateFeature(dst_feat)

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    with shape_ds.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_shape_6(shape_ds):

    with shape_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '35043413'"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "prfedea", ["35043413"])

        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            feat_read,
            "POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))",
            max_error=0.001,
        )


###############################################################################
# Test spatial filtering.


def test_ogr_shape_7(shape_ds):

    shape_lyr = shape_ds.GetLayer(0)

    with ogrtest.spatial_filter(shape_lyr, "LINESTRING(479505 4763195,480526 4762819)"):
        ogrtest.check_features_against_list(shape_lyr, "eas_id", [158])


###############################################################################
# Create spatial index, and verify we get the same results.


def test_ogr_shape_8(shape_ds):

    shape_dir = shape_ds.GetDescription()
    shape_lyr = shape_ds.GetLayer(0)

    index_fname = shape_dir + "/tpoly.qix"

    shape_ds.ExecuteSQL("CREATE SPATIAL INDEX ON tpoly")

    assert os.access(index_fname, os.F_OK), "tpoly.qix not created"

    with ogrtest.spatial_filter(shape_lyr, "LINESTRING(479505 4763195,480526 4762819)"):
        ogrtest.check_features_against_list(shape_lyr, "eas_id", [158])

    # Test recreating while already existing
    shape_ds.ExecuteSQL("CREATE SPATIAL INDEX ON tpoly")

    shape_ds.ExecuteSQL("DROP SPATIAL INDEX ON tpoly")

    assert not os.access(index_fname, os.F_OK), "tpoly.qix not deleted"


###############################################################################
# Test that we don't return a polygon if we are "inside" but non-overlapping.


def test_ogr_shape_9():

    shape_ds = ogr.Open("data/shp/testpoly.shp")
    shape_lyr = shape_ds.GetLayer(0)

    with ogrtest.spatial_filter(shape_lyr, -10, -130, 10, -110):

        if ogrtest.have_geos():
            assert shape_lyr.GetFeatureCount() == 0
        else:
            assert shape_lyr.GetFeatureCount() == 1


###############################################################################
# Do a fair size query that should pull in a few shapes.


def test_ogr_shape_10():

    shape_ds = ogr.Open("data/shp/testpoly.shp")
    shape_lyr = shape_ds.GetLayer(0)

    with ogrtest.spatial_filter(shape_lyr, -400, 22, -120, 400):
        ogrtest.check_features_against_list(shape_lyr, "FID", [0, 4, 8])


###############################################################################
# Do a mixed indexed attribute and spatial query.


def test_ogr_shape_11():

    shape_ds = ogr.Open("data/shp/testpoly.shp")
    shape_lyr = shape_ds.GetLayer(0)

    with ogrtest.attribute_filter(shape_lyr, "FID = 5"), ogrtest.spatial_filter(
        shape_lyr, -400, 22, -120, 400
    ):
        ogrtest.check_features_against_list(shape_lyr, "FID", [])

    with ogrtest.attribute_filter(shape_lyr, "FID = 4"), ogrtest.spatial_filter(
        shape_lyr, -400, 22, -120, 400
    ):
        ogrtest.check_features_against_list(shape_lyr, "FID", [4])


###############################################################################
# Check that multipolygon of asm.shp is properly returned.


def test_ogr_shape_12():

    asm_ds = ogr.Open("data/shp/asm.shp")
    asm_lyr = asm_ds.GetLayer(0)

    feat = asm_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert geom.GetCoordinateDimension() == 2, "dimension wrong."

    assert geom.GetGeometryName() == "MULTIPOLYGON", "Geometry of wrong type."

    assert geom.GetGeometryCount() == 5, "Did not get the expected number of polygons."

    counts = [15, 11, 17, 20, 9]
    for i in range(5):
        poly = geom.GetGeometryRef(i)
        assert (
            poly.GetGeometryName() == "POLYGON"
        ), "Did not get right type for polygons"

        assert poly.GetGeometryCount() == 1, "polygon with more than one ring."

        pnt_count = poly.GetGeometryRef(0).GetPointCount()
        assert pnt_count == counts[i], "Polygon %d has %d points instead of %d." % (
            i,
            pnt_count,
            counts[i],
        )


###############################################################################
# Perform a SetFeature() on a couple features, resetting the size.


def test_ogr_shape_13(shape_ds):

    shp_path = os.path.join(shape_ds.GetDescription(), "tpoly.shp")
    shape_ds.Destroy()

    shape_ds = ogr.Open(shp_path, update=1)
    shape_lyr = shape_ds.GetLayer(0)

    ######################################################################
    # Update FID 9 (EAS_ID=170), making the polygon larger.

    feat = shape_lyr.GetFeature(9)
    feat.SetField("AREA", "6000.00")

    geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((0 0, 0 60, 100 60, 100 0, 200 -30, 0 0))"
    )
    feat.SetGeometry(geom)

    assert shape_lyr.SetFeature(feat) == 0, "SetFeature() failed."

    ######################################################################
    # Update FID 8 (EAS_ID=165), making the polygon smaller.

    feat = shape_lyr.GetFeature(8)
    feat.SetField("AREA", "7000.00")

    geom = ogr.CreateGeometryFromWkt("POLYGON ((0 0, 0 60, 100 60, 100 0, 0 0))")
    feat.SetGeometry(geom)

    assert shape_lyr.SetFeature(feat) == 0, "SetFeature() failed."

    ###############################################################################
    # Verify last changes.

    shape_ds.Destroy()

    shape_ds = ogr.Open(shp_path, update=1)
    shape_lyr = shape_ds.GetLayer(0)

    ######################################################################
    # Check FID 9.

    feat = shape_lyr.GetFeature(9)

    assert feat.GetField("AREA") == 6000.0, "AREA update failed, FID 9."

    ogrtest.check_feature_geometry(
        feat, "POLYGON ((0 0, 0 60, 100 60, 100 0, 200 -30, 0 0))"
    )

    ######################################################################
    # Update FID 8 (EAS_ID=165), making the polygon smaller.

    feat = shape_lyr.GetFeature(8)

    assert feat.GetField("AREA") == 7000.0, "AREA update failed, FID 8."

    ogrtest.check_feature_geometry(feat, "POLYGON ((0 0, 0 60, 100 60, 100 0, 0 0))")


###############################################################################
# Delete a feature, and verify reduced count.


def test_ogr_shape_15(shape_ds):

    shape_lyr = shape_ds.GetLayer(0)

    original_count = shape_lyr.GetFeatureCount()

    ######################################################################
    # Delete FID 9.

    assert shape_lyr.DeleteFeature(9) == 0, "DeleteFeature failed."

    ######################################################################
    # Count features, verifying that none are FID 9.

    count = 0
    feat = shape_lyr.GetNextFeature()
    while feat is not None:
        assert feat.GetFID() != 9, "Still an FID 9 in dataset."

        count = count + 1
        feat = shape_lyr.GetNextFeature()

    assert count == original_count - 1, "Did not get expected FID count."


###############################################################################
# Repack and verify a few things.


def test_ogr_shape_16(shape_ds):

    shape_ds.ExecuteSQL("REPACK tpoly")

    shape_lyr = shape_ds.GetLayer(0)

    ######################################################################
    # Count features.

    got_9 = False
    count = 0
    shape_lyr.ResetReading()
    feat = shape_lyr.GetNextFeature()
    while feat is not None:
        if feat.GetFID() == 9:
            got_9 = True

        count = count + 1
        feat = shape_lyr.GetNextFeature()

    assert count == 10, "Did not get expected FID count."

    assert got_9 != 0, "Did not get FID 9 as expected."

    feat = shape_lyr.GetFeature(9)


###############################################################################
# Test adding a field to the schema of a populated layer.


def test_ogr_shape_16_1(shape_ds):

    shape_lyr = shape_ds.GetLayer(0)

    ######################################################################
    # Add a new field.
    field_defn = ogr.FieldDefn("NEWFLD", ogr.OFTString)
    field_defn.SetWidth(12)

    result = shape_lyr.CreateField(field_defn)

    assert result == 0, "failed to create new field."

    ######################################################################
    # Check at least one feature.

    feat = shape_lyr.GetFeature(8)
    assert feat.EAS_ID == 165, "Got wrong EAS_ID"

    assert feat.IsFieldNull("NEWFLD"), "Expected NULL NEWFLD value!"


###############################################################################
# Simple test with point shapefile with no associated .dbf


def test_ogr_shape_17(tmp_path):

    shutil.copy("data/shp/can_caps.shp", tmp_path / "can_caps.shp")
    shutil.copy("data/shp/can_caps.shx", tmp_path / "can_caps.shx")

    shp_ds = ogr.Open(tmp_path / "can_caps.shp", update=1)
    shp_lyr = shp_ds.GetLayer(0)

    assert (
        shp_lyr.GetLayerDefn().GetFieldCount() == 0
    ), "Unexpectedly got attribute fields."

    count = 0
    while 1:
        feat = shp_lyr.GetNextFeature()
        if feat is None:
            break

        # Re-write feature to test that we can use SetFeature() without
        # a DBF
        shp_lyr.SetFeature(feat)

        count += 1

    assert count == 13, "Got wrong number of features."

    # Create new feature without a DBF
    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    shp_lyr.CreateFeature(feat)
    assert feat.GetFID() == 13, "Got wrong FID."


###############################################################################
# Test reading data/poly.PRJ file with mixed-case file name


def test_ogr_shape_18():

    shp_ds = ogr.Open("data/poly.shp")
    shp_lyr = shp_ds.GetLayer(0)

    srs_lyr = shp_lyr.GetSpatialRef()

    assert srs_lyr is not None, "Missing projection definition."

    assert srs_lyr.GetAuthorityCode(None) == "27700"


###############################################################################
# Test polygon formation logic - recognising what rings are inner/outer
# and deciding on polygon vs. multipolygon (#1217)


def test_ogr_shape_19():

    ds = ogr.Open("data/shp/Stacks.shp")
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = "MULTIPOLYGON (((3115478.809630727861077 13939288.008583962917328,3134266.47213465673849 13971973.394036004319787,3176989.101938112173229 13957303.575368551537395,3198607.7820796193555 13921787.172278933227062,3169010.779504936654121 13891675.439224690198898,3120368.749186545144767 13897852.204979406669736,3115478.809630727861077 13939288.008583962917328),(3130405.993537959177047 13935427.529987264424562,3135038.567853996530175 13902742.144535223022103,3167209.22282647760585 13902227.414055664092302,3184452.693891727831215 13922559.267998272553086,3172871.258101634215564 13947781.061496697366238,3144561.081725850701332 13957818.305848112329841,3130405.993537959177047 13935427.529987264424562)),((3143016.890287171583623 13932596.512349685654044,3152282.038919246289879 13947266.331017138436437,3166179.761867358349264 13940060.104303302243352,3172099.162382294889539 13928221.303273428231478,3169268.144744716584682 13916897.23272311501205,3158201.439434182830155 13911235.197447959333658,3144818.446965630631894 13911749.927927518263459,3139928.507409813348204 13916382.502243556082249,3143016.890287171583623 13932596.512349685654044),(3149193.65604188805446 13926677.11183474957943,3150737.84748056717217 13918698.789401574060321,3158458.804673962760717 13919728.250360693782568,3164892.935668459162116 13923331.36371761187911,3163863.474709339439869 13928736.033752989023924,3157171.978475063573569 13935427.529987264424562,3149193.65604188805446 13926677.11183474957943)))"

    ogrtest.check_feature_geometry(feat, wkt, max_error=0.00000001)


###############################################################################
# Test empty multipoint, multiline, multipolygon.
# From GDAL 1.6.0, the expected behaviour is to return a feature with a NULL geometry


@pytest.mark.parametrize(
    "fname",
    (
        "data/shp/emptymultipoint.shp",
        "data/shp/emptymultiline.shp",
        "data/shp/emptymultipoly.shp",
    ),
)
def test_ogr_shape_20(fname):

    ds = ogr.Open(fname)
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    assert feat is not None
    assert feat.GetGeometryRef() is None


###############################################################################
# Test robutness towards broken/unfriendly shapefiles


@pytest.mark.parametrize(
    "f",
    [
        "data/shp/buggypoint.shp",
        "data/shp/buggymultipoint.shp",
        "data/shp/buggymultiline.shp",
        "data/shp/buggymultipoly.shp",
        "data/shp/buggymultipoly2.shp",
    ],
)
def test_ogr_shape_21(f):

    ds = ogr.Open(f)
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    with gdal.quiet_errors():
        feat = lyr.GetNextFeature()

    assert feat.GetGeometryRef() is None

    # Test fix for #3665
    lyr.ResetReading()
    (minx, maxx, miny, maxy) = lyr.GetExtent()
    with ogrtest.spatial_filter(
        lyr, minx + 1e-9, miny + 1e-9, maxx - 1e-9, maxy - 1e-9
    ), gdaltest.error_handler():
        feat = lyr.GetNextFeature()

    assert feat is None or feat.GetGeometryRef() is None


###############################################################################
# Test writing and reading all handled data types


def test_ogr_shape_22(shape_ds):

    shape_path = shape_ds.GetDescription()
    shape_ds.Destroy()

    #######################################################
    # Create memory Layer
    shape_ds = ogr.GetDriverByName("ESRI Shapefile").Open(shape_path, update=1)
    shape_lyr = shape_ds.CreateLayer("datatypes")

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        shape_lyr,
        [
            ("REAL", ogr.OFTReal),
            ("INTEGER", ogr.OFTInteger),
            ("STRING", ogr.OFTString),
            ("DATE", ogr.OFTDate),
        ],
    )

    #######################################################
    # Create a feature
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetField("REAL", 1.2)
    dst_feat.SetField("INTEGER", 3)
    dst_feat.SetField("STRING", "aString")
    dst_feat.SetField("DATE", "2005/10/12")
    shape_lyr.CreateFeature(dst_feat)

    shape_ds = None

    #######################################################
    # Read back the feature
    shape_ds = ogr.GetDriverByName("ESRI Shapefile").Open(shape_path, update=1)
    shape_lyr = shape_ds.GetLayerByName("datatypes")
    feat_read = shape_lyr.GetNextFeature()
    assert (
        feat_read.GetField("REAL") == 1.2
        and feat_read.GetField("INTEGER") == 3
        and feat_read.GetField("STRING") == "aString"
        and feat_read.GetFieldAsString("DATE") == "2005/10/12"
    )


###############################################################################
# Function used internally by ogr_shape_23.


def ogr_shape_23_write_valid_and_invalid(
    dir_name, layer_name, wkt, invalid_wkt, wkbType, isEmpty
):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(dir_name)

    #######################################################
    # Create a layer
    if wkbType == ogr.wkbUnknown:
        shape_lyr = ds.CreateLayer(layer_name)
    else:
        shape_lyr = ds.CreateLayer(layer_name, geom_type=wkbType)

    #######################################################
    # Write a geometry
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    shape_lyr.CreateFeature(dst_feat)

    #######################################################
    # Write an invalid geometry for this layer type
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(invalid_wkt))
    with gdal.quiet_errors():
        shape_lyr.CreateFeature(dst_feat)

    #######################################################
    # Check feature

    ds = None
    ds = ogr.GetDriverByName("ESRI Shapefile").Open(dir_name, update=1)

    read_lyr = ds.GetLayerByName(layer_name)
    assert read_lyr.GetFeatureCount() == 1, layer_name
    feat_read = read_lyr.GetNextFeature()

    if isEmpty and feat_read.GetGeometryRef() is None:
        return

    ogrtest.check_feature_geometry(
        feat_read, ogr.CreateGeometryFromWkt(wkt), max_error=0.000000001
    )


def ogr_shape_23_write_geom(dir_name, layer_name, geom, expected_geom, wkbType):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(dir_name)

    #######################################################
    # Create a layer
    if wkbType == ogr.wkbUnknown:
        shape_lyr = ds.CreateLayer(layer_name)
    else:
        shape_lyr = ds.CreateLayer(layer_name, geom_type=wkbType)

    #######################################################
    # Write a geometry
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    shape_lyr.CreateFeature(dst_feat)

    #######################################################
    # Check feature

    shape_lyr = None
    ds = None
    ds = ogr.GetDriverByName("ESRI Shapefile").Open(dir_name, update=1)

    read_lyr = ds.GetLayerByName(layer_name)
    assert read_lyr.GetFeatureCount() == 1
    feat_read = read_lyr.GetNextFeature()

    if expected_geom is None:
        assert (
            feat_read.GetGeometryRef() is None
        ), feat_read.GetGeometryRef().ExportToWkt()
        return

    ogrtest.check_feature_geometry(feat_read, expected_geom, max_error=0.000000001)


###############################################################################
# Test writing and reading all handled geometry types


@pytest.mark.parametrize("use_wkb_type", (True, False))
def test_ogr_shape_23(tmp_path, use_wkb_type):

    test_geom_array = [
        ("points", "POINT(0 1)", "LINESTRING(0 1)", ogr.wkbPoint),
        ("points25D", "POINT(0 1 2)", "LINESTRING(0 1)", ogr.wkbPoint25D),
        ("multipoints", "MULTIPOINT(0 1,2 3)", "POINT (0 1)", ogr.wkbMultiPoint),
        (
            "multipoints25D",
            "MULTIPOINT(0 1 2,3 4 5)",
            "POINT (0 1)",
            ogr.wkbMultiPoint25D,
        ),
        (
            "linestrings",
            "LINESTRING(0 1,2 3,4 5,0 1)",
            "POINT (0 1)",
            ogr.wkbLineString,
        ),
        (
            "linestrings25D",
            "LINESTRING(0 1 2,3 4 5,6 7 8,0 1 2)",
            "POINT (0 1)",
            ogr.wkbLineString25D,
        ),
        (
            "multilinestrings",
            "MULTILINESTRING((0 1,2 3,4 5,0 1), (0 1,2 3,4 5,0 1))",
            "POINT (0 1)",
            ogr.wkbMultiLineString,
        ),
        (
            "multilinestrings25D",
            "MULTILINESTRING((0 1 2,3 4 5,6 7 8,0 1 2),(0 1 2,3 4 5,6 7 8,0 1 2))",
            "POINT (0 1)",
            ogr.wkbMultiLineString25D,
        ),
        (
            "polygons",
            "POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5))",
            "POINT (0 1)",
            ogr.wkbPolygon,
        ),
        (
            "polygons25D",
            "POLYGON((0 0 2,0 10 5,10 10 8,0 1 2))",
            "POINT (0 1)",
            ogr.wkbPolygon25D,
        ),
        (
            "multipolygons",
            "MULTIPOLYGON(((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))",
            "POINT (0 1)",
            ogr.wkbMultiPolygon,
        ),
        (
            "multipolygons25D",
            "MULTIPOLYGON(((0 0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))",
            "POINT (0 1)",
            ogr.wkbMultiPolygon25D,
        ),
    ]

    test_empty_geom_array = [
        ("emptypoints", "POINT EMPTY", "LINESTRING(0 1)", ogr.wkbPoint),
        ("emptymultipoints", "MULTIPOINT EMPTY", "POINT(0 1)", ogr.wkbMultiPoint),
        ("emptylinestrings", "LINESTRING EMPTY", "POINT(0 1)", ogr.wkbLineString),
        (
            "emptymultilinestrings",
            "MULTILINESTRING EMPTY",
            "POINT(0 1)",
            ogr.wkbMultiLineString,
        ),
        ("emptypolygons", "POLYGON EMPTY", "POINT(0 1)", ogr.wkbPolygon),
        ("emptymultipolygons", "MULTIPOLYGON EMPTY", "POINT(0 1)", ogr.wkbMultiPolygon),
    ]

    #######################################################
    # Write a feature in a new layer (geometry type unset at layer creation)

    for item in test_geom_array:
        ogr_shape_23_write_valid_and_invalid(
            tmp_path,
            item[0],
            item[1],
            item[2],
            item[3] if use_wkb_type else ogr.wkbUnknown,
            0,
        )
    for item in test_empty_geom_array:
        ogr_shape_23_write_valid_and_invalid(
            tmp_path,
            item[0],
            item[1],
            item[2],
            item[3] if use_wkb_type else ogr.wkbUnknown,
            1,
        )


def test_ogr_shape_23a(tmp_path):
    #######################################################
    # Test writing of a geometrycollection

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_path)

    layer_name = "geometrycollections"
    shape_lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbMultiPolygon)

    # This geometry collection is not compatible with a multipolygon layer
    geom = ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION(POINT (0 0))")
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    with gdal.quiet_errors():
        shape_lyr.CreateFeature(dst_feat)

    # This geometry will be dealt as a multipolygon
    wkt = "GEOMETRYCOLLECTION(POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),POLYGON((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))"
    geom = ogr.CreateGeometryFromWkt(wkt)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    shape_lyr.CreateFeature(dst_feat)

    ds = None
    ds = ogr.GetDriverByName("ESRI Shapefile").Open(tmp_path, update=1)

    read_lyr = ds.GetLayerByName(layer_name)
    feat_read = read_lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read,
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON (((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1.0,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1.0,100 1,100.25 0.5)))"
        ),
        max_error=0.000000001,
    )


def test_ogr_shape_23b(tmp_path):
    #######################################################
    # Test writing of a multipoint with an empty point inside
    layer_name = "strangemultipoints"
    wkt = "MULTIPOINT(0 1)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPoint))

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


def test_ogr_shape_23c(tmp_path):
    #######################################################
    # Test writing of a multilinestring with an empty linestring inside
    layer_name = "strangemultilinestrings"
    wkt = "MULTILINESTRING((0 1,2 3,4 5,0 1), (0 1,2 3,4 5,0 1))"
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLineString))

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


def test_ogr_shape_23d(tmp_path):
    #######################################################
    # Test writing of a polygon with an empty external ring
    layer_name = "polygonwithemptyexternalring"
    geom = ogr.CreateGeometryFromWkt("POLYGON EMPTY")
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLinearRing))
    ring = ogr.Geometry(type=ogr.wkbLinearRing)
    ring.AddPoint_2D(0, 0)
    ring.AddPoint_2D(10, 0)
    ring.AddPoint_2D(10, 10)
    ring.AddPoint_2D(0, 10)
    ring.AddPoint_2D(0, 0)
    geom.AddGeometry(ring)

    ogr_shape_23_write_geom(tmp_path, layer_name, geom, None, ogr.wkbUnknown)


def test_ogr_shape_23e(tmp_path):
    #######################################################
    # Test writing of a polygon with an empty external ring
    layer_name = "polygonwithemptyinternalring"
    wkt = "POLYGON((100 0,100 10,110 10,100 0))"
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLinearRing))

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


def test_ogr_shape_23f(tmp_path):

    #######################################################
    # Test writing of a multipolygon with an empty polygon and a polygon with an empty external ring
    layer_name = "strangemultipolygons"
    wkt = "MULTIPOLYGON(((0 0,0 10,10 10,0 0)), ((100 0,100 10,110 10,100 0)))"
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPolygon))
    poly = ogr.CreateGeometryFromWkt("POLYGON((-100 0,-110 10,-100 10,-100 0))")
    poly.AddGeometry(ogr.Geometry(type=ogr.wkbLinearRing))
    geom.AddGeometry(poly)

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


def test_ogr_shape_23g(tmp_path):

    #######################################################
    # Test writing of a multipolygon with 2 parts touching by an edge (which is illegal simple features) (github #1787)
    layer_name = "multipolygon_two_parts_touching_one_edge"
    wkt = "MULTIPOLYGON (((1 1,1 2,2 2,2 1,1 1)),((2 1,2 2,3 2,3 1,2 1)))"
    geom = ogr.CreateGeometryFromWkt(wkt)

    ogr_shape_23_write_geom(tmp_path, layer_name, geom, geom, ogr.wkbUnknown)


###############################################################################
# Test reading a polygon whose outer and the inner ring touches at one point (#2589)


def test_ogr_shape_24(tmp_path):

    layer_name = "touchingrings"
    wkt = "MULTIPOLYGON(((0 0,0 10,10 10,0 0), (0 0,1 1,0 1,0 0)), ((100 100,100 200,200 200,200 100,100 100)))"
    geom = ogr.CreateGeometryFromWkt(wkt)

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


###############################################################################
# Test reading a multipolygon with one part inside the bounding box of the other
# part, but not inside it, and sharing the same first point... (#2589)
# test with OGR_ORGANIZE_POLYGONS=DEFAULT to avoid relying only on the winding order


@pytest.mark.parametrize("organize_polygons", (None, "DEFAULT"))
def test_ogr_shape_25(tmp_path, organize_polygons):

    layer_name = "touchingrings2"
    wkt = "MULTIPOLYGON(((10 5, 5 5,5 0,0 0,0 10,10 10,10 5)),((10 5,10 0,5 0,5 4.9,10 5)), ((100 100,100 200,200 200,200 100,100 100)))"
    geom = ogr.CreateGeometryFromWkt(wkt)

    with gdaltest.config_option("OGR_ORGANIZE_POLYGONS", organize_polygons):
        ogr_shape_23_write_geom(
            tmp_path,
            layer_name,
            geom,
            ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
            ogr.wkbUnknown,
        )


###############################################################################
# Test a polygon made of one outer ring and two inner rings (special case
# in organizePolygons()


def test_ogr_shape_26(tmp_path):
    layer_name = "oneouterring"
    wkt = "POLYGON ((100 100,100 200,200 200,200 100,100 100),(110 110,120 110,120 120,110 120,110 110),(130 110,140 110,140 120,130 120,130 110))"
    geom = ogr.CreateGeometryFromWkt(wkt)

    ogr_shape_23_write_geom(
        tmp_path,
        layer_name,
        geom,
        ogr.CreateGeometryFromWkt(geom.ExportToWkt()),
        ogr.wkbUnknown,
    )


###############################################################################
# Test reading bad geometries where a multi-part multipolygon is
# written as a single-part multipolygon with its parts as inner
# rings, like done by QGIS <= 3.28.11 with GDAL >= 3.7


def test_ogr_shape_read_multipolygon_as_invalid_polygon():

    ds = ogr.Open("data/shp/multipolygon_as_invalid_polygon.shp")
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
        assert (
            "contains polygon(s) with rings with invalid winding order"
            in gdal.GetLastErrorMsg()
        )
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,11 1,10 1,10 0)))"
    )
    gdal.ErrorReset()
    f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == ""
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((0.5 -0.5,1.5 0.5,0.5 0.5,0.5 -0.5)))"
    )


def test_ogr_shape_read_multipolygon_as_invalid_polygon_no_warning():

    ds = ogr.Open("data/shp/cb_2022_us_county_20m_extract.shp")
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.GetNextFeature()
        assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test alternate date formatting (#2746)


def test_ogr_shape_27():
    ds = ogr.Open("data/shp/water_main_dist.dbf")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    assert feat.installe_1 == "1989/04/25", "got wrong date result!"

    feat = None
    lyr = None
    ds = None


###############################################################################
# Test reading a 3 GB .DBF (#3011)


def test_ogr_shape_28(tmp_path):

    # Determine if the filesystem supports sparse files (we don't want to create a real 3 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files(tmp_path):
        pytest.skip()

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "hugedbf.shp"
    )
    lyr = ds.CreateLayer("test")
    field_defn = ogr.FieldDefn()
    field_defn.SetName("test")
    field_defn.SetWidth(99)
    lyr.CreateField(field_defn)
    ds = None

    os.remove(tmp_path / "hugedbf.shp")
    os.remove(tmp_path / "hugedbf.shx")

    f = open(tmp_path / "hugedbf.dbf", "rb+")

    # Set record count to 24,000,000
    f.seek(4, 0)
    f.write("\x00".encode("latin1"))
    f.write("\x36".encode("latin1"))
    f.write("\x6e".encode("latin1"))
    f.write("\x01".encode("latin1"))

    # Set value for record 23,900,000 at
    # offset 2,390,000,066 = (23,900,000 * (99 + 1) + 65) + 1
    f.seek(2390000066, 0)
    f.write("value_over_2GB".encode("latin1"))

    # Extend to 3 GB file
    f.seek(3000000000, 0)
    f.write("0".encode("latin1"))

    f.close()

    ds = ogr.Open(tmp_path / "hugedbf.dbf", update=1)
    assert ds is not None, f"Cannot open {tmp_path / 'hugedbf.dbf'}"

    # Check that the hand-written value can be read back
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(23900000)
    assert feat.GetFieldAsString(0) == "value_over_2GB"

    # Update with a new value
    feat.SetField(0, "updated_value")
    lyr.SetFeature(feat)
    feat = None

    # Test creating a feature over 2 GB file limit -> should work
    gdal.ErrorReset()
    feat = ogr.Feature(lyr.GetLayerDefn())
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret == 0
    feat = None
    assert (
        gdal.GetLastErrorMsg().find("2GB file size limit reached") >= 0
    ), "did not find expected warning"

    ds = None

    # Re-open and check the new value
    with gdaltest.config_option("SHAPE_2GB_LIMIT", "TRUE"):
        ds = ogr.Open(tmp_path / "hugedbf.dbf", 1)
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(23900000)
    assert feat.GetFieldAsString(0) == "updated_value"
    feat = None

    # Test creating a feature over 2 GB file limit -> should fail
    gdal.ErrorReset()
    feat = ogr.Feature(lyr.GetLayerDefn())
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret != 0
    feat = None
    assert (
        gdal.GetLastErrorMsg().find("2GB file size limit reached") >= 0
    ), "did not find expected warning"

    ds = None


###############################################################################
# Test that REPACK doesn't change extension case (#3293)


def test_ogr_shape_29(tmp_path):

    os.mkdir(tmp_path / "UPPERCASE")
    shutil.copy("data/poly.shp", tmp_path / "UPPERCASE" / "UPPERCASE.SHP")
    shutil.copy("data/poly.shx", tmp_path / "UPPERCASE" / "UPPERCASE.SHX")
    shutil.copy("data/poly.dbf", tmp_path / "UPPERCASE" / "UPPERCASE.DBF")
    f = open(tmp_path / "UPPERCASE" / "UPPERCASE.CPG", "wb")
    f.write("UTF-8".encode("ascii"))
    f.close()

    ds = ogr.Open(tmp_path / "UPPERCASE", update=1)
    lyr = ds.GetLayer(0)

    assert lyr.GetMetadata_Dict("SHAPEFILE") == {
        "CPG_VALUE": "UTF-8",
        "ENCODING_FROM_CPG": "UTF-8",
        "SOURCE_ENCODING": "UTF-8",
    }

    lyr.DeleteFeature(0)
    ds.ExecuteSQL("REPACK UPPERCASE")
    ds = None

    lst = gdal.ReadDir(tmp_path / "UPPERCASE")

    assert len(lst) == 6

    for filename in lst:
        assert filename in [
            ".",
            "..",
            "UPPERCASE.SHP",
            "UPPERCASE.SHX",
            "UPPERCASE.DBF",
            "UPPERCASE.CPG",
        ], lst
        assert "packed" not in filename, lst


###############################################################################
# Test that REPACK doesn't change extension case (#3293)


def test_ogr_shape_30(tmp_path):

    os.mkdir(tmp_path / "lowercase")
    shutil.copy("data/poly.shp", tmp_path / "lowercase" / "lowercase.shp")
    shutil.copy("data/poly.shx", tmp_path / "lowercase" / "lowercase.shx")
    shutil.copy("data/poly.dbf", tmp_path / "lowercase" / "lowercase.dbf")

    ds = ogr.Open(tmp_path / "lowercase", update=1)
    lyr = ds.GetLayer(0)
    lyr.DeleteFeature(0)
    ds.ExecuteSQL("REPACK lowercase")
    ds = None

    lst = gdal.ReadDir(tmp_path / "lowercase")

    assert len(lst) == 5

    for filename in lst:
        assert filename in [
            ".",
            "..",
            "lowercase.shp",
            "lowercase.shx",
            "lowercase.dbf",
        ], lst


###############################################################################
# Test truncation of long and duplicate field names.
# FIXME: Empty field names are allowed now!


def test_ogr_shape_31(tmp_path):

    fields = [
        ("a", ogr.OFTReal),
        ("A", ogr.OFTInteger),
        ("A_1", ogr.OFTInteger),
        ("A_1", ogr.OFTInteger),
        ("a_1_2", ogr.OFTInteger),
        ("aaaaaAAAAAb", ogr.OFTInteger),
        ("aAaaaAAAAAc", ogr.OFTInteger),
        ("aaaaaAAAABa", ogr.OFTInteger),
        ("aaaaaAAAABb", ogr.OFTInteger),
        ("aaaaaAAA_1", ogr.OFTInteger),
        ("aaaaaAAAABc", ogr.OFTInteger),
        ("aaaaaAAAABd", ogr.OFTInteger),
        ("aaaaaAAAABe", ogr.OFTInteger),
        ("aaaaaAAAABf", ogr.OFTInteger),
        ("aaaaaAAAABg", ogr.OFTInteger),
        ("aaaaaAAAABh", ogr.OFTInteger),
        ("aaaaaAAAABi", ogr.OFTInteger),
        ("aaaaaAAA10", ogr.OFTString),
        ("", ogr.OFTInteger),
        ("", ogr.OFTInteger),
    ]

    expected_fields = [
        "a",
        "A_1",
        "A_1_1",
        "A_1_2",
        "a_1_2_1",
        "aaaaaAAAAA",
        "aAaaaAAA_1",
        "aaaaaAAAAB",
        "aaaaaAAA_2",
        "aaaaaAAA_3",
        "aaaaaAAA_4",
        "aaaaaAAA_5",
        "aaaaaAAA_6",
        "aaaaaAAA_7",
        "aaaaaAAA_8",
        "aaaaaAAA_9",
        "aaaaaAAA10",
        "aaaaaAAA11",
        "",
        "_1",
    ]

    shape_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_path)

    #######################################################
    # Create Layer
    shape_lyr = shape_ds.CreateLayer("Fields")

    #######################################################
    # Setup Schema with weird field names
    with gdal.quiet_errors():
        ogrtest.quick_create_layer_def(shape_lyr, fields)

    layer_defn = shape_lyr.GetLayerDefn()
    for i in range(layer_defn.GetFieldCount()):
        assert layer_defn.GetFieldDefn(i).GetNameRef() == expected_fields[i]


###############################################################################
# Test creating a nearly 4GB (2^32 Bytes) .shp (#3236)
# Check for proper error report.
# Assuming 2^32 is the max value for unsigned int.


def test_ogr_shape_32(tmp_path):
    # This test takes a few minutes and disk space. Hence, skipped by default.
    # To run this test, make sure that the directory BigFilePath points to has
    # 4.5 GB space available or give a new directory that does and delete the
    # directory afterwards.

    pytest.skip()  # pylint: disable=unreachable

    # pylint: disable=unreachable
    from decimal import Decimal

    BigFilePath = tmp_path

    #######################################################
    # Create a layer
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    shape_ds_big = shape_drv.CreateDataSource(BigFilePath)
    shape_lyr = shape_ds_big.CreateLayer("bigLayer", geom_type=ogr.wkbPolygon)

    #######################################################
    # Write a geometry repeatedly.
    # File size is pre-calculated according to the geometry's size.
    wkt = "POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1.1,0.5 1,0.25 0.5))"
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPolygon))

    ret = 0
    n = 0
    print("")
    for n in range(0, 22845571):
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(geom)
        ret = shape_lyr.CreateFeature(dst_feat)
        assert ret == 0 or n >= 22845570, "File limit reached before 4GB!"

        if (n % 22846) == 0:
            sys.stdout.write("\r%.1f%%   " % (n / Decimal("228460.0")))
            sys.stdout.flush()

    #######################################################
    # Check some features

    shape_ds_big = None
    shape_ds_big = ogr.GetDriverByName("ESRI Shapefile").Open(BigFilePath, update=0)

    read_lyr = shape_ds_big.GetLayerByName("bigLayer")

    for i in [0, 1, read_lyr.GetFeatureCount() - 1]:
        feat_read = read_lyr.GetFeature(i)
        assert feat_read is not None, ("Could not retrieve geometry at FID", i)
        ogrtest.check_feature_geometry(
            feat_read,
            "POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1.1,0.5 1,0.25 0.5))",
            max_error=0.000000001,
            context=f"FID {i}",
        )


###############################################################################
# Check that we can detect correct winding order even with polygons with big
# coordinate offset (#3356)


def test_ogr_shape_33():

    ds = ogr.Open("data/shp/bigoffset.shp")
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read,
        "MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )",
        max_error=0.000000001,
    )


###############################################################################
# Check that we can write correct winding order even with polygons with big
# coordinate offset (#33XX)


def test_ogr_shape_34(tmp_path):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "bigoffset.shp"
    )
    lyr = ds.CreateLayer("bigoffset")
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    geom_wkt = "MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )"
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open(tmp_path / "bigoffset.shp")
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read,
        "MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )",
        max_error=0.000000001,
    )


###############################################################################
# Check that we can read & write a VSI*L dataset


def test_ogr_shape_35(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "test35.shp"
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test35", srs=srs)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    geom_wkt = "POINT(0 1)"
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open(tmp_vsimem / "test35.shp")
    lyr = ds.GetLayer(0)
    srs_read = lyr.GetSpatialRef()
    assert srs_read.ExportToWkt() == srs.ExportToWkt(), "did not get expected SRS"
    feat_read = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read, ogr.CreateGeometryFromWkt("POINT(0 1)"), max_error=0.000000001
    )


###############################################################################
# Check that we can read from the root of a .ZIP file


def test_ogr_shape_36():

    ds = ogr.Open("/vsizip/data/shp/poly.zip")
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("OSGB") != -1, "did not get expected SRS"

    feat_read = lyr.GetFeature(9)
    ogrtest.check_feature_geometry(
        feat_read,
        ogr.CreateGeometryFromWkt(
            "POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))"
        ),
        max_error=0.000000001,
    )


###############################################################################
# Check that we can read from the root of a .tar.gz file


def test_ogr_shape_37():

    ds = ogr.Open("/vsitar/data/shp/poly.tar.gz")
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("OSGB") != -1, "did not get expected SRS"

    for i in range(10):
        feat_read = lyr.GetNextFeature()
        if i == 9:
            ogrtest.check_feature_geometry(
                feat_read,
                ogr.CreateGeometryFromWkt(
                    "POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))"
                ),
                max_error=0.000000001,
            )

    lyr.ResetReading()
    feat_read = lyr.GetFeature(9)
    ogrtest.check_feature_geometry(
        feat_read,
        ogr.CreateGeometryFromWkt(
            "POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))"
        ),
        max_error=0.000000001,
    )

    ds = None
    gdal.Unlink("data/shp/poly.tar.gz.properties")


###############################################################################
# Check that we can read from a .tar file


def test_ogr_shape_37_bis():

    ds = ogr.Open("/vsitar/data/shp/poly.tar")
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("OSGB") != -1, "did not get expected SRS"

    for i in range(10):
        feat_read = lyr.GetNextFeature()
        if i == 9:
            ogrtest.check_feature_geometry(
                feat_read,
                ogr.CreateGeometryFromWkt(
                    "POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))"
                ),
                max_error=0.000000001,
            )

    lyr.ResetReading()
    feat_read = lyr.GetFeature(9)
    ogrtest.check_feature_geometry(
        feat_read,
        ogr.CreateGeometryFromWkt(
            "POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))"
        ),
        max_error=0.000000001,
    )


###############################################################################
# Check that we cannot create duplicated layers


def test_ogr_shape_38(tmp_vsimem):

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem) as ds:
        ds.CreateLayer("test35")

    assert gdal.VSIStatL(tmp_vsimem / "test35.shp") is not None

    with ogr.Open(tmp_vsimem, update=1) as ds:
        with gdal.quiet_errors():
            lyr = ds.CreateLayer("test35")

        assert lyr is None, "should not have created a new layer"


###############################################################################
# Check that we can detect correct winding order even with polygons with big
# coordinate offset (#3356)


def test_ogr_shape_39():

    ds = ogr.Open("data/shp/multipatch.shp")
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read,
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (TIN (((5 4 10,0 0 5,10 0 5,5 4 10)),((5 4 10,10 0 5,10 8 5,5 4 10)),((5 4 10,10 8 5,0 8 5,5 4 10)),((5 4 10,0 8 5,0 0 5,5 4 10))),TIN (((10 0 5,10 0 0,10 8 5,10 0 5)),((10 0 0,10 8 5,10 8 0,10 0 0)),((10 8 5,10 8 0,0 8 5,10 8 5)),((10 8 0,0 8 5,0 8 0,10 8 0)),((0 8 5,0 8 0,0 0 5,0 8 5)),((0 8 0,0 0 5,0 0 0,0 8 0))),MULTIPOLYGON (((0 0 0,0 0 5,10 0 5,10 0 0,6 0 0,6 0 3,4 0 3,4 0 0,0 0 0),(1 0 2,3 0 2,3 0 4,1 0 4,1 0 2),(7 0 2,9 0 2,9 0 4,7 0 4,7 0 2))))"
        ),
        max_error=0.000000001,
    )


###############################################################################
# Make some changes to a shapefile and check the index files. qix, sbn & sbx


@pytest.mark.parametrize("operation", ("update", "add", "delete"))
def test_ogr_shape_40(tmp_path, operation):

    datafiles = ("gjpoint.dbf", "gjpoint.shp", "gjpoint.shx")
    indexfiles = ("gjpoint.sbn", "gjpoint.sbx", "gjpoint.qix")
    for f in datafiles:
        shutil.copy(os.path.join("data", "shp", f), tmp_path / f)
    for i in range(2):
        shutil.copy(
            os.path.join("data", "shp", indexfiles[i]),
            tmp_path / indexfiles[i],
        )

    shape_ds = ogr.Open(tmp_path / "gjpoint.shp", update=1)
    shape_lyr = shape_ds.GetLayer(0)
    # gdaltest.shape_lyr.SetAttributeFilter(None)
    shape_ds.ExecuteSQL("CREATE SPATIAL INDEX ON gjpoint")

    if operation == "update":
        # Ensure that updating a feature removes the indices
        feat = shape_lyr.GetFeature(0)
        geom = ogr.CreateGeometryFromWkt("POINT (99 1)")
        feat.SetGeometry(geom)

        for f in indexfiles:
            assert (tmp_path / f).exists()

        shape_lyr.SetFeature(feat)
    elif operation == "add":
        # Ensure that adding a feature removes the indices
        feat = ogr.Feature(shape_lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt("POINT (98 2)")
        feat.SetGeometry(geom)
        feat.SetField("NAME", "Point 2")
        feat.SetField("FID", "2")
        feat.SetFID(1)

        for f in indexfiles:
            assert (tmp_path / f).exists()

        shape_lyr.CreateFeature(feat)
    elif operation == "delete":
        # Ensure that deleting a feature removes the indices
        for f in indexfiles:
            assert (tmp_path / f).exists()
        assert shape_lyr.DeleteFeature(0) == 0, "DeleteFeature failed."


###############################################################################
# Run test_ogrsf


def test_ogr_shape_41(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    shutil.copy("data/poly.shp", tmp_path / "poly.shp")
    shutil.copy("data/poly.shx", tmp_path / "poly.shx")
    shutil.copy("data/poly.dbf", tmp_path / "poly.dbf")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -fsf {tmp_path}/poly.shp"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Run test_ogrsf with -sql


def test_ogr_shape_42(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    shutil.copy("data/poly.shp", tmp_path / "poly.shp")
    shutil.copy("data/poly.shx", tmp_path / "poly.shx")
    shutil.copy("data/poly.dbf", tmp_path / "poly.dbf")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f' {tmp_path}/poly.shp -sql "SELECT * FROM poly"'
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test /vsizip//vsicurl/


@pytest.mark.require_curl()
def test_ogr_shape_43():

    conn = gdaltest.gdalurlopen(
        "https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip"
    )
    if conn is None:
        pytest.skip("cannot open URL")
    conn.close()

    ds = ogr.Open(
        "/vsizip//vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip"
    )
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("OSGB") != -1, "did not get expected SRS"

    f = lyr.GetNextFeature()
    assert f is not None, "did not get expected feature"


###############################################################################
# Test /vsicurl/ on a directory


@pytest.mark.require_curl()
@pytest.mark.skip(reason="file should be hosted on a non github server")
def test_ogr_shape_44():

    conn = gdaltest.gdalurlopen(
        "https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip"
    )
    if conn is None:
        pytest.skip("cannot open URL")
    conn.close()

    ds = ogr.Open(
        "/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/testshp"
    )
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find("OSGB") != -1, "did not get expected SRS"

    f = lyr.GetNextFeature()
    assert f is not None, "did not get expected feature"


###############################################################################
# Test ignored fields works ok on a shapefile.


def test_ogr_shape_45():

    shp_ds = ogr.Open("data/poly.shp")
    shp_layer = shp_ds.GetLayer(0)
    shp_layer.SetIgnoredFields(["AREA"])

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

    feat = None
    shp_layer = None
    shp_ds = None


###############################################################################
# This is a very weird use case : the user creates/open a datasource
# made of a single shapefile 'foo.shp' and wants to add a new layer
# to it, 'bar'. So we create a new shapefile 'bar.shp' in the same
# directory as 'foo.shp'


def test_ogr_shape_46(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_46.shp"
    )
    ds.CreateLayer("you_can_put_here_what_you_want_i_dont_care")
    ds.CreateLayer("this_one_i_care_46")
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_46.shp")
    assert ds.GetLayerCount() == 1
    ds = None

    ds = ogr.Open(tmp_vsimem / "this_one_i_care_46.shp")
    assert ds.GetLayerCount() == 1
    ds = None


###############################################################################
# Test that we can open a symlink whose pointed filename isn't a real
# file, but a filename that OGR recognizes


def test_ogr_shape_47(tmp_path):

    if not gdaltest.support_symlink():
        pytest.skip()

    os.symlink("/vsizip/data/shp/poly.zip", tmp_path / "poly.zip")

    ds = ogr.Open(tmp_path / "poly.zip")
    assert ds is not None, f"{tmp_path / 'poly.zip'} symlink does not open."
    ds = None


###############################################################################
# Test RECOMPUTE EXTENT ON (#4027)


def test_ogr_shape_48(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_48")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(feat)

    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    lyr.SetFeature(feat)
    extent = lyr.GetExtent()
    assert extent == (1, 3, 2, 4), "did not get expected extent (1)"
    extent3D = lyr.GetExtent3D()
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert extent3D == (
        1,
        3,
        2,
        4,
        float("inf"),
        float("-inf"),
    ), "did not get expected extent 3D"

    ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_48")
    extent = lyr.GetExtent()
    assert extent == (3, 3, 4, 4), "did not get expected extent (2)"
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_48.shp")
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent()
    assert extent == (3, 3, 4, 4), "did not get expected extent (3)"
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )

    # Test with Polygon
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_48")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 -1,-1 -1,-1 0,0 0))"))
    lyr.CreateFeature(feat)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    lyr.SetFeature(feat)
    ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_48")
    extent = lyr.GetExtent()
    assert extent == (0, 1, 0, 1), "did not get expected extent (4)"
    ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )

    # Test with PolygonZ
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_48")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON((0 0 -2,0 -1 -2,-1 -1 -2,-1 0 -2,0 0 -2))")
    )
    lyr.CreateFeature(feat)
    feat.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON((0 0 2,0 1 1,1 1 2,1 0 2,0 0 3))")
    )
    lyr.SetFeature(feat)
    ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_48")
    extent = lyr.GetExtent()
    assert extent == (0, 1, 0, 1), "did not get expected extent (4)"
    extent3D = lyr.GetExtent3D()
    assert lyr.TestCapability(ogr.OLCFastGetExtent3D)
    assert extent3D == (0, 1, 0, 1, 1, 3), "did not get expected extent 3D"
    ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "ogr_shape_48.shp"
    )


###############################################################################
# Test that we can read at an LDID/87 file and recode to UTF-8.


def test_ogr_shape_49():

    ds = ogr.Open("data/shp/facility_surface_dd.dbf")
    lyr = ds.GetLayer(0)

    assert lyr.GetMetadata_Dict("SHAPEFILE") == {
        "ENCODING_FROM_LDID": "ISO-8859-1",
        "LDID_VALUE": "87",
        "SOURCE_ENCODING": "ISO-8859-1",
    }

    feat = lyr.GetFeature(91)

    name = feat.GetField("NAME")

    # Setup the utf-8 string.
    gdaltest.exp_name = "OSEBERG S\u00D8R"

    assert name == gdaltest.exp_name, "Did not get expected name, encoding problems?"


###############################################################################
# Test that we can read encoded field names


def test_ogr_shape_50():

    ds = ogr.Open("data/shp/chinese.dbf")
    if ds is None:
        pytest.skip()
    lyr = ds.GetLayer(0)

    reconv_possible = lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1

    if "Recode from CP936 to UTF-8" in gdal.GetLastErrorMsg():
        assert (
            not reconv_possible
        ), "Recode failed, but TestCapability(OLCStringsAsUTF8) returns TRUE"

        pytest.skip("skipping test: iconv support needed")

    assert lyr.GetMetadata_Dict("SHAPEFILE") == {
        "ENCODING_FROM_LDID": "CP936",
        "LDID_VALUE": "77",
        "SOURCE_ENCODING": "CP936",
    }

    # Setup the utf-8 string.
    gdaltest.fieldname = "\u4e2d\u56fd"

    assert lyr.GetLayerDefn().GetFieldIndex(gdaltest.fieldname) == 0, (
        lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef()
    )

    assert reconv_possible, "TestCapability(OLCStringsAsUTF8) should return TRUE"

    with ds.ExecuteSQL(f"SELECT * FROM {lyr.GetName()}") as sql_lyr:
        assert sql_lyr.TestCapability(ogr.OLCStringsAsUTF8)

    if ogr.GetDriverByName("SQLITE"):
        with ds.ExecuteSQL(
            f"SELECT * FROM {lyr.GetName()}", dialect="SQLITE"
        ) as sql_lyr:
            assert sql_lyr.TestCapability(ogr.OLCStringsAsUTF8)


###############################################################################
# Test that we can add a field when there's no dbf file initially


def test_ogr_shape_51(tmp_vsimem):

    if int(gdal.VersionInfo("VERSION_NUM")) < 1900:
        pytest.skip("would crash")

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_51.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_51")
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_51.dbf")

    ds = ogr.Open(tmp_vsimem / "ogr_shape_51.shp", update=1)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    feat = lyr.GetNextFeature()
    feat.SetField(0, "bar")
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_51.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    value = feat.GetFieldAsString(0)
    field_count = lyr.GetLayerDefn().GetFieldCount()
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "ogr_shape_51.shp"
    )

    assert field_count == 1, "did not get expected field count"

    assert value == "bar", "did not get expected value"


###############################################################################
# Test fix for #3356


def test_ogr_shape_52(tmp_vsimem):

    expected_geom = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON (((175.524709766699999 -40.17203475,175.524757883299998 -40.172050566700001,175.52480505 -40.1720663,175.524858766699992 -40.172091433299997,175.524913916700001 -40.172112966699999,175.524966049999989 -40.172136933300003,175.525030633299991 -40.17216185,175.5250873 -40.17218215,175.52515168330001 -40.1722011,175.525217666700001 -40.172221216700002,175.525269416700013 -40.172234466699997,175.5253165 -40.1722478,175.52535415 -40.1722577667,175.52538385 -40.17226365,175.525436816699994 -40.1722814333,175.525507016700004 -40.17229905,175.525594783299994 -40.172322033299999,175.525669933300009 -40.172339533299997,175.52574 -40.17235335,175.525807566699996 -40.1723672,175.52585005 -40.17237395,175.52588115 -40.172378683300003,175.525969816700012 -40.172388633300002,175.526057266700008 -40.1724020833,175.52723455 -40.17253515,175.527275583299996 -40.1725388,175.527324533300003 -40.17254675,175.527394866700007 -40.172552766700001,175.527473066699997 -40.172561616700001,175.527576666700014 -40.172572916699998,175.527678333300003 -40.172584266699999,175.527787883299993 -40.17259845,175.52789345 -40.172609716700002,175.527953933300012 -40.17261295,175.528028083300001 -40.1726174,175.52809835 -40.1726219333,175.528151650000012 -40.172625833300003,175.528190349999988 -40.17262725,175.528230900000011 -40.172631183299998,175.5282776 -40.1726338,175.528322800000012 -40.172637633299999,175.5283648 -40.17263915,175.5284115 -40.172641766700004,175.528452133299993 -40.17264435,175.528492133300006 -40.172646033299998,175.52856465 -40.17264805,175.528621733300014 -40.1726492,175.52868035 -40.172650333299998,175.528751333299994 -40.172652383299997,175.528814566699992 -40.1726534,175.528883933299994 -40.172653116699998,175.528939383300013 -40.17265195,175.529002566700001 -40.1726518,175.529070350000012 -40.172650366699997,175.529136633299998 -40.17265015,175.529193616700013 -40.17264895,175.529250616700011 -40.172647733300003,175.529313800000011 -40.172647583299998,175.529376783299995 -40.172647016699997,175.52895773329999 -40.172694633299997,175.528450866700013 -40.172752216699998,175.52835635 -40.172753466700001,175.52741181670001 -40.1727757333,175.52685245 -40.172532333299998,175.52627245 -40.172501266700003,175.5262405167 -40.172502816700003,175.5258356 -40.172522816700003,175.5256125 -40.172533833300001,175.525424433300003 -40.172543116699998,175.524834133300004 -40.1725533,175.524739033299994 -40.172414983300001,175.5247128 -40.17207405,175.524709766699999 -40.17203475)),((175.531267916699989 -40.17286525,175.5312654 -40.172863283300003,175.531252849999987 -40.172853516700002,175.531054566699993 -40.172822366699997,175.530193283300008 -40.172687333299997,175.529890266699994 -40.1726398,175.529916116700008 -40.172639383300002,175.529972483300014 -40.172639216699999,175.53002885 -40.1726398,175.530085183300002 -40.17264115,175.530141500000013 -40.17264325,175.530197733300014 -40.172646133299999,175.530253916699991 -40.172649766699998,175.530309983299986 -40.172654166699999,175.53036595 -40.172659333299997,175.5304218 -40.17266525,175.53047748329999 -40.172671916699997,175.530533016699991 -40.17267935,175.5305883833 -40.1726875333,175.530643533300008 -40.172696466700003,175.530722333299991 -40.172710633299999,175.530800633300004 -40.1727263167,175.5308541 -40.17273795,175.5309073 -40.1727503,175.530960216700009 -40.172763366700003,175.531012816700013 -40.172777133300002,175.5310651 -40.1727916,175.53111705 -40.172806766699999,175.531168650000012 -40.172822633300001,175.531219883299997 -40.172839183299999,175.531270733300005 -40.1728564,175.531267916699989 -40.17286525)))"
    )

    ds = ogr.Open("data/shp/test3356.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, expected_geom, max_error=0.000000001)

    ds = None

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_52.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_52")
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(expected_geom)
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_52.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, expected_geom, max_error=0.000000001)

    ds = None


###############################################################################
# Test various expected error cases


def test_ogr_shape_53(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_53.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_53")

    # Test ReorderFields() when there are no fields
    ret = lyr.ReorderFields([])
    assert ret == 0

    # Test REPACK when there are no features
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = ds.ExecuteSQL("REPACK ogr_shape_53")
    # Should work without any error
    assert gdal.GetLastErrorMsg() == ""

    # Create a field
    fd = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(fd)

    # GetFeature() on a invalid FID
    gdal.ErrorReset()
    with gdal.quiet_errors():
        feat = lyr.GetFeature(-1)
    assert feat is None and gdal.GetLastErrorMsg() != ""

    # SetFeature() on a invalid FID
    gdal.ErrorReset()
    with gdal.quiet_errors():
        feat = ogr.Feature(lyr.GetLayerDefn())
        ret = lyr.SetFeature(feat)
        feat = None
    assert ret != 0

    # SetFeature() on a invalid FID
    gdal.ErrorReset()
    with gdal.quiet_errors():
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetFID(1000)
        ret = lyr.SetFeature(feat)
        feat = None
    assert ret != 0

    # DeleteFeature() on a invalid FID
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(-1)
    assert ret != 0

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    ret = lyr.DeleteFeature(0)
    assert ret == 0

    # Try deleting an already deleted feature
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(0)
    assert ret != 0

    # Test DeleteField() on a invalid index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteField(-1)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test ReorderFields() with invalid permutation
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.ReorderFields([1])
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test AlterFieldDefn() on a invalid index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        fd = ogr.FieldDefn("foo2", ogr.OFTString)
        ret = lyr.AlterFieldDefn(-1, fd, 0)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test AlterFieldDefn() when attempting to convert from OFTString to something else
    gdal.ErrorReset()
    with gdal.quiet_errors():
        fd = ogr.FieldDefn("foo", ogr.OFTInteger)
        ret = lyr.AlterFieldDefn(0, fd, ogr.ALTER_TYPE_FLAG)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test DROP SPATIAL INDEX ON layer without index
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = ds.ExecuteSQL("DROP SPATIAL INDEX ON ogr_shape_53")
    assert gdal.GetLastErrorMsg() != ""

    # Re-create a feature
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    lyr = None
    ds = None

    # Test that some operations are not possible in read-only mode
    ds = ogr.Open(tmp_vsimem / "ogr_shape_53.shp")
    lyr = ds.GetLayer(0)

    assert lyr.TestCapability(ogr.OLCSequentialWrite) == 0
    assert lyr.TestCapability(ogr.OLCDeleteFeature) == 0
    assert lyr.TestCapability(ogr.OLCCreateField) == 0
    assert lyr.TestCapability(ogr.OLCDeleteField) == 0
    assert lyr.TestCapability(ogr.OLCReorderFields) == 0
    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 0

    # Test CreateField()
    fd = ogr.FieldDefn("bar", ogr.OFTString)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.CreateField(fd)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test ReorderFields()
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.ReorderFields([0])
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test DeleteField()
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteField(0)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test AlterFieldDefn()
    gdal.ErrorReset()
    with gdal.quiet_errors():
        fd = ogr.FieldDefn("foo2", ogr.OFTString)
        ret = lyr.AlterFieldDefn(0, fd, 0)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test CreateFeature()
    feat = ogr.Feature(lyr.GetLayerDefn())

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test DeleteFeature()
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(0)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test SetFeature()
    feat = lyr.GetNextFeature()

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.SetFeature(feat)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test REPACK
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = ds.ExecuteSQL("REPACK ogr_shape_53")
    assert gdal.GetLastErrorMsg() != ""

    # Test RECOMPUTE EXTENT ON
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_53")
    assert gdal.GetLastErrorMsg() != ""

    feat = None
    lyr = None
    ds = None

    # Attempt to delete shape in shapefile with no .dbf file
    gdal.Unlink(tmp_vsimem / "ogr_shape_53.dbf")
    ds = ogr.Open(tmp_vsimem / "ogr_shape_53.shp", update=1)
    lyr = ds.GetLayer(0)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.DeleteFeature(0)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == "")

    # Test REPACK
    ds.ExecuteSQL("REPACK ogr_shape_53")

    lyr = None
    ds = None

    # Tests on a DBF only
    ds = ogr.Open("data/idlink.dbf")
    lyr = ds.GetLayer(0)

    # Test GetExtent()
    # FIXME : GetExtent() should fail. Currently we'll get garbage here
    lyr.GetExtent()

    # Test RECOMPUTE EXTENT ON
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_53")
    assert gdal.GetLastErrorMsg() != ""

    lyr = None
    ds = None


###############################################################################
# Test accessing a shape datasource with hundreds of layers (#4306)


def ogr_shape_54_create_layer(ds, layer_index):
    lyr = ds.CreateLayer("layer%03d" % layer_index)
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "val%d" % layer_index)
    if (layer_index % 2) == 0:
        feat.SetGeometry(
            ogr.CreateGeometryFromWkt("POINT (%d %d)" % (layer_index, layer_index + 1))
        )
    lyr.CreateFeature(feat)
    feat = None


def ogr_shape_54_test_layer(ds, layer_index):
    lyr = ds.GetLayerByName("layer%03d" % layer_index)
    assert lyr is not None, "failed for layer %d" % layer_index
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None, "failed for layer %d" % layer_index
    assert feat.GetField(0) == "val%d" % layer_index, (
        "failed for layer %d" % layer_index
    )
    if (layer_index % 2) == 0:
        assert feat.GetGeometryRef() is not None and feat.GetGeometryRef().ExportToWkt() == "POINT (%d %d)" % (
            layer_index,
            layer_index + 1,
        ), (
            "failed for layer %d" % layer_index
        )


def test_ogr_shape_54(tmp_vsimem):

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_54"
    # ds_name = 'tmp/ogr_shape_54'
    N = 500
    LRUListSize = 100

    # Test creating N layers
    ds = shape_drv.CreateDataSource(ds_name)
    for i in range(N):
        ogr_shape_54_create_layer(ds, i)

    ds = None

    # Test access to the N layers in sequence
    ds = ogr.Open(ds_name)
    for i in range(N):
        ogr_shape_54_test_layer(ds, i)

    # Now some 'random' access
    ogr_shape_54_test_layer(ds, N - 1 - LRUListSize)
    ogr_shape_54_test_layer(ds, N - LRUListSize / 2)
    ogr_shape_54_test_layer(ds, N - LRUListSize / 4)
    ogr_shape_54_test_layer(ds, 0)
    ogr_shape_54_test_layer(ds, 0)
    ogr_shape_54_test_layer(ds, 2)
    ogr_shape_54_test_layer(ds, 1)
    ds = None

    # Test adding a new layer
    ds = ogr.Open(ds_name, update=1)
    ogr_shape_54_create_layer(ds, N)
    ds = None

    # Test accessing the new layer
    ds = ogr.Open(ds_name)
    ogr_shape_54_test_layer(ds, N)
    ds = None

    # Test deleting layers
    ds = ogr.Open(ds_name, update=1)
    for i in range(N):
        ogr_shape_54_test_layer(ds, i)
    for i in range(N - LRUListSize + 1, N):
        ds.ExecuteSQL("DROP TABLE layer%03d" % i)
    ogr_shape_54_test_layer(ds, N - LRUListSize)
    ogr_shape_54_create_layer(ds, N + 2)
    for i in range(0, N - LRUListSize + 1):
        ds.ExecuteSQL("DROP TABLE layer%03d" % i)
    ogr_shape_54_test_layer(ds, N)
    ogr_shape_54_test_layer(ds, N + 2)
    ds = None

    # Destroy and recreate datasource
    shape_drv.DeleteDataSource(ds_name)
    ds = shape_drv.CreateDataSource(ds_name)
    for i in range(N):
        ogr_shape_54_create_layer(ds, i)
    ds = None

    # Reopen in read-only so as to be able to delete files */
    # if testing on a real filesystem.
    ds = ogr.Open(ds_name)

    # Test corner case where we cannot reopen a closed layer
    ideletedlayer = 0
    gdal.Unlink(ds_name / ("layer%03d.shp" % ideletedlayer))
    with gdal.quiet_errors():
        lyr = ds.GetLayerByName("layer%03d" % ideletedlayer)
    if lyr is not None:
        gdal.ErrorReset()
        with gdal.quiet_errors():
            lyr.ResetReading()
            lyr.GetNextFeature()
        assert gdal.GetLastErrorMsg() != ""
    gdal.ErrorReset()

    ideletedlayer = 1
    gdal.Unlink(ds_name / ("layer%03d.dbf" % ideletedlayer))
    lyr = ds.GetLayerByName("layer%03d" % ideletedlayer)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.ResetReading()
        lyr.GetNextFeature()
    # if gdal.GetLastErrorMsg() == '':
    #    gdaltest.post_reason('failed')
    #    return 'fail'
    gdal.ErrorReset()

    ds = None


###############################################################################
# Test that we cannot add more fields that the maximum allowed


def test_ogr_shape_55(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_55"
    ds = shape_drv.CreateDataSource(ds_name)
    lyr = ds.CreateLayer("ogr_shape_55")

    max_field_count = int((65535 - 33) / 32)  # 2046

    for i in range(max_field_count):
        if i == 255:
            gdal.ErrorReset()
            gdal.PushErrorHandler("CPLQuietErrorHandler")
        ret = lyr.CreateField(ogr.FieldDefn("foo%d" % i, ogr.OFTInteger))
        if i == 255:
            gdal.PopErrorHandler()
            assert (
                gdal.GetLastErrorMsg() != ""
            ), "expecting a warning for 256th field added"
        assert ret == 0, "failed creating field foo%d" % i

    i = max_field_count
    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("foo%d" % i, ogr.OFTInteger))
    assert ret != 0, "should have failed creating field foo%d" % i

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, i)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, i)
    lyr.CreateFeature(feat)

    ds = None


###############################################################################
# Test that we cannot add more fields that the maximum allowed record length


def test_ogr_shape_56(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_56"
    ds = shape_drv.CreateDataSource(ds_name)
    lyr = ds.CreateLayer("ogr_shape_56")

    max_field_count = int(65535 / 80)  # 819

    for i in range(max_field_count):
        if i == 255:
            gdal.ErrorReset()
            gdal.PushErrorHandler("CPLQuietErrorHandler")
        ret = lyr.CreateField(ogr.FieldDefn("foo%d" % i, ogr.OFTString))
        if i == 255:
            gdal.PopErrorHandler()
            assert (
                gdal.GetLastErrorMsg() != ""
            ), "expecting a warning for 256th field added"
        assert ret == 0, "failed creating field foo%d" % i

    i = max_field_count
    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("foo%d" % i, ogr.OFTString))
    assert ret != 0, "should have failed creating field foo%d" % i

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, "foo%d" % i)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, "foo%d" % i)
    lyr.CreateFeature(feat)

    ds = None


###############################################################################
# Test that we emit a warning if the truncation of a field value occurs


def test_ogr_shape_57(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_57"
    ds = shape_drv.CreateDataSource(ds_name)
    lyr = ds.CreateLayer("ogr_shape_57")

    field_defn = ogr.FieldDefn("foo", ogr.OFTString)
    field_defn.SetWidth(1024)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateField(field_defn)
    # print(gdal.GetLastErrorMsg())
    assert gdal.GetLastErrorMsg() != "", "expecting a warning"

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "0123456789" * 27)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr.CreateFeature(feat)
    # print(gdal.GetLastErrorMsg())
    assert gdal.GetLastErrorMsg() != "", "expecting a warning"

    ds = None


###############################################################################
# Test creating and reading back all geometry types


def test_ogr_shape_58(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_58"
    ds = shape_drv.CreateDataSource(ds_name)

    wkt_list = [
        "POINT (0 1)",
        "POINT (0 1 2)",
        "MULTIPOINT (0 1,2 3)",
        "MULTIPOINT (0 1 2,3 4 5)",
        "LINESTRING (0 1,2 3)",
        "LINESTRING (0 1 2,3 4 5)",
        "MULTILINESTRING ((0 1,2 3),(0 1,2 3))",
        "MULTILINESTRING ((0 1 2,3 4 5),(0 1 2,3 4 5))",
        "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
        "POLYGON ((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2))",
        "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)),((100 0,100 1,101 1,101 0,100 0)))",
        "MULTIPOLYGON (((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2)),((100 0 2,100 1 2,101 1 2,101 0 2,100 0 2)))",
    ]

    for wkt in wkt_list:
        geom = ogr.CreateGeometryFromWkt(wkt)
        layer_name = geom.GetGeometryName()
        if geom.GetGeometryType() & ogr.wkb25Bit:
            layer_name = layer_name + "3D"
        lyr = ds.CreateLayer(layer_name)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_58")

    for wkt in wkt_list:
        geom = ogr.CreateGeometryFromWkt(wkt)
        layer_name = geom.GetGeometryName()
        if geom.GetGeometryType() & ogr.wkb25Bit:
            layer_name = layer_name + "3D"
        lyr = ds.GetLayerByName(layer_name)
        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        geom_read = feat.GetGeometryRef()
        assert geom_read.ExportToWkt() == wkt, (
            "did not get expected geom for field %s" % layer_name
        )

    ds = None


###############################################################################
# Test reading a shape with XYM geometries


def test_ogr_shape_59(tmp_vsimem):

    shp_ds = ogr.Open("data/shp/testpointm.shp")
    if shp_ds is None:
        pytest.skip()
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert geom.GetGeometryName() == "POINT", "Geometry of wrong type."

    assert geom.GetCoordinateDimension() == 2, "dimension wrong."

    assert geom.GetPointZM(0) == (1.0, 2.0, 0.0, 3.0), "Did not get right point result."

    shp_ds = ogr.Open("data/shp/arcm_with_m.shp")
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToIsoWkt() == "LINESTRING M (0 0 10,1 1 20)"
    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToIsoWkt() == "MULTILINESTRING M ((0 0 10,1 1 20),(2 2 30,3 3 40))"
    )
    geom = None
    feat = None

    shp_ds = ogr.Open("data/shp/polygonm_with_m.shp")
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToIsoWkt() == "POLYGON M ((0 0 10,0 1 20,1 1 30,0 0 40))"
    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert (
        geom.ExportToIsoWkt()
        == "POLYGON M ((0 0 10,0 1 20,1 1 30,0 0 40),(0.25 0.25 50,0.75 0.75 60,0.25 0.75 70,0.25 0.25 80))"
    )
    geom = None
    feat = None


###############################################################################
# Test reading a shape with XYZM geometries


def test_ogr_shape_60():

    shp_ds = ogr.Open("data/shp/testpointzm.shp")
    if shp_ds is None:
        pytest.skip()
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert geom.GetGeometryName() == "POINT", "Geometry of wrong type."

    assert geom.GetCoordinateDimension() == 3, "dimension wrong."

    assert geom.GetPoint(0) == (1.0, 2.0, 3.0), "Did not get right point result."

    geom = None
    feat = None


###############################################################################
# Test field auto-growing


def test_ogr_shape_61(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_61"
    ds = shape_drv.CreateDataSource(ds_name)
    lyr = ds.CreateLayer("ogr_shape_61")

    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    field_defn = ogr.FieldDefn("intfield", ogr.OFTInteger)
    field_defn.SetWidth(1)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "0123456789" * 8)
    feat.SetField(1, 2)
    lyr.CreateFeature(feat)
    feat = None

    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetWidth() == 80, "did not get initial field size"

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "0123456789" * 9)
    feat.SetField(1, 34)
    lyr.CreateFeature(feat)
    feat = None

    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetWidth() == 90, "did not extend field"

    field_defn = lyr.GetLayerDefn().GetFieldDefn(1)
    assert field_defn.GetWidth() == 2, "did not extend field"

    ds = None

    ds = ogr.Open(ds_name)
    lyr = ds.GetLayer(0)
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetWidth() == 90, "did not get expected field size"

    feat = lyr.GetFeature(1)
    val = feat.GetFieldAsString(0)
    assert val == "0123456789" * 9, "did not get expected field value"
    val = feat.GetFieldAsInteger(1)
    assert val == 34, "did not get expected field value"


###############################################################################
# Test field resizing


def test_ogr_shape_62(tmp_vsimem):
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds_name = tmp_vsimem / "ogr_shape_62"
    ds = shape_drv.CreateDataSource(ds_name)
    lyr = ds.CreateLayer("ogr_shape_62", options=["RESIZE=YES"])

    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("bar", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("baz", ogr.OFTInteger))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "hugehugehugehuge")
    lyr.CreateFeature(feat)
    feat = None

    lyr.DeleteFeature(0)

    values = ["ab", "deef", "ghi"]
    for value in values:
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, value)
        feat.SetField(2, 12)
        lyr.CreateFeature(feat)
        feat = None

    ds = None

    # Reopen file
    ds = ogr.Open(ds_name)
    lyr = ds.GetLayer(0)

    # Check
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetWidth() == 4, "did not get expected field size"

    # Reopen file
    ds = ogr.Open(ds_name, update=1)
    lyr = ds.GetLayer(0)

    # Should do nothing
    ds.ExecuteSQL("RESIZE ogr_shape_62")

    # Check
    lyr.ResetReading()
    for expected_value in values:
        feat = lyr.GetNextFeature()
        got_val = feat.GetFieldAsString(0)
        assert got_val == expected_value, "did not get expected value"
        got_val = feat.GetFieldAsInteger(2)
        assert got_val == 12, "did not get expected value"

    ds = None


###############################################################################
# More testing of recoding


def test_ogr_shape_63(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_63.dbf"
    )
    lyr = ds.CreateLayer("ogr_shape_63", geom_type=ogr.wkbNone)
    gdaltest.fieldname = "\xc3\xa9"
    assert lyr.CreateField(ogr.FieldDefn(gdaltest.fieldname, ogr.OFTString)) == 0

    gdaltest.fieldname = "\xc3\xa9\xc3\xa9"
    assert (
        lyr.AlterFieldDefn(
            0, ogr.FieldDefn(gdaltest.fieldname, ogr.OFTString), ogr.ALTER_NAME_FLAG
        )
        == 0
    )

    chinese_str = struct.pack("B" * 6, 229, 144, 141, 231, 167, 176)
    chinese_str = chinese_str.decode("UTF-8")

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(
            0, ogr.FieldDefn(chinese_str, ogr.OFTString), ogr.ALTER_NAME_FLAG
        )

    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn(chinese_str, ogr.OFTString))

    assert ret != 0

    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_63.dbf")
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == gdaltest.fieldname
    ds = None

    # Set an invalid encoding
    gdal.FileFromMemBuffer(tmp_vsimem / "ogr_shape_63.cpg", "FOO")

    ds = ogr.Open(tmp_vsimem / "ogr_shape_63.dbf")
    lyr = ds.GetLayer(0)
    # TestCapability(OLCStringsAsUTF8) should return FALSE
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 0
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_63.dbf")
    gdal.Unlink(tmp_vsimem / "ogr_shape_63.cpg")


###############################################################################
# Test creating layers whose name include dot character


def test_ogr_shape_64(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_64"
    )

    lyr = ds.CreateLayer("a.b")
    assert lyr.GetName() == "a.b"
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    lyr.CreateFeature(feat)
    feat = None

    lyr = ds.CreateLayer("a.c")
    assert lyr.GetName() == "a.c"

    # Test that we cannot create a duplicate layer
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("a.b")
    assert lyr is None

    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_64/a.b.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("foo") == "bar"
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.b.shp")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.b.shx")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.b.dbf")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.c.shp")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.c.shx")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64/a.c.dbf")
    gdal.Unlink(tmp_vsimem / "ogr_shape_64")


###############################################################################
# Test reading a DBF with a 'nan' as a numeric value (#4799)


def test_ogr_shape_65():

    ds = ogr.Open("data/shp/nan.dbf")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsDouble(0)
    feat = None
    ds = None

    assert gdaltest.isnan(val)


###############################################################################
# Test failures when creating files and datasources


def test_ogr_shape_66(tmp_path):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/i_dont_exist/bar.dbf")
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("bar", geom_type=ogr.wkbNone)
    assert lyr is None
    ds = None

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/i_dont_exist/bar.shp")
    with gdal.quiet_errors():
        lyr = ds.CreateLayer("bar", geom_type=ogr.wkbPoint)
    assert lyr is None
    ds = None

    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/i_dont_exist/bar")
    assert ds is None

    f = open(tmp_path / "foo", "wb")
    f.close()
    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_path / "foo")
    assert ds is None


###############################################################################
# Test opening an empty .sbn spatial index


def test_ogr_shape_67(tmp_path):

    shutil.copy(
        "data/shp/emptyshapefilewithsbn.shp", tmp_path / "emptyshapefilewithsbn.shp"
    )
    shutil.copy(
        "data/shp/emptyshapefilewithsbn.shx", tmp_path / "emptyshapefilewithsbn.shx"
    )
    shutil.copy(
        "data/shp/emptyshapefilewithsbn.sbn", tmp_path / "emptyshapefilewithsbn.sbn"
    )
    shutil.copy(
        "data/shp/emptyshapefilewithsbn.sbx", tmp_path / "emptyshapefilewithsbn.sbx"
    )

    ds = ogr.Open(tmp_path / "emptyshapefilewithsbn.shp", update=1)
    ds.ExecuteSQL("DROP SPATIAL INDEX ON emptyshapefilewithsbn")
    ds = None

    with pytest.raises(OSError):
        os.stat(tmp_path / "emptyshapefilewithsbn.sbn")


###############################################################################
# Test opening a shape datasource with files with mixed case and then REPACK


@pytest.mark.skipif(sys.platform == "darwin", reason="Fails on MacOSX. Not sure why.")
def test_ogr_shape_68(tmp_path):

    for i in range(2):
        if i == 1 and sys.platform != "win32":
            break

        try:
            shutil.rmtree(tmp_path / "mixedcase")
        except OSError:
            pass
        os.mkdir(tmp_path / "mixedcase")
        shutil.copy("data/poly.shp", tmp_path / "mixedcase" / "mixedcase.shp")
        shutil.copy("data/poly.shx", tmp_path / "mixedcase" / "mixedcase.shx")
        shutil.copy(
            "data/poly.dbf", tmp_path / "mixedcase" / "MIXEDCASE.DBF"
        )  # funny !

        ds = ogr.Open(tmp_path / "mixedcase", update=1)
        if sys.platform == "win32":
            expected_layer_count = 1
        else:
            expected_layer_count = 2
        assert (
            ds.GetLayerCount() == expected_layer_count
        ), "expected %d layers, got %d" % (expected_layer_count, ds.GetLayerCount())
        if i == 1:
            lyr = ds.GetLayerByName("mixedcase")
        else:
            lyr = ds.GetLayerByName("MIXEDCASE")
        lyr.DeleteFeature(0)
        if i == 1:
            ds.ExecuteSQL("REPACK mixedcase")
        else:
            ds.ExecuteSQL("REPACK MIXEDCASE")

        if sys.platform == "win32":
            assert lyr.GetGeomType() == ogr.wkbPolygon
        else:
            assert lyr.GetGeomType() == ogr.wkbNone
            lyr = ds.GetLayerByName("mixedcase")
            assert lyr.GetGeomType() == ogr.wkbPolygon
            with gdal.quiet_errors():
                ret = lyr.DeleteFeature(0)
            assert ret != 0, "expected failure on DeleteFeature()"

            ds.ExecuteSQL("REPACK mixedcase")

        ds = None

        ori_shp_size = os.stat("data/poly.shp").st_size
        ori_shx_size = os.stat("data/poly.shx").st_size
        ori_dbf_size = os.stat("data/poly.dbf").st_size

        new_shp_size = os.stat(tmp_path / "mixedcase" / "mixedcase.shp").st_size
        new_shx_size = os.stat(tmp_path / "mixedcase" / "mixedcase.shx").st_size
        new_dbf_size = os.stat(tmp_path / "mixedcase" / "MIXEDCASE.DBF").st_size

        assert new_dbf_size != ori_dbf_size

        if sys.platform == "win32":
            assert new_shp_size != ori_shp_size
            assert new_shx_size != ori_shx_size
        else:
            assert new_shp_size == ori_shp_size
            assert new_shx_size == ori_shx_size


###############################################################################
# Test fix for #5135 (creating a field of type Integer with a big width)


def test_ogr_shape_69(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_69.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_69")
    field_defn = ogr.FieldDefn("intfield", ogr.OFTInteger)
    field_defn.SetWidth(64)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 123456)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_69.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTReal
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == 123456
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "ogr_shape_69.shp"
    )


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/pull/17
# (shapefile opened twice on Windows)


@pytest.mark.skipif(sys.platform != "win32", reason="Incorrect platform")
def test_ogr_shape_70(tmp_path):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "ogr_shape_70.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_70")
    field_defn = ogr.FieldDefn("intfield", ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    fid = feat.GetFID()
    feat = None
    lyr.DeleteFeature(fid)

    # Locks the file. No way to do this on Unix easily
    f = open(tmp_path / "ogr_shape_70.dbf", "r+")

    gdal.ErrorReset()
    with gdal.quiet_errors(), gdal.config_option("OGR_SHAPE_PACK_IN_PLACE", "NO"):
        ds.ExecuteSQL("REPACK ogr_shape_70")
    errmsg = gdal.GetLastErrorMsg()
    ds = None

    f.close()

    assert errmsg != ""


###############################################################################
# Test heterogeneous file permissions on .shp and .dbf.


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
def test_ogr_shape_71(tmp_path):

    if os.getuid() == 0:
        pytest.skip("running as root... skipping")

    import stat

    shutil.copy("data/poly.shp", tmp_path / "ogr_shape_71.shp")
    shutil.copy("data/poly.shx", tmp_path / "ogr_shape_71.shx")
    shutil.copy("data/poly.dbf", tmp_path / "ogr_shape_71.dbf")
    old_mode = os.stat(tmp_path / "ogr_shape_71.dbf").st_mode
    os.chmod(tmp_path / "ogr_shape_71.dbf", stat.S_IREAD)
    with gdal.quiet_errors():
        ds = ogr.Open(tmp_path / "ogr_shape_71.shp", update=1)
    ok = ds is None
    ds = None
    os.chmod(tmp_path / "ogr_shape_71.dbf", old_mode)

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_path / "ogr_shape_71.shp"
    )

    assert ok


###############################################################################
# Test shapefile size limit


def test_ogr_shape_72(tmp_path):

    # Determine if the filesystem supports sparse files (we don't want to create a real 3 GB
    # file !
    if gdaltest.filesystem_supports_sparse_files(tmp_path) is False:
        pytest.skip()

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "ogr_shape_72.shp"
    )
    lyr = ds.CreateLayer("2gb", geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(feat)
    ds = None

    f = open(tmp_path / "ogr_shape_72.shp", "rb+")
    f.seek(24)
    f.write(struct.pack("B" * 4, 0x7F, 0xFF, 0xFF, 0xFE))
    f.close()

    # Test creating a feature over 4 GB file limit -> should fail
    ds = ogr.Open(tmp_path / "ogr_shape_72.shp", update=1)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret != 0
    ds = None

    f = open(tmp_path / "ogr_shape_72.shp", "rb+")
    f.seek(24)
    f.write(struct.pack("B" * 4, 0x3F, 0xFF, 0xFF, 0xFE))
    f.close()

    # Test creating a feature over 2 GB file limit -> should fail
    with gdaltest.config_option("SHAPE_2GB_LIMIT", "TRUE"):
        ds = ogr.Open(tmp_path / "ogr_shape_72.shp", update=1)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (5 6)"))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret != 0
    ds = None

    # Test creating a feature over 2 GB file limit -> should succeed with warning
    ds = ogr.Open(tmp_path / "ogr_shape_72.shp", update=1)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (7 8)"))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert (
        gdal.GetLastErrorMsg().find("2GB file size limit reached") >= 0
    ), "did not find expected warning"
    ds = None

    ds = ogr.Open(tmp_path / "ogr_shape_72.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(1)
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (7 8)"
    ds = None


###############################################################################
# Test that isClockwise() works correctly on a degenerated ring that passes
# twice by the same point (#5342)


def test_ogr_shape_73(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_73.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_73", geom_type=ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    # (5 1) is the first(and last) point, and the pivot point selected by the
    # algorithm (lowest rightmost vertex), but it is also reused later in the
    # coordinate list
    # But the second ring is counter-clock-wise
    geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((0 0,0 10,10 10,10 0,0 0),(5 1,4 3,4 2,5 1,6 2,6 3,5 1))"
    )
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_73.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    got_geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == got_geom.ExportToWkt()
    ds = None


###############################################################################
# Test organizePolygons() in OGR_ORGANIZE_POLYGONS=DEFAULT mode when
# two outer rings are touching, by the first vertex of one.


def test_ogr_shape_74(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_74.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_74", geom_type=ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON (((0 10,10 10,10 0,0 0,0 1,9 1,9 9,0 9,0 10)),((9 5,5 4,0 5,5 6, 9 5)))"
    )
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_74.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    got_geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == got_geom.ExportToWkt()

    lyr.ResetReading()
    with gdaltest.config_option("OGR_ORGANIZE_POLYGONS", "DEFAULT"):
        feat = lyr.GetNextFeature()
    got_geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == got_geom.ExportToWkt()
    ds = None


###############################################################################
# Test GetFileList()


def test_ogr_shape_75(tmp_vsimem):

    ds = gdal.OpenEx("data/poly.shp")
    filelist_PRJ = [
        "data/poly.shp",
        "data/poly.shx",
        "data/poly.dbf",
        "data/poly.PRJ",
    ]
    filelist_prj = [
        "data/poly.shp",
        "data/poly.shx",
        "data/poly.dbf",
        "data/poly.prj",
    ]

    if (
        sys.platform in ("win32", "darwin")
        and "poly.PRJ" in os.listdir("data")
        and os.path.exists("data/poly.prj")
    ):
        # On Windows & Mac and if the filesystem is case insensitive (we detect
        # this by checking for the lowercase name...), then expect the uppercase
        # variant to be returned
        assert ds.GetFileList() == filelist_PRJ
    else:
        assert ds.GetFileList() == filelist_PRJ or ds.GetFileList() == filelist_prj
    ds = None

    ds = gdal.OpenEx("data/idlink.dbf")
    assert ds.GetFileList() == ["data/idlink.dbf"]
    ds = None

    ds = gdal.OpenEx("data/shp/testpoly.shp")
    assert ds.GetFileList() == [
        "data/shp/testpoly.shp",
        "data/shp/testpoly.shx",
        "data/shp/testpoly.dbf",
        "data/shp/testpoly.qix",
    ]
    ds = None

    ds = gdal.OpenEx("data/shp/emptyshapefilewithsbn.shx")
    assert ds.GetFileList() == [
        "data/shp/emptyshapefilewithsbn.shp",
        "data/shp/emptyshapefilewithsbn.shx",
        "data/shp/emptyshapefilewithsbn.sbn",
        "data/shp/emptyshapefilewithsbn.sbx",
    ]
    ds = None

    ds = gdal.OpenEx("data/shp/testpoly.shp")
    assert ds.GetFileList() == [
        "data/shp/testpoly.shp",
        "data/shp/testpoly.shx",
        "data/shp/testpoly.dbf",
        "data/shp/testpoly.qix",
    ]
    ds = None

    # Test that CreateLayer() + GetFileList() list the .prj file when it
    # exists
    src_ds = gdal.OpenEx("data/shp/Stacks.shp")
    driver = gdal.GetDriverByName("ESRI Shapefile")
    copy_ds = driver.CreateCopy(tmp_vsimem / "test_copy.shp", src_ds)
    src_ds = None
    try:
        assert copy_ds.GetFileList() == [
            f"{tmp_vsimem}/test_copy.shp",
            f"{tmp_vsimem}/test_copy.shx",
            f"{tmp_vsimem}/test_copy.dbf",
            f"{tmp_vsimem}/test_copy.prj",
        ]
    finally:
        copy_ds = None
        driver.Delete(tmp_vsimem / "test_copy.shp")

    # Test that CreateLayer() + GetFileList() don't list the .prj file when it
    # doesn't exist.
    src_ds = gdal.OpenEx("data/shp/testpoly.shp")
    driver = gdal.GetDriverByName("ESRI Shapefile")
    copy_ds = driver.CreateCopy(tmp_vsimem / "test_copy.shp", src_ds)
    src_ds = None
    try:
        assert copy_ds.GetFileList() == [
            f"{tmp_vsimem}/test_copy.shp",
            f"{tmp_vsimem}/test_copy.shx",
            f"{tmp_vsimem}/test_copy.dbf",
        ]
    finally:
        copy_ds = None
        driver.Delete(tmp_vsimem / "test_copy.shp")


###############################################################################
# Test opening shapefile whose .prj has a UTF-8 BOM marker


def test_ogr_shape_76():

    ds = ogr.Open("data/shp/prjwithutf8bom.shp")
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr.ExportToWkt().find('GEOGCS["NAD83"') == 0


###############################################################################
# Test opening shapefile whose .shx doesn't follow the official shapefile spec (#5608)


def test_ogr_shape_77():

    ds = ogr.Open("data/shp/nonconformant_shx_ticket5608.shp")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == "LINESTRING (0 1,2 3)"


###############################################################################
# Test writing integer values through double fields, and cases of truncation or
# loss of precision (#5625)


def test_ogr_shape_78(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_78.dbf"
    )
    lyr = ds.CreateLayer("ogr_shape_78")

    fd = ogr.FieldDefn("dblfield", ogr.OFTReal)
    fd.SetWidth(20)
    lyr.CreateField(fd)

    fd = ogr.FieldDefn("dblfield2", ogr.OFTReal)
    fd.SetWidth(20)
    fd.SetPrecision(1)
    lyr.CreateField(fd)

    # Integer values up to 2^53 can be exactly converted into a double.
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("dblfield", (2**53) * 1.0)
    lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() == "", "got unexpected error/warning"

    # Field width too small
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("dblfield2", 1e21)
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != "", "did not get expected error/warning"

    # Likely precision loss
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("dblfield", (2**53) * 1.0 + 2)  # 2^53+1 == 2^53 !
    with gdal.quiet_errors():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != "", "did not get expected error/warning"

    gdal.ErrorReset()
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_78.dbf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField("dblfield") == 9007199254740992.0
    ds = None


###############################################################################
# Test adding a field after creating features with 0 field


def test_ogr_shape_79(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_79.dbf"
    )
    lyr = ds.CreateLayer("ogr_shape_79")

    # This will create a (for now) invisible 'FID' field
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    # This will delete the implicit field
    fd = ogr.FieldDefn("field1", ogr.OFTReal)
    lyr.CreateField(fd)
    fd = ogr.FieldDefn("field2", ogr.OFTReal)
    lyr.CreateField(fd)

    # If the implicit field isn't deleted, this will cause crash
    lyr.ReorderField(0, 1)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_79.dbf")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    ds = None


###############################################################################
# Test reading a shape with invalid extent (nan values) (#5702)


def test_ogr_shape_80():

    ds = ogr.Open("data/shp/extentnan.shp")
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent()
    assert extent is None or extent[0] == extent[0]
    ds = None


###############################################################################
# Test REPACK after SetFeature() and geometry change (#XXXX)


def test_ogr_shape_81(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_81.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_81")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,-1 -1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_81.shp", update=1)
    lyr = ds.GetLayer(0)

    # Add junk behind our back
    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_shape_81.shp", "ab")
    gdal.VSIFWriteL("foo", 1, 3, f)
    gdal.VSIFCloseL(f)

    size_before = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size

    # Should be a no-op
    ds.ExecuteSQL("REPACK ogr_shape_81")
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after == size_before

    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2 2,3 3)"))
    lyr.SetFeature(f)

    # Should be a no-op
    ds.ExecuteSQL("REPACK ogr_shape_81")
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after == size_before

    # Writes a longer geometry. So .shp will be extended
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2 2,3 3,4 4)"))
    lyr.SetFeature(f)
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after != size_before

    # Should do something
    size_before = size_after
    ds.ExecuteSQL("REPACK ogr_shape_81")
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after != size_before

    # Writes a shorter geometry, so .shp should not change size.
    size_before = size_after
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(3 3,4 4)"))
    lyr.SetFeature(f)
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after == size_before
    size_before = size_after

    # Should do something
    ds.ExecuteSQL("REPACK ogr_shape_81")
    size_after = gdal.VSIStatL(tmp_vsimem / "ogr_shape_81.shp").size
    assert size_after != size_before

    ds = None


###############################################################################
# Test string length more than 254 bytes in UTF-8 encoding cut to 254 bytes


def test_ogr_shape_82(shape_ds):

    # create ogrlayer to test cut long strings with UTF-8 encoding
    shape_lyr = shape_ds.CreateLayer(
        "test_utf_cut", geom_type=ogr.wkbPoint, options=["ENCODING=UTF-8"]
    )

    # create field to put strings to automatic cut (254 is longest field length)
    field_defn = ogr.FieldDefn("cut_field", ogr.OFTString)
    field_defn.SetWidth(254)

    result = shape_lyr.CreateField(field_defn)

    assert result == 0, "failed to create new field."

    # Insert feature with long string in Russian.  Shoe repair ad.
    feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    init_rus = (
        "работает два мастера, установка набоек, замена подошвы, замена "
        "каблуков, растяжка обуви, растяжка голенищ сапог, швейные работы, "
        "ушив голенища сапога, чистка обуви, чистка замшевой обуви, замена "
        "стелек"
    )
    result_rus = (
        "работает два мастера, установка набоек, замена подошвы, замена "
        "каблуков, растяжка обуви, растяжка голенищ сапог, швейные работы, "
        "ушив голен"
    )
    feat.SetField("cut_field", init_rus)
    with gdal.quiet_errors():
        shape_lyr.CreateFeature(feat)

    # Insert feature with long a string in Russian.  Shoe repair ad.
    init_en = (
        "Remont kablukov i ih zamena; zamena naboek; profilaktika i remont "
        "podoshvy; remont i zamena supinatorov; zamena stelek; zamena obuvnoj "
        "furnitury; remont golenishha; rastjazhka obuvi; chistka i pokraska "
        "obuvi. Smolenskaja oblast, p. Monastyrshhina, ulica Sovetskaja, "
        "d. 38.	Rabotaet ponedelnik – chetverg s 9.00 do 18.00, pjatnica s "
        "10.00 do 17.00, vyhodnoj: subbota"
    )
    result_en = (
        "Remont kablukov i ih zamena; zamena naboek; profilaktika i remont "
        "podoshvy; remont i zamena supinatorov; zamena stelek; zamena "
        "obuvnoj furnitury; remont golenishha; rastjazhka obuvi; chistka "
        "i pokraska obuvi. Smolenskaja oblast, p. Monastyrshhina, ulica"
    )
    feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    feat.SetField("cut_field", init_en)
    shape_lyr.CreateFeature(feat)

    # TODO: check your language

    # save layer?

    # Read strings and compare with correct values.
    feat = shape_lyr.GetFeature(0)  # rus
    assert feat.cut_field == result_rus, "Wrong rus string cut"

    feat = shape_lyr.GetFeature(1)  # en
    assert feat.cut_field == result_en, "Wrong en string cut"


###############################################################################
# Test behaviour with curve geometries


def test_ogr_shape_83(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_83.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_83", geom_type=ogr.wkbCurvePolygon)
    assert lyr.GetGeomType() == ogr.wkbPolygon
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0))"))
    lyr.CreateFeature(f)
    f = None

    f = lyr.GetFeature(0)
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon
    ds = None


###############################################################################
# Test SPATIAL_INDEX creation option


def test_ogr_shape_84(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_84.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_84", options=["SPATIAL_INDEX=YES"])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    assert gdal.VSIStatL(tmp_vsimem / "ogr_shape_84.qix") is not None


###############################################################################
# Test Integer64


def test_ogr_shape_85(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_85.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_85")
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 123456789)
    f.SetField(1, 123456789012345678)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_85.shp", update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger64
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 123456789 and f.GetField(1) == 123456789012345678
    # Passing from 9 to 10 figures causes "promotion" to Integer64
    f.SetField(0, 2000000000)
    # Passing from 18 to 19 figures causes "promotion" to Real
    f.SetField(1, 9000000000000000000)
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_85.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger64
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTReal
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 2000000000 and f.GetField(1) == 9000000000000000000
    ds = None

    # Test open option ADJUST_TYPE
    ds = gdal.OpenEx(
        tmp_vsimem / "ogr_shape_85.shp",
        gdal.OF_VECTOR,
        open_options={"ADJUST_TYPE": "YES"},
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger64
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 2000000000 and f.GetField(1) == 9000000000000000000
    ds = None


def test_ogr_shape_85bis(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_85.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_85")
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 123456789)
    lyr.CreateFeature(f)
    fd = ogr.FieldDefn("foo", ogr.OFTInteger64)
    ret = lyr.AlterFieldDefn(0, fd, ogr.ALTER_TYPE_FLAG)
    assert ret == 0
    f.SetField(0, 123456789012345678)
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_85.shp", update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger64
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 123456789012345678
    ds = None


###############################################################################
# Robustness: test reading a non-conformant shapefile that mixes different shape type
# OGR can not produce such a file (unless patched)


def test_ogr_shape_86():

    ds = ogr.Open("data/shp/mixed_shape_type_non_conformant.shp")
    sql_lyr = ds.ExecuteSQL(
        "select count(distinct ogr_geometry) from mixed_shape_type_non_conformant"
    )
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    assert val == 6


###############################################################################
# Check we accept opening standalone .dbf files with weird header lengths (#6035)


def test_ogr_shape_87():

    ds = ogr.Open("data/shp/weird_header_length.dbf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 1


###############################################################################
# Test REPACK after SetFeature() and geometry change, without DBF


def test_ogr_shape_88(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_88.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_88")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_88.dbf")

    ds = ogr.Open(tmp_vsimem / "ogr_shape_88.shp", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1,2 2)"))
    lyr.SetFeature(f)

    ds.ExecuteSQL("REPACK ogr_shape_88")

    ds = None


###############################################################################
# Test reading geometry bigger than 10 MB


def test_ogr_shape_89(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_89.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_89")
    f = ogr.Feature(lyr.GetLayerDefn())
    g = ogr.Geometry(ogr.wkbLineString)
    g.AddPoint_2D(0, 0)
    g.AddPoint_2D(1, 1)
    f.SetGeometryDirectly(g)
    lyr.CreateFeature(f)
    f = None
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_89.dbf")

    # The declare file size doesn't match the real one
    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_shape_89.shx", "rb+")
    gdal.VSIFSeekL(f, 100 + 4, 0)
    gdal.VSIFWriteL(struct.pack(">i", int((10 * 1024 * 1024) / 2)), 1, 4, f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmp_vsimem / "ogr_shape_89.shp")
    lyr = ds.GetLayer(0)
    with gdal.quiet_errors():
        f = lyr.GetNextFeature()
    assert f is None or f.GetGeometryRef() is None
    ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_shape_89.shp", "rb+")
    gdal.VSIFSeekL(f, 100 + 8 + 10 * 1024 * 1024 - 1, 0)
    gdal.VSIFWriteL(struct.pack("B", 0), 1, 1, f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmp_vsimem / "ogr_shape_89.shp")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetPointCount() == 2

    ds = None


###############################################################################
# Test reading a lot of geometries


def test_ogr_shape_90(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_90.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_90")
    g = ogr.CreateGeometryFromWkt("POINT(0 0)")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None

    gdal.Unlink(tmp_vsimem / "ogr_shape_90.dbf")

    # The declare file size doesn't match the real one
    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_shape_90.shx", "rb+")
    filesize = int((100 + 8 * 1024 * 1024) / 2)
    gdal.VSIFSeekL(f, 24, 0)
    gdal.VSIFWriteL(struct.pack(">i", filesize), 1, 4, f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmp_vsimem / "ogr_shape_90.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    # Now it is consistent
    f = gdal.VSIFOpenL(tmp_vsimem / "ogr_shape_90.shx", "rb+")
    gdal.VSIFSeekL(f, 100 + 8 * 1024 * 1024 - 1, 0)
    gdal.VSIFWriteL(struct.pack("B", 0), 1, 1, f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmp_vsimem / "ogr_shape_90.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1024 * 1024

    ds = None


###############################################################################
# Test reading XYM geometries but with missing M array (#6317)


def test_ogr_shape_91():

    ds = ogr.Open("data/shp/arcm_without_m.shp")
    lyr = ds.GetLayer(0)
    for _ in lyr:
        pass

    ds = ogr.Open("data/shp/polygonm_without_m.shp")
    lyr = ds.GetLayer(0)
    for _ in lyr:
        pass


###############################################################################
# Test reading multipoint Z geometries without M


def test_ogr_shape_92():

    ds = ogr.Open("data/shp/multipointz_without_m.shp")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    wkt = f.GetGeometryRef().ExportToIsoWkt()
    assert wkt == "MULTIPOINT Z ((0 1 2),(3 4 5))"


###############################################################################
# Test reading point Z geometries without M


def test_ogr_shape_93():

    ds = ogr.Open("data/shp/pointz_without_m.shp")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    wkt = f.GetGeometryRef().ExportToIsoWkt()
    assert wkt == "POINT Z (1 2 3)"


###############################################################################
# Test SHPT creation option / CreateLayer(geom_type = xxx)


@pytest.mark.parametrize(
    "shpt,geom_type,wkt",
    [
        ("POINT", ogr.wkbPoint, "POINT (1 2)"),
        ("POINTM", ogr.wkbPointM, "POINT M (1 2 3)"),
        ("POINTZ", ogr.wkbPoint25D, "POINT Z (1 2 3)"),
        ("POINTZM", ogr.wkbPointZM, "POINT ZM (1 2 3 4)"),
        ("MULTIPOINT", ogr.wkbMultiPoint, "MULTIPOINT ((1 2))"),
        ("MULTIPOINTM", ogr.wkbMultiPointM, "MULTIPOINT M ((1 2 3))"),
        ("MULTIPOINTZ", ogr.wkbMultiPoint25D, "MULTIPOINT Z ((1 2 3))"),
        ("MULTIPOINTZM", ogr.wkbMultiPointZM, "MULTIPOINT ZM ((1 2 3 4))"),
        ("ARC", ogr.wkbLineString, "LINESTRING (1 2,3 4)"),
        ("ARCM", ogr.wkbLineStringM, "LINESTRING M (1 2 3,5 6 7)"),
        ("ARCZ", ogr.wkbLineString25D, "LINESTRING Z (1 2 3,5 6 7)"),
        ("ARCZM", ogr.wkbLineStringZM, "LINESTRING ZM (1 2 3 4,5 6 7 8)"),
        ("ARC", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4),(1 2,3 4))"),
        (
            "ARCM",
            ogr.wkbMultiLineStringM,
            "MULTILINESTRING M ((1 2 3,5 6 7),(1 2 3,5 6 7))",
        ),
        (
            "ARCZ",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((1 2 3,5 6 7),(1 2 3,5 6 7))",
        ),
        (
            "ARCZM",
            ogr.wkbMultiLineStringZM,
            "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8),(1 2 3 4,5 6 7 8))",
        ),
        ("POLYGON", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0))"),
        ("POLYGONM", ogr.wkbPolygonM, "POLYGON M ((0 0 2,0 1 2,1 1 2,1 0 2))"),
        ("POLYGONZ", ogr.wkbPolygon25D, "POLYGON Z ((0 0 2,0 1 2,1 1 2,1 0 2))"),
        (
            "POLYGONZM",
            ogr.wkbPolygonZM,
            "POLYGON ZM ((0 0 2 3,0 1 2 3,1 1 2 3,1 0 2 3))",
        ),
        (
            "POLYGON",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,1 0)),((100 0,100 1,101 1,101 0)))",
        ),
        (
            "POLYGONM",
            ogr.wkbMultiPolygonM,
            "MULTIPOLYGON M (((0 0 2,0 1 2,1 1 2,1 0 2)),((100 0 2,100 1 2,101 1 2,101 0 2)))",
        ),
        (
            "POLYGONZ",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON Z (((0 0 2,0 1 2,1 1 2,1 0 2)),((100 0 2,100 1 2,101 1 2,101 0 2)))",
        ),
        (
            "POLYGONZM",
            ogr.wkbMultiPolygonZM,
            "MULTIPOLYGON ZM (((0 0 2 3,0 1 2 3,1 1 2 3,1 0 2 3)),((100 0 2 3,100 1 2 3,101 1 2 3,101 0 2 3)))",
        ),
    ],
)
def test_ogr_shape_94(tmp_vsimem, shpt, geom_type, wkt):

    for i in range(2):
        ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
            tmp_vsimem / "ogr_shape_94.shp"
        )
        if i == 0:
            lyr = ds.CreateLayer("ogr_shape_94", options={"SHPT": shpt})
        else:
            lyr = ds.CreateLayer("ogr_shape_94", geom_type=geom_type)
        test_lyr_geom_type = (
            ogr.GT_Flatten(geom_type) != ogr.wkbMultiLineString
            and ogr.GT_Flatten(geom_type) != ogr.wkbMultiPolygon
        )
        assert not test_lyr_geom_type or lyr.GetGeomType() == geom_type, (
            i,
            shpt,
            geom_type,
            wkt,
            lyr.GetGeomType(),
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
        f = None
        ds = None
        ds = ogr.Open(tmp_vsimem / "ogr_shape_94.shp")
        lyr = ds.GetLayer(0)
        assert not test_lyr_geom_type or lyr.GetGeomType() == geom_type, (
            shpt,
            geom_type,
            wkt,
            lyr.GetGeomType(),
        )
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == wkt
        ds = None
        ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
            tmp_vsimem / "ogr_shape_94.shp"
        )


###############################################################################
# Test demoting of ZM to Z when the M values are nodata


def test_ogr_shape_95():

    ds = gdal.OpenEx("data/shp/pointzm_with_all_nodata_m.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT Z (1 2 3)", lyr.GetGeomType()

    ds = gdal.OpenEx(
        "data/shp/pointzm_with_all_nodata_m.shp", open_options=["ADJUST_GEOM_TYPE=NO"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, ogr.CreateGeometryFromWkt("POINT ZM (1 2 3 -1.79769313486232e+308)")
    )

    # The shape with a non nodata M is the second one
    ds = gdal.OpenEx(
        "data/shp/pointzm_with_one_valid_m.shp",
        open_options=["ADJUST_GEOM_TYPE=FIRST_SHAPE"],
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    ds = gdal.OpenEx(
        "data/shp/pointzm_with_one_valid_m.shp",
        open_options=["ADJUST_GEOM_TYPE=ALL_SHAPES"],
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPointZM


###############################################################################
# Test updating a XYM shapefile (#6331)


def test_ogr_shape_96(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_96.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_96")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT M (1 2 3)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_96.shp", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT M (1 2 3)"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT M (1 2 4)"))
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_96.shp")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT M (1 2 4)"
    ds = None


###############################################################################
# Test updating a XYZM shapefile


def test_ogr_shape_97(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_97.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_97")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT ZM (1 2 3 4)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_97.shp", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT ZM (1 2 3 4)"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT ZM (1 2 5 6)"))
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(tmp_vsimem / "ogr_shape_97.shp")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT ZM (1 2 5 6)"
    ds = None


###############################################################################
# Test restore function when .shx file is missing


def test_ogr_shape_98(tmp_path):

    with gdaltest.config_option("SHAPE_RESTORE_SHX", "TRUE"):
        shutil.copy("data/shp/can_caps.shp", tmp_path / "can_caps.shp")

        shp_ds = ogr.Open(tmp_path / "can_caps.shp", update=1)
        shp_lyr = shp_ds.GetLayer(0)

        assert shp_lyr.GetFeatureCount() == 13, "Got wrong number of features."

        shp_lyr = None
        shp_ds = None

    ref_shx = open("data/shp/can_caps.shx", "rb").read()
    got_shx = open(tmp_path / "can_caps.shx", "rb").read()

    assert ref_shx == got_shx, "Rebuilt shx is different from original shx."


###############################################################################
# Test restore function when .shx file is missing


@pytest.mark.parametrize(
    "geom_type,wkt",
    [
        (ogr.wkbPoint, "POINT (0 1)"),
        (ogr.wkbLineString, "LINESTRING (0 1,2 3)"),
        (ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,0 0))"),
        (ogr.wkbMultiPoint, "MULTIPOINT (0 1,2 3)"),
        (ogr.wkbMultiLineString, "MULTILINESTRING ((0 1,2 3))"),
        (ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))"),
        (ogr.wkbPointM, "POINT M (0 1 2)"),
        (ogr.wkbPoint25D, "POINT Z (0 1 2)"),
        (ogr.wkbPointZM, "POINT ZM (0 1 2 3)"),
        (ogr.wkbLineStringM, "LINESTRING M (0 1 10,2 3 10)"),
        (ogr.wkbLineString25D, "LINESTRING Z (0 1 10,2 3 10)"),
        (ogr.wkbLineStringZM, "LINESTRING ZM (0 1 10 20,2 3 10 20)"),
        (ogr.wkbPolygonM, "POLYGON M ((0 0 10,0 1 10,1 1 10,0 0 10))"),
        (ogr.wkbPolygon25D, "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))"),
        (ogr.wkbPolygonZM, "POLYGON ZM ((0 0 10 20,0 1 10 20,1 1 10 20,0 0 10 20))"),
        (ogr.wkbMultiPointM, "MULTIPOINT M (0 1 10,2 3 10)"),
        (ogr.wkbMultiPoint25D, "MULTIPOINT Z (0 1 10,2 3 10)"),
        (ogr.wkbMultiPointZM, "MULTIPOINT ZM (0 1 10 20,2 3 10 20)"),
        (ogr.wkbTINZ, "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))"),
    ],
)
def test_ogr_shape_restore_shx(tmp_vsimem, geom_type, wkt):

    filename = tmp_vsimem / "test_ogr_shape_restore_shx.shp"
    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer("test_ogr_shape_restore_shx", geom_type=geom_type)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)

    shx_filename = filename.with_suffix(".shx")

    f = gdal.VSIFOpenL(shx_filename, "rb")
    expected_data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)

    gdal.Unlink(shx_filename)

    with gdaltest.config_option("SHAPE_RESTORE_SHX", "TRUE"):
        ogr.Open(filename, update=1)

    f = gdal.VSIFOpenL(shx_filename, "rb")
    got_data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)

    assert got_data == expected_data


###############################################################################
# Test WGS 84 with a TOWGS84[0,0,0,0,0,0]


def test_ogr_shape_wgs84_with_zero_TOWGS84(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "test_ogr_shape_wgs84_with_zero_TOWGS84.shp"
    )
    lyr = ds.CreateLayer("test_ogr_shape_wgs84_with_zero_TOWGS84")
    ds = None
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test_ogr_shape_wgs84_with_zero_TOWGS84.prj",
        """GEOGCS["WGS84 Coordinate System",DATUM["WGS 1984",SPHEROID["WGS 1984",6378137,298.257223563],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"],AUTHORITY["SBMG","LAT-LONG,LAT-LONG,WGS84,METERS"]]""",
    )
    ds = ogr.Open(tmp_vsimem / "test_ogr_shape_wgs84_with_zero_TOWGS84.shp")
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToPrettyWkt()
    ds = None

    assert got_wkt.startswith("GEOGCS[")
    assert "4326" in got_wkt


###############################################################################
# Test a ETRS89-based CRS with a TOWGS84[0,0,0,0,0,0]
# Test case of https://lists.osgeo.org/pipermail/qgis-developer/2021-November/064340.html


def test_ogr_shape_etrs89_with_zero_TOWGS84(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "test_ogr_shape_etrs89_with_zero_TOWGS84.shp"
    )
    lyr = ds.CreateLayer("test_ogr_shape_etrs89_with_zero_TOWGS84")
    ds = None
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test_ogr_shape_etrs89_with_zero_TOWGS84.prj",
        """PROJCS["ETRS89 / Portugal TM06", GEOGCS["ETRS89", DATUM["European Terrestrial Reference System 1989", SPHEROID["GRS 1980", 6378137.0, 298.257222101, AUTHORITY["EPSG","7019"]], TOWGS84[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], AUTHORITY["EPSG","6258"]], PRIMEM["Greenwich", 0.0, AUTHORITY["EPSG","8901"]], UNIT["degree", 0.017453292519943295], AXIS["Geodetic longitude", EAST], AXIS["Geodetic latitude", NORTH], AUTHORITY["EPSG","4258"]], PROJECTION["Transverse_Mercator", AUTHORITY["EPSG","9807"]], PARAMETER["central_meridian", -8.133108333333334], PARAMETER["latitude_of_origin", 39.66825833333334], PARAMETER["scale_factor", 1.0], PARAMETER["false_easting", 0.0], PARAMETER["false_northing", 0.0], UNIT["m", 1.0], AXIS["Easting", EAST], AXIS["Northing", NORTH], AUTHORITY["EPSG","3763"]]""",
    )
    ds = ogr.Open(tmp_vsimem / "test_ogr_shape_etrs89_with_zero_TOWGS84.shp")
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "3763"
    assert "BOUNDCRS" not in srs.ExportToWkt(["FORMAT=WKT2"])
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "test_ogr_shape_etrs89_with_zero_TOWGS84.shp"
    )


###############################################################################
# Test REPACK with both implementations


@pytest.mark.parametrize("variant", ["YES", "NO"])
def test_ogr_shape_100(tmp_path, variant):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "ogr_shape_100.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_100")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1)"))
    f.SetField("foo", "1")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 1,2 2,3 3)"))
    f.SetField("foo", "2")
    lyr.CreateFeature(f)
    f = None
    lyr.DeleteFeature(0)
    with gdaltest.config_option("OGR_SHAPE_PACK_IN_PLACE", variant):

        f_dbf = None
        f_shp = None
        f_shx = None
        if sys.platform == "win32" and variant == "YES":
            # Locks the files. No way to do this on Unix easily
            f_dbf = open(tmp_path / "ogr_shape_100.dbf", "rb")
            f_shp = open(tmp_path / "ogr_shape_100.shp", "rb")
            f_shx = open(tmp_path / "ogr_shape_100.shx", "rb")

        ds.ExecuteSQL("REPACK ogr_shape_100")

        del f_dbf
        del f_shp
        del f_shx

    assert gdal.GetLastErrorMsg() == "", variant

    for ext in ["dbf", "shp", "shx", "cpg"]:
        assert gdal.VSIStatL(tmp_path / f"ogr_shape_100_packed.{ext}") is None, variant

    f = lyr.GetFeature(0)
    assert f["foo"] == "2"
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2,3 3)"

    with gdal.quiet_errors():
        f = lyr.GetFeature(1)
    assert f is None, variant
    lyr.ResetReading()
    assert lyr.GetFeatureCount() == 1, variant
    f = lyr.GetNextFeature()
    assert f["foo"] == "2"
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2,3 3)"
    f = lyr.GetNextFeature()
    assert f is None, variant
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (3 3,4 4,5 5,6 6)"))
    f.SetField("foo", "3")
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(tmp_path / "ogr_shape_100.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2, variant
    f = lyr.GetNextFeature()
    assert f["foo"] == "2"
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2,3 3)"

    f = lyr.GetNextFeature()
    assert f["foo"] == "3"
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (3 3,4 4,5 5,6 6)"

    f = lyr.GetNextFeature()
    assert f is None, variant
    ds = None


###############################################################################
# Test auto repack


def test_ogr_shape_101(tmp_vsimem):

    for i in range(2):

        # Auto-repack on create
        ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
            tmp_vsimem / "ogr_shape_101.shp"
        )
        lyr = ds.CreateLayer("ogr_shape_101")
        lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1)"))
        f.SetField("foo", "1")
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 1,2 2,3 3)"))
        f.SetField("foo", "2")
        lyr.CreateFeature(f)
        f = None
        lyr.DeleteFeature(0)

        if i == 0:
            ds = None
        else:
            ds.SyncToDisk()
            assert lyr.GetFeatureCount() == 1, i
            # No-op
            ds.ExecuteSQL("REPACK ogr_shape_101")

        ds_read = ogr.Open(tmp_vsimem / "ogr_shape_101.shp")
        lyr = ds_read.GetLayer(0)
        assert lyr.GetFeatureCount() == 1, i
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0
        assert f["foo"] == "2"
        assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2,3 3)"

        f = lyr.GetNextFeature()
        assert f is None, i

        ds = None
        ds_read = None

        if i == 0:

            # Auto-repack on update
            ds = ogr.Open(tmp_vsimem / "ogr_shape_101.shp", update=1)
            lyr = ds.GetLayer(0)

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (3 3,4 4,5 5,6 6)"))
            f.SetField("foo", "3")
            lyr.CreateFeature(f)

            lyr.DeleteFeature(0)
            ds = None

            ds = ogr.Open(tmp_vsimem / "ogr_shape_101.shp")
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 1, i
            f = lyr.GetNextFeature()
            assert f.GetFID() == 0
            assert f["foo"] == "3"
            assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (3 3,4 4,5 5,6 6)"

            f = lyr.GetNextFeature()
            assert f is None, i
            ds = None

            # Test disabling auto-repack on update
            ds = gdal.OpenEx(
                tmp_vsimem / "ogr_shape_101.shp",
                gdal.OF_UPDATE,
                open_options=["AUTO_REPACK=NO"],
            )
            lyr = ds.GetLayer(0)

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField("foo", "4")
            lyr.CreateFeature(f)

            lyr.DeleteFeature(0)
            ds = None

            ds = ogr.Open(tmp_vsimem / "ogr_shape_101.shp")
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 2, i
            f = lyr.GetNextFeature()
            assert f.GetFID() == 1
            assert f["foo"] == "4"

            ds = None

        ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
            tmp_vsimem / "ogr_shape_101.shp"
        )


###############################################################################
# Test reading invalid .prj


def test_ogr_shape_102(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "ogr_shape_102.shp"
    )
    lyr = ds.CreateLayer("ogr_shape_102", geom_type=ogr.wkbPoint)
    ds = None
    gdal.FileFromMemBuffer(tmp_vsimem / "ogr_shape_102.prj", "invalid")
    ds = ogr.Open(tmp_vsimem / "ogr_shape_102.shp")
    lyr = ds.GetLayer(0)
    lyr.GetSpatialRef()
    ds = None


###############################################################################
# Test handling of EOF character


def check_EOF(filename, expected=True):

    f = gdal.VSIFOpenL(filename, "rb")
    if f is None:
        print("%s does not exist" % filename)
        return False
    size = gdal.VSIStatL(filename).size
    content = gdal.VSIFReadL(1, size, f)
    gdal.VSIFCloseL(f)
    pos = content.find("\x1A".encode("LATIN1"))
    if expected:
        if pos < 0:
            print("Did not find EOF char")
            return False
        if pos != size - 1:
            print("Found EOF char but not at end of file!")
            return False
    elif pos >= 0:
        print("Found EOF char but we did not expect that !")
        return False
    return True


@pytest.mark.parametrize(
    "options,expected",
    [
        (["DBF_EOF_CHAR=YES"], True),
        ([], True),
        (["DBF_EOF_CHAR=NO"], False),
    ],
)
def test_ogr_shape_103(tmp_vsimem, options, expected):

    filename = tmp_vsimem / "ogr_shape_103.dbf"

    options += ["DBF_DATE_LAST_UPDATE=1970-01-01"]

    # Create empty file
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer("ogr_shape_103", geom_type=ogr.wkbNone, options=options)
    ds = None

    assert check_EOF(filename, expected=expected), options

    # Add field
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ds = None

    assert check_EOF(filename, expected=expected)

    # Add record
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None

    assert check_EOF(filename, expected=expected)

    # Add another field
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn("foo2", ogr.OFTString))
    ds = None

    assert check_EOF(filename, expected=expected)

    # Grow a field
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    fd = lyr.GetLayerDefn().GetFieldDefn(0)
    new_fd = ogr.FieldDefn(fd.GetName(), fd.GetType())
    new_fd.SetWidth(fd.GetWidth() + 1)
    lyr.AlterFieldDefn(0, fd, ogr.ALTER_ALL_FLAG)
    ds = None

    assert check_EOF(filename, expected=expected)

    # Reorder fields
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.ReorderFields([1, 0])
    ds = None

    assert check_EOF(filename, expected=expected)

    # Shrink a field
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    fd = lyr.GetLayerDefn().GetFieldDefn(0)
    new_fd = ogr.FieldDefn(fd.GetName(), fd.GetType())
    new_fd.SetWidth(fd.GetWidth() + 1)
    lyr.AlterFieldDefn(0, fd, ogr.ALTER_ALL_FLAG)
    ds = None

    assert check_EOF(filename, expected=expected)

    # Remove a field
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.DeleteField(0)
    ds = None

    assert check_EOF(filename, expected=expected)

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)

    # Create file with one field but no record
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer("ogr_shape_103", geom_type=ogr.wkbNone, options=options)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ds = None

    assert check_EOF(filename, expected=expected)
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)

    # Create file with two records
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer("ogr_shape_103", geom_type=ogr.wkbNone, options=options)
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None

    assert check_EOF(filename, expected=expected)

    # Test editing a record that is not the last one
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.SetFeature(lyr.GetNextFeature())
    ds = None

    assert check_EOF(filename, expected=expected)

    # Test editing the last record
    ds = gdal.OpenEx(filename, gdal.OF_UPDATE, open_options=options)
    lyr = ds.GetLayer(0)
    lyr.GetNextFeature()
    lyr.SetFeature(lyr.GetNextFeature())
    ds = None

    assert check_EOF(filename, expected=expected)

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)

    # Test appending to a file without a EOF marker
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer(
        "ogr_shape_103",
        geom_type=ogr.wkbNone,
        options={"DBF_EOF_CHAR": "NO", "DBF_DATE_LAST_UPDATE": "1970-01-01"},
    )
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None
    ds = gdal.OpenEx(
        filename, gdal.OF_UPDATE, open_options=["DBF_DATE_LAST_UPDATE=1970-01-01"]
    )
    lyr = ds.GetLayer(0)
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds.FlushCache()

    assert check_EOF(filename)
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)

    # Test editing a record (that is not the last one ) in a file without a EOF marker
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer(
        "ogr_shape_103",
        geom_type=ogr.wkbNone,
        options=["DBF_EOF_CHAR=NO"] + ["DBF_DATE_LAST_UPDATE=1970-01-01"],
    )
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None
    ds = gdal.OpenEx(
        filename, gdal.OF_UPDATE, open_options=["DBF_DATE_LAST_UPDATE=1970-01-01"]
    )
    lyr = ds.GetLayer(0)
    lyr.SetFeature(lyr.GetNextFeature())
    ds = None

    # To document our current behaviour. Could make sense to be changed.
    assert check_EOF(filename, expected=False)

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)


###############################################################################
# Test writing MULTIPATCH


@pytest.mark.parametrize(
    "wkt,lyr_type,options,expected_wkt",
    [
        ["TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))", ogr.wkbUnknown, [], None],
        [
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)),((0 0 0,1 1 3,2 2 4,0 0 0)))",
            ogr.wkbUnknown,
            [],
            None,
        ],  # triangle fan
        [
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)),((0 1 2,1 1 3,4 4 5,0 1 2)))",
            ogr.wkbUnknown,
            [],
            None,
        ],  # triangle strip
        [
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)),((1 1 3,0 1 2,4 4 5,1 1 3)))",
            ogr.wkbUnknown,
            [],
            None,
        ],  # no fan no strip
        [
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)),((0 0 0,0 1 2,1 1 3,0 0 0)),((1 1 3,0 1 2,4 4 5,1 1 3)))",
            ogr.wkbUnknown,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)),((1 1 3,0 1 2,4 4 5,1 1 3)))",
        ],
        # no fan no strip with duplicated triangle (as found in #5888)
        [
            "POLYHEDRALSURFACE Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
            ogr.wkbUnknown,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
        [
            "GEOMETRYCOLLECTION Z (TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0))))",
            ogr.wkbUnknown,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
        [
            "TRIANGLE Z ((0 0 0,0 1 2,1 1 3,0 0 0))",
            ogr.wkbUnknown,
            ["SHPT=MULTIPATCH"],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
        [
            "TRIANGLE Z ((0 0 0,0 1 2,1 1 3,0 0 0))",
            ogr.wkbTINZ,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
        [
            "POLYGON Z ((0 0 0,0 1 2,1 1 3,0 0 0))",
            ogr.wkbTINZ,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
        [
            "MULTIPOLYGON Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
            ogr.wkbTINZ,
            [],
            "TIN Z (((0 0 0,0 1 2,1 1 3,0 0 0)))",
        ],
    ],
)
def test_ogr_shape_104(tmp_vsimem, wkt, lyr_type, options, expected_wkt):

    if expected_wkt is None:
        expected_wkt = wkt

    filename = tmp_vsimem / "ogr_shape_104.shp"
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer("ogr_shape_104", geom_type=lyr_type, options=options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == expected_wkt
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)


###############################################################################
# Test reading .dbf with substantial padding after last field definition.


def test_ogr_shape_105():

    ds = ogr.Open("data/shp/padding_after_field_defns.dbf")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f["id"] == "1" and f["foo"] == "2"


###############################################################################
# Test that rewriting the last shape reuses the space it took. (#6787)


def test_ogr_shape_106(tmp_vsimem):

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(tmp_vsimem / "ogr_shape_106.shp")
    lyr = ds.CreateLayer("ogr_shape_81")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    size = gdal.VSIStatL(tmp_vsimem / "ogr_shape_106.shp").size
    assert size == 188

    ds = ogr.Open(tmp_vsimem / "ogr_shape_106.shp", update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # Write larger shape
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2 2,3 3,4 4)"))
    lyr.SetFeature(f)
    ds = None

    size = gdal.VSIStatL(tmp_vsimem / "ogr_shape_106.shp").size
    assert size == 188 + 2 * 8

    shape_drv.DeleteDataSource(tmp_vsimem / "ogr_shape_106.shp")


###############################################################################
# Compare to VSI*L file


def is_same(filename1, filename2, verbose=True):
    f1 = gdal.VSIFOpenL(filename1, "rb")
    if f1 is None:
        if verbose:
            print("%s does not exist" % filename1)
        return False
    f2 = gdal.VSIFOpenL(filename2, "rb")
    if f2 is None:
        if verbose:
            print("%s does not exist" % filename2)
        gdal.VSIFCloseL(f1)
        return False

    ret = True
    size1 = gdal.VSIStatL(filename1).size
    size2 = gdal.VSIStatL(filename2).size
    if size1 != size2:
        if verbose:
            print(
                "%s size is %d, whereas %s size is %d"
                % (filename1, size1, filename2, size2)
            )
        ret = False
    if ret:
        data1 = gdal.VSIFReadL(1, size1, f1)
        data2 = gdal.VSIFReadL(1, size2, f2)
        if data1 != data2:
            if verbose:
                print(
                    "File content of %s and %s are different" % (filename1, filename2)
                )
                print(struct.unpack("B" * len(data1), data1))
                print(struct.unpack("B" * len(data2), data2))
            ret = False

    gdal.VSIFCloseL(f1)
    gdal.VSIFCloseL(f2)
    return ret


###############################################################################
# Test that multiple edition of the last shape works properly (#7031)


def test_ogr_shape_107(tmp_vsimem):

    layer_name = "ogr_shape_107"
    filename = tmp_vsimem / f"{layer_name}.shp"
    copy_filename = tmp_vsimem / f"{layer_name}_copy.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")

    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name)

    # Create a shape
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2.5 3.5)"))
    lyr.CreateFeature(f)

    # Modify it to be larger
    f = lyr.GetFeature(0)
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"))
    lyr.SetFeature(f)

    # Insert new feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (5 6)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 2,3 4)"

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (5 6)"
    ds = None

    gdal.VectorTranslate(copy_filename, filename)
    assert is_same(copy_filename, filename)

    shape_drv.DeleteDataSource(copy_filename)
    shape_drv.DeleteDataSource(filename)

    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name)

    # Create a shape
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 2,1.5 2.5,3 4)"))
    lyr.CreateFeature(f)

    # Modify it to be smaller
    f = lyr.GetFeature(0)
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 2,3 4)"))
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 2,3 4)"
    ds = None

    gdal.VectorTranslate(copy_filename, filename)
    assert is_same(copy_filename, filename)

    shape_drv.DeleteDataSource(copy_filename)
    shape_drv.DeleteDataSource(filename)


###############################################################################
# Test spatial + attribute filter


def test_ogr_shape_108():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)
    with ogrtest.spatial_filter(lyr, 479750.6875, 4764702.0, 479750.6875, 4764702.0):
        expected_fc = lyr.GetFeatureCount()
        with ogrtest.attribute_filter(lyr, "1=1"):
            assert lyr.GetFeatureCount() == expected_fc


###############################################################################
# Test writing invalid polygon


def test_ogr_shape_109(tmp_vsimem):

    layer_name = "ogr_shape_109"
    filename = tmp_vsimem / f"{layer_name}.shp"

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name)

    # Create a shape
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((1 2))"))
    lyr.CreateFeature(f)

    ds = None


###############################################################################


def test_ogr_shape_110_write_invalid_multipatch(tmp_vsimem):

    layer_name = "ogr_shape_110"
    filename = tmp_vsimem / f"{layer_name}.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name, options=["SHPT=MULTIPATCH"])

    # Create a shape
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION(POINT(0 0))"))
    lyr.CreateFeature(f)

    ds = None


###############################################################################


def test_ogr_shape_111_delete_field_no_record(tmp_vsimem):

    layer_name = "ogr_shape_111_delete_field_no_record"
    filename = tmp_vsimem / f"{layer_name}.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name)
    lyr.CreateField(ogr.FieldDefn("field_1"))
    lyr.CreateField(ogr.FieldDefn("field_2"))
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    lyr.DeleteField(1)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "field_1"
    ds = None


###############################################################################


def test_ogr_shape_delete_all_fields_with_records(tmp_vsimem):

    # Scenario of https://github.com/qgis/QGIS/issues/51247
    layer_name = "ogr_shape_delete_all_fields_with_records"
    filename = tmp_vsimem / f"{layer_name}.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(filename)
    lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("field_1"))
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    # We could question if this is allowed...
    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"
    ds = None


###############################################################################


def test_ogr_shape_112_delete_layer(tmp_vsimem):

    dirname = tmp_vsimem
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(dirname)
    ds.CreateLayer("test")
    ds = None

    ds = ogr.Open(dirname)
    with gdal.quiet_errors():
        assert ds.DeleteLayer(0) != 0
    ds = None

    ds = ogr.Open(dirname, update=1)
    with gdal.quiet_errors():
        assert ds.DeleteLayer(-1) != 0
        assert ds.DeleteLayer(1) != 0
    gdal.FileFromMemBuffer(dirname / "test.cpg", "foo")
    assert ds.DeleteLayer(0) == 0
    assert not gdal.VSIStatL(dirname / "test.shp")
    assert not gdal.VSIStatL(dirname / "test.cpg")
    ds = None


###############################################################################


def test_ogr_shape_113_restore_shx_empty_shp_shx(tmp_vsimem):

    dirname = tmp_vsimem / "test_ogr_shape_113_restore_shx_empty_shp_shx"
    dbfname = dirname / "foo.dbf"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(dbfname)
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    lyr.CreateFeature(f)
    ds = None

    gdal.FileFromMemBuffer(dirname / "foo.shp", "")
    gdal.FileFromMemBuffer(dirname / "foo.shx", "")

    with gdaltest.config_option("SHAPE_RESTORE_SHX", "YES"):
        with gdal.quiet_errors():
            ds = ogr.Open(dbfname)
    assert ds
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo"] == "bar"
    ds = None


###############################################################################


def test_ogr_shape_layer_no_geom_but_srs(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_shape_layer_no_geom_but_srs"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(filename)
    sr = osr.SpatialReference()
    ds.CreateLayer("test", sr, ogr.wkbNone)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.CreateLayer("test2", sr, ogr.wkbNone)
    ds = None


###############################################################################


def test_ogr_shape_114_shz(tmp_path):

    shz_name = f"{tmp_path}/test_ogr_shape_114.shz"
    assert gdal.VectorTranslate(shz_name, "data/poly.shp")

    # Add an extra unrelated file
    f = gdal.VSIFOpenL("/vsizip/{" + shz_name + "}/README.TXT", "wb")
    assert f
    gdal.VSIFWriteL("hello", 1, len("hello"), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(shz_name)
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "test_ogr_shape_114"
    assert lyr.GetFeatureCount() == 10
    ds = None

    ds = ogr.Open(shz_name, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 0
    with gdal.quiet_errors():
        assert not ds.CreateLayer("foo")
    assert ds.TestCapability(ogr.ODsCDeleteLayer) == 0
    with gdal.quiet_errors():
        assert ds.DeleteLayer(0) == ogr.OGRERR_FAILURE
    assert lyr.DeleteFeature(1) == 0
    ds = None

    ds = ogr.Open(shz_name)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 9
    ds = None

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(
            test_cli_utilities.get_test_ogrsf_path() + " " + shz_name
        )
        assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    # Check that our extra unrelated file is still there
    assert gdal.VSIStatL("/vsizip/{" + shz_name + "}/README.TXT")

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    shape_drv.DeleteDataSource(shz_name)
    assert not gdal.VSIStatL(shz_name)

    with gdal.quiet_errors():
        assert not shape_drv.CreateDataSource("/i_do/not_exist/my.shz")


###############################################################################


@pytest.mark.skipif(
    gdaltest.is_ci(), reason="test skipped on CI due to random stalls on it"
)
def test_ogr_shape_115_shp_zip(tmp_path):

    dirname = str(tmp_path)
    filename = str(tmp_path / "test_ogr_shape_115.shp.zip")
    tmp_uncompressed = "test_ogr_shape_115.shp.zip_tmp_uncompressed"
    lockfile = "test_ogr_shape_115.shp.zip.gdal.lock"

    with gdaltest.config_option("OGR_SHAPE_PACK_IN_PLACE", "YES"):
        ds = gdal.VectorTranslate(filename, "data/poly.shp")
        assert tmp_uncompressed in gdal.ReadDir(dirname)
    assert ds
    with gdaltest.config_option("OGR_SHAPE_PACK_IN_PLACE", "NO"):
        assert gdal.VectorTranslate(ds, "data/poly.shp", options="-nln polyY")
    ds = None
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    with gdaltest.config_option("OGR_SHAPE_USE_VSIMEM_FOR_TEMP", "NO"):
        assert gdal.VectorTranslate(ds, "data/poly.shp", options="-nln polyX")
        assert tmp_uncompressed in gdal.ReadDir(dirname)
    ds = None
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    assert gdal.VectorTranslate(ds, "data/poly.shp", options="-nln polyZ")
    assert tmp_uncompressed not in gdal.ReadDir(dirname)
    ds = None

    # Add an extra unrelated file
    f = gdal.VSIFOpenL("/vsizip/{" + filename + "}/README.TXT", "wb")
    assert f
    gdal.VSIFWriteL("hello", 1, len("hello"), f)
    gdal.VSIFCloseL(f)

    with gdaltest.config_option("OGR_SHAPE_USE_VSIMEM_FOR_TEMP", "NO"):
        ds = ogr.Open(filename)
        assert ds.GetLayerCount() == 4
        assert [ds.GetLayer(i).GetName() for i in range(4)] == [
            "poly",
            "polyY",
            "polyX",
            "polyZ",
        ]
        assert [ds.GetLayer(i).GetFeatureCount() for i in range(4)] == [10, 10, 10, 10]
        assert tmp_uncompressed not in gdal.ReadDir(dirname)
        gdal.ErrorReset()
        ds.ExecuteSQL("UNCOMPRESS")
        assert gdal.GetLastErrorMsg() == ""
        assert tmp_uncompressed not in gdal.ReadDir(dirname)
        ds = None

        ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
        lyr = ds.GetLayer(0)
        assert tmp_uncompressed not in gdal.ReadDir(dirname)
        with gdaltest.config_option("OGR_SHAPE_LOCK_DELAY", "0.01"):
            ds.ExecuteSQL("UNCOMPRESS")
        assert tmp_uncompressed in gdal.ReadDir(dirname)
        assert lockfile in gdal.ReadDir(dirname)
        old_data = open(dirname + "/" + lockfile, "rb").read()
        time.sleep(0.1)
        assert open(dirname + "/" + lockfile, "rb").read() != old_data
        gdal.ErrorReset()
        ds.ExecuteSQL("UNCOMPRESS")
        assert gdal.GetLastErrorMsg() == ""
        assert len(ds.GetFileList()) == 1
        ds.ExecuteSQL("RECOMPRESS")
        assert tmp_uncompressed not in gdal.ReadDir(dirname)
        assert lockfile not in gdal.ReadDir(dirname)
        gdal.ErrorReset()
        ds.ExecuteSQL("RECOMPRESS")
        assert gdal.GetLastErrorMsg() == ""
        assert lyr.DeleteFeature(1) == 0
        assert tmp_uncompressed in gdal.ReadDir(dirname)
        assert lockfile in gdal.ReadDir(dirname)

    # Check lock file
    ds2 = ogr.Open(filename, update=1)
    lyr2 = ds2.GetLayer(0)
    with gdal.quiet_errors():
        assert lyr2.DeleteFeature(2) != 0
    ds2 = None
    ds = None

    ds = ogr.Open(filename)
    assert [ds.GetLayer(i).GetFeatureCount() for i in range(4)] == [9, 10, 10, 10]
    ds = None

    ds = ogr.Open(filename, update=1)
    assert ds.DeleteLayer(1) == 0
    ds = None

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 3
    assert [ds.GetLayer(i).GetName() for i in range(3)] == ["poly", "polyX", "polyZ"]
    ds = None

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(
            test_cli_utilities.get_test_ogrsf_path() + " " + filename
        )
        assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    # Check that our extra unrelated file is still there
    assert gdal.VSIStatL("/vsizip/{" + filename + "}/README.TXT")

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    shape_drv.DeleteDataSource(filename)
    assert not gdal.VSIStatL(filename)
    assert set(gdal.ReadDir(dirname)) in (set([".", ".."]), set([]))
    gdal.RmdirRecursive(dirname)

    with gdal.quiet_errors():
        assert not shape_drv.CreateDataSource("/i_do/not_exist/my.shp.zip")


###############################################################################


def test_ogr_shape_116_invalid_layer_name(tmp_path):

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    ds = shape_drv.CreateDataSource(tmp_path)
    with gdal.quiet_errors():
        assert ds.CreateLayer('test<>:"/\\?*', None, ogr.wkbNone)
    ds = None
    ds = ogr.Open(tmp_path)
    assert ds.GetLayerCount() == 1
    ds = None


###############################################################################
# Test case where a file with LDID/87 is overridden by a .cpg file


def test_ogr_shape_ldid_and_cpg(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.dbf", open("data/shp/facility_surface_dd.dbf", "rb").read()
    )
    gdal.FileFromMemBuffer(tmp_vsimem / "tmp.cpg", "UTF-8")
    ds = gdal.OpenEx(tmp_vsimem / "tmp.dbf")
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata_Dict("SHAPEFILE") == {
        "CPG_VALUE": "UTF-8",
        "ENCODING_FROM_CPG": "UTF-8",
        "ENCODING_FROM_LDID": "ISO-8859-1",
        "LDID_VALUE": "87",
        "SOURCE_ENCODING": "UTF-8",
    }
    ds = None

    # Disable recoding
    ds = gdal.OpenEx(tmp_vsimem / "tmp.dbf", open_options=["ENCODING="])
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata_Dict("SHAPEFILE") == {
        "CPG_VALUE": "UTF-8",
        "ENCODING_FROM_CPG": "UTF-8",
        "ENCODING_FROM_LDID": "ISO-8859-1",
        "LDID_VALUE": "87",
        "SOURCE_ENCODING": "",
    }
    ds = None

    gdal.Unlink(tmp_vsimem / "tmp.dbf")
    gdal.Unlink(tmp_vsimem / "tmp.cpg")


###############################################################################
# Test reading a shapefile with a point whose coordinates are NaN and with a spatial filter (#3542)


def test_ogr_shape_point_nan():

    ds = ogr.Open("data/shp/pointnan.shp")
    lyr = ds.GetLayer(0)
    with ogrtest.spatial_filter(lyr, 0, 0, 100, 100):
        count = 0
        for f in lyr:
            count += 1
    assert count == 1


###############################################################################
# Test writing a point with non-finite value


def test_ogr_shape_write_point_z_non_finite(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")
    g = ogr.Geometry(ogr.wkbPoint25D)
    g.AddPoint(0, 0, float("inf"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################
# Test writing a linestring with non-finite value


def test_ogr_shape_write_linestring_z_non_finite(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")
    g = ogr.Geometry(ogr.wkbLineString25D)
    g.AddPoint(0, 0, 0)
    g.AddPoint(0, 1, float("inf"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################
# Test writing a multilinestring with non-finite value


def test_ogr_shape_write_multilinestring_z_non_finite(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")
    ls = ogr.Geometry(ogr.wkbLineString25D)
    ls.AddPoint(0, 0, 0)
    ls.AddPoint(0, 1, float("inf"))
    g = ogr.Geometry(ogr.wkbMultiLineString25D)
    g.AddGeometry(ls)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################
# Test writing a polygon with non-finite value


def test_ogr_shape_write_polygon_z_non_finite(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")
    ls = ogr.Geometry(ogr.wkbLinearRing)
    ls.AddPoint(0, 0, 0)
    ls.AddPoint(0, 1, float("inf"))
    ls.AddPoint(1, 1, 0)
    ls.AddPoint(0, 0, 0)
    g = ogr.Geometry(ogr.wkbPolygon25D)
    g.AddGeometry(ls)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################
# Test writing a multipolygon with non-finite value


def test_ogr_shape_write_multipolygon_z_non_finite(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")
    ls = ogr.Geometry(ogr.wkbLinearRing)
    ls.AddPoint(0, 0, 0)
    ls.AddPoint(0, 1, float("inf"))
    ls.AddPoint(1, 1, 0)
    ls.AddPoint(0, 0, 0)
    p = ogr.Geometry(ogr.wkbPolygon25D)
    p.AddGeometry(ls)
    g = ogr.Geometry(ogr.wkbMultiPolygon25D)
    g.AddGeometry(p)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    with gdal.quiet_errors():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    ds = None


###############################################################################
# Test writing a multipolygon with parts slightly overlapping


def test_ogr_shape_write_multipolygon_parts_slightly_overlapping(tmp_vsimem):

    outfilename = tmp_vsimem / "out.shp"
    gdal.VectorTranslate(outfilename, "data/shp/slightly_overlapping_polygons.shp")
    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbMultiPolygon
    assert geom.GetGeometryCount() == 3

    # When using the full analyzer mode, one of the ring will be considered as
    # the inner ring of another one (which is arguable, as they are slightly
    # overlapping.
    with gdaltest.config_option("OGR_ORGANIZE_POLYGONS", "DEFAULT"):
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        geom = f.GetGeometryRef()
        assert geom.GetGeometryType() == ogr.wkbMultiPolygon
        assert geom.GetGeometryCount() == 2

    ds = None


###############################################################################
# Test writing a multipolygon with parts of non constant Z (#5315)


def test_ogr_shape_write_multipolygon_parts_non_constant_z(tmp_vsimem):

    outfilename = tmp_vsimem / "out.shp"
    gdal.VectorTranslate(outfilename, "data/shp/multipointz_non_constant_z.shp")
    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbMultiPolygon25D
    assert geom.GetGeometryCount() == 7
    ds = None


###############################################################################
# Test renaming a layer


def test_ogr_shape_rename_layer(tmp_path):

    outfilename = tmp_path / "test_rename.shp"
    gdal.VectorTranslate(outfilename, "data/poly.shp")

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCRename) == 1

    with gdal.quiet_errors():
        assert lyr.Rename("test_rename") != ogr.OGRERR_NONE

    f = gdal.VSIFOpenL(tmp_path / "test_rename_foo.dbf", "wb")
    assert f
    gdal.VSIFCloseL(f)

    with gdal.quiet_errors():
        assert lyr.Rename("test_rename_foo") != ogr.OGRERR_NONE

    gdal.Unlink(tmp_path / "test_rename_foo.dbf")

    assert sum(1 for f in lyr) == 10

    assert lyr.Rename("test_rename_foo") == ogr.OGRERR_NONE
    assert gdal.VSIStatL(tmp_path / "test_rename_foo.shp") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_foo.shx") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_foo.dbf") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_foo.prj") is not None
    assert lyr.GetDescription() == "test_rename_foo"
    assert lyr.GetLayerDefn().GetName() == "test_rename_foo"

    assert sum(1 for f in lyr) == 10

    ds.ExecuteSQL("ALTER TABLE test_rename_foo RENAME TO test_rename_bar")
    assert gdal.VSIStatL(tmp_path / "test_rename_bar.shp") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_bar.shx") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_bar.dbf") is not None
    assert gdal.VSIStatL(tmp_path / "test_rename_bar.prj") is not None
    assert lyr.GetDescription() == "test_rename_bar"
    assert lyr.GetLayerDefn().GetName() == "test_rename_bar"

    assert sum(1 for f in lyr) == 10

    ds = None


###############################################################################
# Test renaming a layer in a .shp.zip


def test_ogr_shape_rename_layer_zip(tmp_vsimem):

    outfilename = "tmp/test_rename.shp.zip"
    gdal.VectorTranslate(outfilename, "data/poly.shp")

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCRename) == 1

    assert lyr.Rename("test_rename_foo") == ogr.OGRERR_NONE
    assert lyr.GetDescription() == "test_rename_foo"
    assert lyr.GetLayerDefn().GetName() == "test_rename_foo"

    assert sum(1 for f in lyr) == 10

    assert lyr.Rename("test_rename_bar") == ogr.OGRERR_NONE
    assert lyr.GetDescription() == "test_rename_bar"
    assert lyr.GetLayerDefn().GetName() == "test_rename_bar"

    assert sum(1 for f in lyr) == 10

    ds = None


###############################################################################
# Test AlterGeomFieldDefn()


def test_ogr_shape_alter_geom_field_defn(tmp_vsimem):

    outfilename = tmp_vsimem / "test_ogr_shape_alter_geom_field_defn.shp"
    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(outfilename)
    srs_4326 = osr.SpatialReference()
    srs_4326.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(
        "test_ogr_shape_alter_geom_field_defn", geom_type=ogr.wkbPoint, srs=srs_4326
    )
    ds = None

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    other_srs = osr.SpatialReference()
    other_srs.ImportFromEPSG(4269)
    new_geom_field_defn.SetSpatialRef(other_srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().IsSame(other_srs)

    ds = None

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4269"

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is None
    ds = None

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is None

    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    new_geom_field_defn.SetSpatialRef(srs_4326)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    ds = None

    ds = ogr.Open(outfilename, update=1)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"

    # Wrong index
    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    with gdal.quiet_errors():
        assert (
            lyr.AlterGeomFieldDefn(
                -1, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

    # Changing geometry type ==> unsupported
    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbLineString)
    with gdal.quiet_errors():
        assert (
            lyr.AlterGeomFieldDefn(
                0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

    # Setting coordinate epoch ==> unsupported
    new_geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbPoint)
    srs_with_epoch = osr.SpatialReference()
    srs_with_epoch.ImportFromEPSG(4326)
    srs_with_epoch.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs_with_epoch)
    with gdal.quiet_errors():
        assert (
            lyr.AlterGeomFieldDefn(
                0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
            )
            != ogr.OGRERR_NONE
        )

    ds = None


###############################################################################
# Test writing non-planar polygon with inner ring


def test_ogr_shape_write_non_planar_polygon(tmp_vsimem):

    layer_name = "test_ogr_shape_write_non_planar_polygon"
    filename = tmp_vsimem / f"{layer_name}.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")

    with shape_drv.CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbPolygon25D)

        # Create a shape
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                "POLYGON Z ((516113.631069 5041435.137874 137.334, 516141.2239 5041542.465874 137.614, 515998.390418 5041476.527121 137.288, 516113.631069 5041435.137874 137.334), (516041.808551 5041476.527121 137.418, 516111.602184 5041505.337284 137.322, 516098.617322 5041456.644051 137.451, 516041.808551 5041476.527121 137.418))"
            )
        )
        lyr.CreateFeature(f)

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((516113.631069 5041435.137874 137.334,515998.390418 5041476.527121 137.288,516141.2239 5041542.465874 137.614,516113.631069 5041435.137874 137.334),(516041.808551 5041476.527121 137.418,516098.617322 5041456.644051 137.451,516111.602184 5041505.337284 137.322,516041.808551 5041476.527121 137.418))",
    )


###############################################################################


def test_ogr_shape_prj_with_wrong_axis_order(tmp_vsimem):

    layer_name = "test_ogr_shape_prj_with_wrong_axis_order"
    filename = tmp_vsimem / f"{layer_name}.shp"
    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    with shape_drv.CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbPolygon25D)
    prj = """GEOGCS["WGS 84",
    DATUM["World Geodetic System 1984",
        SPHEROID["WGS 84", 6378137.0, 298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich", 0.0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree", 0.017453292519943295],
    AXIS["Geodetic longitude", EAST],
    AXIS["Geodetic latitude", NORTH],
    AUTHORITY["EPSG","4326"]]
    """
    gdal.FileFromMemBuffer(tmp_vsimem / f"{layer_name}.prj", prj)
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    # Axis order has been changed
    assert lyr.GetSpatialRef().GetAxisName(None, 0) == "Latitude"
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################
# Test WriteArrowBatch() and fallback types


@gdaltest.enable_exceptions()
def test_ogr_shape_write_arrow_fallback_types(tmp_vsimem):

    src_ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("string", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    src_lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    src_lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("date", ogr.OFTDate))
    src_lyr.CreateField(ogr.FieldDefn("time", ogr.OFTTime))
    src_lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    src_lyr.CreateField(ogr.FieldDefn("binary", ogr.OFTBinary))
    src_lyr.CreateField(ogr.FieldDefn("stringlist", ogr.OFTStringList))
    src_lyr.CreateField(ogr.FieldDefn("intlist", ogr.OFTIntegerList))
    src_lyr.CreateField(ogr.FieldDefn("int64list", ogr.OFTInteger64List))
    src_lyr.CreateField(ogr.FieldDefn("reallist", ogr.OFTRealList))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["string"] = "foo"
    f["int"] = 123
    f["int64"] = 12345678901234
    f["real"] = 1.5
    f["date"] = "2023/10/06"
    f["time"] = "12:34:56"
    f["datetime"] = "2023/10/06 19:43:00"
    f.SetField("binary", b"\x01\x23\x46\x57\x89\xAB\xCD\xEF")
    f["stringlist"] = ["foo", "bar"]
    f["intlist"] = [1, 2]
    f["int64list"] = [12345678901234, 2]
    f["reallist"] = [1.5, 2.5]
    src_lyr.CreateFeature(f)

    filename = tmp_vsimem / "test_ogr_gpkg_write_arrow_fallback_types.shp"
    ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown
    )
    lyr = ds.CreateLayer("test")

    stream = src_lyr.GetArrowStream()
    schema = stream.GetSchema()

    success, error_msg = lyr.IsArrowSchemaSupported(schema)
    assert success

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() not in ("wkb_geometry", "OGC_FID"):
            lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        lyr.WriteArrowBatch(schema, array, ["FID=OGC_FID"])

    f = lyr.GetNextFeature()
    assert f["string"] == "foo"
    assert f["int"] == 123
    assert f["int64"] == 12345678901234
    assert f["real"] == 1.5
    assert f["date"] == "2023/10/06"
    assert f["time"] == "12:34:56"
    assert f["binary"] == "0123465789ABCDEF"
    assert f["datetime"] == "2023-10-06T19:43:00"
    assert f["stringlist"] == '[ "foo", "bar" ]'
    assert f["intlist"] == "[ 1, 2 ]"
    assert f["int64list"] == "[ 12345678901234, 2 ]"
    assert f["reallist"] == "[ 1.5, 2.5 ]"


###############################################################################
# Test WriteArrowBatch()


@gdaltest.enable_exceptions()
def test_ogr_shape_write_arrow_IF_FID_NOT_PRESERVED_ERROR(tmp_vsimem):

    src_ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    filename = tmp_vsimem / "test_ogr_shape_write_arrow_IF_FID_NOT_PRESERVED_ERROR.shp"
    ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown
    )
    lyr = ds.CreateLayer("test")

    stream = src_lyr.GetArrowStream()
    schema = stream.GetSchema()

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        with pytest.raises(Exception, match="Feature id 1 not preserved"):
            lyr.WriteArrowBatch(
                schema, array, ["FID=OGC_FID", "IF_FID_NOT_PRESERVED=ERROR"]
            )


###############################################################################
# Test writing an invalid "0000/00/00" date


@gdaltest.enable_exceptions()
def test_ogr_shape_write_date_0000_00_00(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_shape_write_date_0000_00_00.shp"
    ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown
    )
    lyr = ds.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("date", ogr.OFTDate))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["date"] = "0000/00/00"
    lyr.CreateFeature(f)
    f = None
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.IsFieldNull("date")


###############################################################################
# Test GetArrowStream()


def test_ogr_shape_arrow_stream():
    pytest.importorskip("osgeo.gdal_array")
    pytest.importorskip("numpy")

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 5
    assert len(batches[0]["OGC_FID"]) == 10
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    assert list(batches[0]["EAS_ID"]) == [
        168,
        179,
        171,
        173,
        172,
        169,
        166,
        158,
        165,
        170,
    ]


###############################################################################
# Test GetArrowStream()


def test_ogr_shape_arrow_stream_fid_optim(tmp_vsimem):
    pytest.importorskip("osgeo.gdal_array")
    pytest.importorskip("numpy")

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)
    ignored_fields = ["OGR_GEOMETRY"]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        ignored_fields.append(lyr.GetLayerDefn().GetFieldDefn(i).GetName())
    lyr.SetIgnoredFields(ignored_fields)

    # Optimized code path
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "YES"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 1
    assert len(batches[0]["OGC_FID"]) == 10
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

    # Optimized code path
    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "MAX_FEATURES_IN_BATCH=7"]
    )
    batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "YES"
    )
    assert len(batches) == 2
    assert len(batches[0]) == 1
    assert len(batches[0]["OGC_FID"]) == 7
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6]
    assert len(batches[1]["OGC_FID"]) == 3
    assert list(batches[1]["OGC_FID"]) == [7, 8, 9]

    # Regular code path
    lyr.SetAttributeFilter("1 = 1")
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 1
    assert len(batches[0]["OGC_FID"]) == 10
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

    # Regular code path
    minx, maxx, miny, maxy = lyr.GetExtent()
    lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetSpatialFilter(None)
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 0

    # Regular code path
    lyr.SetIgnoredFields(ignored_fields[0:-1])
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetIgnoredFields(ignored_fields)
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 2
    assert len(batches[0]["OGC_FID"]) == 10
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

    # Regular code path
    lyr.SetIgnoredFields(ignored_fields[1:])
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetIgnoredFields(ignored_fields)
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 2
    assert len(batches[0]["OGC_FID"]) == 10
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

    # Regular code path
    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "INCLUDE_FID=NO"]
    )
    batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "NO"
    )
    assert len(batches) == 0

    del ds

    # Test hole in FID numbering
    filename = str(tmp_vsimem / "test_ogr_shape_arrow_stream_fid_optim.shp")
    ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown
    )
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["AUTO_REPACK=NO"])
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f["str"] = "foo"
        lyr.CreateFeature(f)
        f = None
    lyr.DeleteFeature(3)
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    lyr.SetIgnoredFields(["OGR_GEOMETRY", "str"])

    # Optimized code path
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "YES"
    )
    assert len(batches) == 1
    assert len(batches[0]) == 1
    assert len(batches[0]["OGC_FID"]) == 4
    assert list(batches[0]["OGC_FID"]) == [0, 1, 2, 4]

    f = gdal.VSIFOpenL(filename[0:-4] + ".dbf", "rb+")
    assert f
    gdal.VSIFTruncateL(f, 300)
    gdal.VSIFCloseL(f)

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    with gdal.quiet_errors():
        batches = [batch for batch in stream]
    assert (
        lyr.GetMetadataItem(
            "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH", "__DEBUG__"
        )
        == "YES"
    )
    assert len(batches) == 0


###############################################################################
# Test DBF Logical field type


@gdaltest.enable_exceptions()
def test_ogr_shape_logical_field(tmp_vsimem):

    filename = tmp_vsimem / "test_ogr_shape_logical_field.shp"
    ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        filename, 0, 0, 0, gdal.GDT_Unknown
    )
    lyr = ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("bool_field", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["bool_field"] = True
    f["int_field"] = 1234
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["bool_field"] = False
    f["int_field"] = -1234
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f["bool_field"] = None
    lyr.CreateFeature(f)
    f = None
    ds.Close()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetType() == ogr.OFTInteger
    assert fld_defn.GetSubType() == ogr.OFSTBoolean
    assert fld_defn.GetWidth() == 1
    f = lyr.GetNextFeature()
    assert f["bool_field"] == True
    assert f["int_field"] == 1234
    f = lyr.GetNextFeature()
    assert f["bool_field"] == False
    assert f["int_field"] == -1234
    f = lyr.GetNextFeature()
    assert f["bool_field"] is None
