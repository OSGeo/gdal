#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR GMT driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("OGR_GMT")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():
    yield
    gdaltest.clean_tmp()


###############################################################################
# Create table from data/poly.shp


def test_ogr_gmt_2():

    gmt_drv = ogr.GetDriverByName("GMT")
    gmt_ds = gmt_drv.CreateDataSource("tmp/tpoly.gmt")

    #######################################################
    # Create gmtory Layer
    gmt_lyr = gmt_ds.CreateLayer("tpoly")
    assert gmt_lyr.GetDataset().GetDescription() == gmt_ds.GetDescription()

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        gmt_lyr,
        [("AREA", ogr.OFTReal), ("EAS_ID", ogr.OFTInteger), ("PRFEDEA", ogr.OFTString)],
    )

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gmt_lyr.GetLayerDefn())

    shp_ds = ogr.Open("data/poly.shp")
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    poly_feat = []

    while feat is not None:

        poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gmt_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    gmt_lyr = None
    gmt_ds = None

    # Verify that stuff we just wrote is still OK.

    gmt_ds = ogr.Open("tmp/tpoly.gmt")
    gmt_lyr = gmt_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(gmt_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(gmt_lyr, "eas_id", expect)

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = gmt_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.000000001
        )

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )


###############################################################################
# Verify reading of multilinestring file. (#3802)


def test_ogr_gmt_4():

    ds = ogr.Open("data/gmt/test_multi.gmt")
    lyr = ds.GetLayer(0)

    assert (
        lyr.GetLayerDefn().GetGeomType() == ogr.wkbMultiLineString
    ), "did not get expected multilinestring type."

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "MULTILINESTRING ((175 -45,176 -45),(180.0 -45.3,179.0 -45.4))"
    )

    assert feat.GetField("name") == "feature 1", "got wrong name, feature 1"

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "MULTILINESTRING ((175.1 -45.0,175.2 -45.1),(180.1 -45.3,180.0 -45.2))"
    )

    assert feat.GetField("name") == "feature 2", "got wrong name, feature 2"

    feat = lyr.GetNextFeature()

    assert feat is None, "did not get null feature when expected."


###############################################################################
# Write a multipolygon file and verify it.


def test_ogr_gmt_5():

    #######################################################
    # Create gmtory Layer
    gmt_drv = ogr.GetDriverByName("GMT")
    gmt_ds = gmt_drv.CreateDataSource("tmp/mpoly.gmt")
    gmt_lyr = gmt_ds.CreateLayer("mpoly")

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gmt_lyr, [("ID", ogr.OFTInteger)])

    #######################################################
    # Write a first multipolygon

    dst_feat = ogr.Feature(feature_def=gmt_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((0 0,0 10,10 10,0 10,0 0),(3 3,4 4, 3 4,3 3)),((12 0,14 0,12 3,12 0)))"
        )
    )
    dst_feat.SetField("ID", 15)
    with gdal.config_option("GMT_USE_TAB", "TRUE"):  # Ticket #6453
        gmt_lyr.CreateFeature(dst_feat)

    dst_feat = ogr.Feature(feature_def=gmt_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("MULTIPOLYGON(((30 20,40 20,30 30,30 20)))")
    )
    dst_feat.SetField("ID", 16)
    gmt_lyr.CreateFeature(dst_feat)

    gmt_lyr = None
    gmt_ds = None

    # Reopen.

    assert "@R" in open("tmp/mpoly.gmt", "rt", encoding="UTF-8").read()

    ds = ogr.Open("tmp/mpoly.gmt")
    lyr = ds.GetLayer(0)

    assert (
        lyr.GetLayerDefn().GetGeomType() == ogr.wkbMultiPolygon
    ), "did not get expected multipolygon type."

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat,
        "MULTIPOLYGON(((0 0,0 10,10 10,0 10,0 0),(3 3,4 4, 3 4,3 3)),((12 0,14 0,12 3,12 0)))",
    )

    assert feat.GetField("ID") == 15, "got wrong id, first feature"

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "MULTIPOLYGON(((30 20,40 20,30 30,30 20)))")

    assert feat.GetField("ID") == 16, "got wrong ID, second feature"

    feat = lyr.GetNextFeature()

    assert feat is None, "did not get null feature when expected."


###############################################################################
# Test reading a file with just point coordinates


def test_ogr_gmt_coord_only():

    with gdaltest.tempfile("/vsimem/test.gmt", """1 2 3\n"""):
        ds = ogr.Open("/vsimem/test.gmt")
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, "POINT Z (1 2 3)")


###############################################################################
# Test writing to stdout


def test_ogr_gmt_write_stdout():

    gdal.VectorTranslate(
        "/vsistdout_redirect//vsimem/test.gmt", "data/poly.shp", format="GMT"
    )
    ds = ogr.Open("/vsimem/test.gmt")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon
    assert lyr.GetFeatureCount() == 10
    ds = None
    gdal.Unlink("/vsimem/test.gmt")
