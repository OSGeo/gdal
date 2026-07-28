#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Oracle OCI driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.skipif(
        "OCI_DSNAME" not in os.environ, reason="no OCI_DSNAME in environment"
    ),
    pytest.mark.require_driver("OCI"),
]


@pytest.fixture(scope="module", autouse=True)
def setup_tests():
    with gdaltest.disable_exceptions():
        gdaltest.oci_ds = ogr.Open(os.environ["OCI_DSNAME"])
    if gdaltest.oci_ds is None:
        pytest.skip("Cannot open %s" % os.environ["OCI_DSNAME"])
    yield

    gdaltest.oci_ds.ExecuteSQL("DELLAYER:tpoly")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:xpoly")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:testsrs")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:testsrs2")
    try:
        gdaltest.oci_ds.ExecuteSQL("drop table geom_test")
    except Exception:
        pass
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_POINT")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_POINT3")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_LINESTRING")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_LINESTRING3")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_POLYGON")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_POLYGON3")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_MULTIPOINT")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_MULTILINESTRING")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_MULTIPOLYGON")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_GEOMETRYCOLLECTION")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:test_NONE")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:testdate")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:testdate_with_tz")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:ogr_oci_20")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:ogr_oci_20bis")
    gdaltest.oci_ds.ExecuteSQL("DELLAYER:ogr_oci_21")

    gdaltest.oci_ds = None
    gdaltest.shp_ds = None


###############################################################################
# Create Oracle table from data/poly.shp


def test_ogr_oci_2():

    with gdal.quiet_errors():
        gdaltest.oci_ds.ExecuteSQL("DELLAYER:tpoly")

    ######################################################
    # Create Oracle Layer
    gdaltest.oci_lyr = gdaltest.oci_ds.CreateLayer("tpoly", options=["DIM=3"])

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        gdaltest.oci_lyr,
        [("AREA", ogr.OFTReal), ("EAS_ID", ogr.OFTInteger), ("PRFEDEA", ogr.OFTString)],
    )

    ######################################################
    # Copy in poly.shp

    shp_ds = ogr.Open("data/poly.shp")
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    gdaltest.poly_feat = []

    for feat in shp_lyr:

        gdaltest.poly_feat.append(feat)

        dst_feat = ogr.Feature(feature_def=gdaltest.oci_lyr.GetLayerDefn())
        dst_feat.SetFrom(feat)
        gdaltest.oci_lyr.CreateFeature(dst_feat)

    # Test updating non-existing feature
    shp_lyr.ResetReading()
    feat = shp_lyr.GetNextFeature()
    feat.SetFID(-10)
    assert (
        gdaltest.oci_lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."

    # Test deleting non-existing feature
    assert (
        gdaltest.oci_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."


###############################################################################
# Helper method to reverse ring winding.  This is needed because the
# winding direction in shapefiles, and in Oracle is opposite for polygons.


def reverse_rings(poly):

    for i_ring in range(poly.GetGeometryCount()):
        ring = poly.GetGeometryRef(i_ring)
        v_count = ring.GetPointCount()
        for i_vert in range(v_count // 2):
            i_other = v_count - i_vert - 1
            p1 = (ring.GetX(i_vert), ring.GetY(i_vert), ring.GetZ(i_vert))
            ring.SetPoint(
                i_vert, ring.GetX(i_other), ring.GetY(i_other), ring.GetZ(i_other)
            )
            ring.SetPoint(i_other, p1[0], p1[1], p1[2])


###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_oci_3():

    expect = [168, 169, 166, 158, 165]

    gdaltest.oci_lyr.SetAttributeFilter("eas_id < 170")
    tr = ogrtest.check_features_against_list(gdaltest.oci_lyr, "eas_id", expect)
    gdaltest.oci_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.oci_lyr.GetNextFeature()

        ref_geom = orig_feat.GetGeometryRef()
        ref_geom.SetCoordinateDimension(3)
        reverse_rings(ref_geom)

        ogrtest.check_feature_geometry(read_feat, ref_geom, max_error=0.000000001)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_oci_4():

    wkt_list = ["10", "2", "1", "3d_1", "4", "5", "6"]

    for item in wkt_list:

        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat = ogr.Feature(feature_def=gdaltest.oci_lyr.GetLayerDefn())
        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        gdaltest.oci_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.oci_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.oci_lyr.GetNextFeature()
        geom_ref = feat_read.GetGeometryRef()
        geom.SetCoordinateDimension(3)
        ogrtest.check_feature_geometry(geom_ref, geom)


###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_oci_5():

    expect = [None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158]

    sql_lyr = gdaltest.oci_ds.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0

    tr = ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)

    gdaltest.oci_ds.ReleaseResultSet(sql_lyr)

    assert tr


###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_oci_6():

    sql_lyr = gdaltest.oci_ds.ExecuteSQL("select * from tpoly where prfedea = '2'")
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 1

    try:
        assert ogrtest.check_features_against_list(sql_lyr, "prfedea", ["2"]) == 1
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        expected_geom = ogr.CreateGeometryFromWkt(
            "MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))"
        )
        expected_geom.SetCoordinateDimension(3)
        ogrtest.check_feature_geometry(feat_read, expected_geom)
        feat_read = None
    finally:
        gdaltest.oci_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test spatial filtering.


def test_ogr_oci_7():

    gdaltest.oci_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt("LINESTRING(479505 4763195,480526 4762819)")
    gdaltest.oci_lyr.SetSpatialFilter(geom)

    tr = ogrtest.check_features_against_list(gdaltest.oci_lyr, "eas_id", [158])

    gdaltest.oci_lyr.SetSpatialFilter(None)

    assert tr


###############################################################################
# Test that we can create a layer with a coordinate system that is mapped
# to an oracle coordinate system using the ORACLE authority code.


def test_ogr_oci_8():

    #######################################################
    # Preclean.

    with gdal.quiet_errors():
        gdaltest.oci_ds.ExecuteSQL("DELLAYER:testsrs")

    #######################################################
    # Prepare an SRS with an ORACLE authority code.
    srs = osr.SpatialReference()
    srs.SetGeogCS(
        "gcs_dummy",
        "datum_dummy",
        "ellipse_dummy",
        osr.SRS_WGS84_SEMIMAJOR,
        osr.SRS_WGS84_INVFLATTENING,
    )
    srs.SetAuthority("GEOGCS", "Oracle", 8241)

    #######################################################
    # Create Oracle Layer
    oci_lyr2 = gdaltest.oci_ds.CreateLayer("testsrs", srs=srs, options=["INDEX=FALSE"])

    #######################################################
    # Now check that the srs for the layer is really the built-in
    # oracle SRS.
    srs2 = oci_lyr2.GetSpatialRef()

    assert (
        srs2.GetAuthorityCode("GEOGCS") == "8241"
    ), "Did not get expected authority code"

    assert (
        srs2.GetAuthorityName("GEOGCS") == "Oracle"
    ), "Did not get expected authority name"

    assert (
        srs2.GetAttrValue("GEOGCS|DATUM") == "Kertau 1948"
    ), "Did not get expected datum name"


###############################################################################
# This time we create a layer with a EPSG marked GEOGCS, and verify that
# the CRS is properly round-tripped.


def test_ogr_oci_9():

    #######################################################
    # Preclean.

    with gdal.quiet_errors():
        gdaltest.oci_ds.ExecuteSQL("DELLAYER:testsrs2")

    #######################################################
    # Prepare an SRS with an EPSG authority code.
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    #######################################################
    # Create Oracle Layer
    oci_lyr2 = gdaltest.oci_ds.CreateLayer("testsrs2", srs=srs, options=["INDEX=FALSE"])

    #######################################################
    # Now check that the srs for the layer is really the built-in
    # oracle SRS we expect.
    srs2 = oci_lyr2.GetSpatialRef()

    assert (
        srs2.GetAuthorityCode("GEOGCS") == "4326"
    ), "Did not get expected authority code"

    assert (
        srs2.GetAuthorityName("GEOGCS") == "EPSG"
    ), "Did not get expected authority name"

    assert srs2.IsSame(srs)


###############################################################################
# Test handling of specialized Oracle Rectangle Geometries.


def test_ogr_oci_10():

    # Create a test table.
    with gdal.quiet_errors():
        gdaltest.oci_ds.ExecuteSQL("drop table geom_test")

    gdaltest.oci_ds.ExecuteSQL(
        "CREATE TABLE geom_test(ora_fid number primary key, shape sdo_geometry)"
    )

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
1,
SDO_GEOMETRY(
2003, -- two-dimensional polygon
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,3), -- one rectangle (1003 = exterior)
SDO_ORDINATE_ARRAY(1,1, 5,7) -- only 2 points needed to
-- define rectangle (lower left and upper right) with
-- Cartesian-coordinate data
)
)
""")

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 1"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        expected_wkt = "POLYGON ((1 1 0,5 1 0,5 7 0,1 7 0,1 1 0))"

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test handling of specialized Oracle circle Geometries.


def test_ogr_oci_11():

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
4,
SDO_GEOMETRY(
2003, -- two-dimensional polygon
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,4), -- one circle
SDO_ORDINATE_ARRAY(8,7, 10,9, 8,11)
)
)
""")

    expected_wkt = "POLYGON ((10 9,9.989043790736547 9.209056926535308,9.956295201467611 9.415823381635519,9.902113032590307 9.618033988749895,9.827090915285202 9.8134732861516,9.732050807568877 10.0,9.618033988749895 10.175570504584947,9.486289650954788 10.338261212717716,9.338261212717717 10.486289650954788,9.175570504584947 10.618033988749895,9.0 10.732050807568877,8.8134732861516 10.827090915285202,8.618033988749895 10.902113032590307,8.415823381635519 10.956295201467611,8.209056926535308 10.989043790736547,8 11,7.790943073464693 10.989043790736547,7.584176618364482 10.956295201467611,7.381966011250105 10.902113032590307,7.1865267138484 10.827090915285202,7.0 10.732050807568877,6.824429495415054 10.618033988749895,6.661738787282284 10.486289650954788,6.513710349045212 10.338261212717716,6.381966011250105 10.175570504584947,6.267949192431122 10.0,6.172909084714799 9.8134732861516,6.097886967409693 9.618033988749895,6.043704798532389 9.415823381635519,6.010956209263453 9.209056926535308,6 9,6.010956209263453 8.790943073464694,6.043704798532389 8.584176618364483,6.097886967409693 8.381966011250105,6.172909084714798 8.1865267138484,6.267949192431123 8.0,6.381966011250105 7.824429495415054,6.513710349045212 7.661738787282284,6.661738787282284 7.513710349045212,6.824429495415053 7.381966011250105,7 7.267949192431123,7.1865267138484 7.172909084714798,7.381966011250105 7.097886967409693,7.584176618364481 7.043704798532389,7.790943073464693 7.010956209263453,8 7,8.209056926535306 7.010956209263453,8.415823381635518 7.043704798532389,8.618033988749895 7.097886967409693,8.8134732861516 7.172909084714799,9.0 7.267949192431123,9.175570504584947 7.381966011250105,9.338261212717715 7.513710349045211,9.486289650954788 7.661738787282284,9.618033988749895 7.824429495415053,9.732050807568877 8,9.827090915285202 8.1865267138484,9.902113032590307 8.381966011250105,9.956295201467611 8.584176618364481,9.989043790736547 8.790943073464693,10 9))"

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 4"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test handling of specialized Oracle circular arc linestring Geometries.


def test_ogr_oci_12():

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
12,
SDO_GEOMETRY(
2002,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,2,2), -- compound line string
SDO_ORDINATE_ARRAY(0,0, 1,1, 0,2, -1,3, 0,4, 2,2, 0,0 )
)
)
""")

    expected_wkt = "LINESTRING (0.0 0.0,0.104528463267653 0.005478104631727,0.207911690817759 0.021852399266194,0.309016994374947 0.048943483704846,0.4067366430758 0.086454542357399,0.5 0.133974596215561,0.587785252292473 0.190983005625053,0.669130606358858 0.256855174522606,0.743144825477394 0.330869393641142,0.809016994374947 0.412214747707527,0.866025403784439 0.5,0.913545457642601 0.5932633569242,0.951056516295154 0.690983005625053,0.978147600733806 0.792088309182241,0.994521895368273 0.895471536732347,1 1,0.994521895368273 1.104528463267654,0.978147600733806 1.207911690817759,0.951056516295154 1.309016994374948,0.913545457642601 1.4067366430758,0.866025403784439 1.5,0.809016994374947 1.587785252292473,0.743144825477394 1.669130606358858,0.669130606358858 1.743144825477394,0.587785252292473 1.809016994374948,0.5 1.866025403784439,0.4067366430758 1.913545457642601,0.309016994374947 1.951056516295154,0.207911690817759 1.978147600733806,0.104528463267653 1.994521895368273,0 2,-0.104528463267653 2.005478104631727,-0.207911690817759 2.021852399266194,-0.309016994374947 2.048943483704846,-0.4067366430758 2.086454542357399,-0.5 2.133974596215561,-0.587785252292473 2.190983005625053,-0.669130606358858 2.256855174522606,-0.743144825477394 2.330869393641142,-0.809016994374947 2.412214747707527,-0.866025403784439 2.5,-0.913545457642601 2.593263356924199,-0.951056516295154 2.690983005625053,-0.978147600733806 2.792088309182241,-0.994521895368273 2.895471536732346,-1 3,-0.994521895368273 3.104528463267653,-0.978147600733806 3.207911690817759,-0.951056516295154 3.309016994374948,-0.913545457642601 3.4067366430758,-0.866025403784439 3.5,-0.809016994374948 3.587785252292473,-0.743144825477394 3.669130606358858,-0.669130606358858 3.743144825477394,-0.587785252292473 3.809016994374948,-0.5 3.866025403784438,-0.4067366430758 3.913545457642601,-0.309016994374948 3.951056516295154,-0.20791169081776 3.978147600733806,-0.104528463267653 3.994521895368274,0 4,0.209056926535307 3.989043790736547,0.415823381635519 3.956295201467611,0.618033988749895 3.902113032590307,0.8134732861516 3.827090915285202,1.0 3.732050807568877,1.175570504584946 3.618033988749895,1.338261212717717 3.486289650954788,1.486289650954789 3.338261212717717,1.618033988749895 3.175570504584946,1.732050807568877 3.0,1.827090915285202 2.8134732861516,1.902113032590307 2.618033988749895,1.956295201467611 2.415823381635519,1.989043790736547 2.209056926535307,2 2,1.989043790736547 1.790943073464693,1.956295201467611 1.584176618364481,1.902113032590307 1.381966011250105,1.827090915285202 1.1865267138484,1.732050807568877 1.0,1.618033988749895 0.824429495415054,1.486289650954789 0.661738787282284,1.338261212717717 0.513710349045212,1.175570504584946 0.381966011250105,1.0 0.267949192431123,0.8134732861516 0.172909084714798,0.618033988749895 0.097886967409693,0.415823381635519 0.043704798532389,0.209056926535307 0.010956209263453,0.0 0.0)"

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 12"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test handling of specialized Oracle circular arc polygon Geometries.


def test_ogr_oci_13():

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
13,
SDO_GEOMETRY(
2003,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,2), -- compound line string
SDO_ORDINATE_ARRAY(0,0, 1,1, 0,2, -1,3, 0,4, 2,2, 0,0 )
)
)
""")

    expected_wkt = "POLYGON ((0.0 0.0,0.104528463267653 0.005478104631727,0.207911690817759 0.021852399266194,0.309016994374947 0.048943483704846,0.4067366430758 0.086454542357399,0.5 0.133974596215561,0.587785252292473 0.190983005625053,0.669130606358858 0.256855174522606,0.743144825477394 0.330869393641142,0.809016994374947 0.412214747707527,0.866025403784439 0.5,0.913545457642601 0.5932633569242,0.951056516295154 0.690983005625053,0.978147600733806 0.792088309182241,0.994521895368273 0.895471536732347,1 1,0.994521895368273 1.104528463267654,0.978147600733806 1.207911690817759,0.951056516295154 1.309016994374948,0.913545457642601 1.4067366430758,0.866025403784439 1.5,0.809016994374947 1.587785252292473,0.743144825477394 1.669130606358858,0.669130606358858 1.743144825477394,0.587785252292473 1.809016994374948,0.5 1.866025403784439,0.4067366430758 1.913545457642601,0.309016994374947 1.951056516295154,0.207911690817759 1.978147600733806,0.104528463267653 1.994521895368273,0 2,-0.104528463267653 2.005478104631727,-0.207911690817759 2.021852399266194,-0.309016994374947 2.048943483704846,-0.4067366430758 2.086454542357399,-0.5 2.133974596215561,-0.587785252292473 2.190983005625053,-0.669130606358858 2.256855174522606,-0.743144825477394 2.330869393641142,-0.809016994374947 2.412214747707527,-0.866025403784439 2.5,-0.913545457642601 2.593263356924199,-0.951056516295154 2.690983005625053,-0.978147600733806 2.792088309182241,-0.994521895368273 2.895471536732346,-1 3,-0.994521895368273 3.104528463267653,-0.978147600733806 3.207911690817759,-0.951056516295154 3.309016994374948,-0.913545457642601 3.4067366430758,-0.866025403784439 3.5,-0.809016994374948 3.587785252292473,-0.743144825477394 3.669130606358858,-0.669130606358858 3.743144825477394,-0.587785252292473 3.809016994374948,-0.5 3.866025403784438,-0.4067366430758 3.913545457642601,-0.309016994374948 3.951056516295154,-0.20791169081776 3.978147600733806,-0.104528463267653 3.994521895368274,0 4,0.209056926535307 3.989043790736547,0.415823381635519 3.956295201467611,0.618033988749895 3.902113032590307,0.8134732861516 3.827090915285202,1.0 3.732050807568877,1.175570504584946 3.618033988749895,1.338261212717717 3.486289650954788,1.486289650954789 3.338261212717717,1.618033988749895 3.175570504584946,1.732050807568877 3.0,1.827090915285202 2.8134732861516,1.902113032590307 2.618033988749895,1.956295201467611 2.415823381635519,1.989043790736547 2.209056926535307,2 2,1.989043790736547 1.790943073464693,1.956295201467611 1.584176618364481,1.902113032590307 1.381966011250105,1.827090915285202 1.1865267138484,1.732050807568877 1.0,1.618033988749895 0.824429495415054,1.486289650954789 0.661738787282284,1.338261212717717 0.513710349045212,1.175570504584946 0.381966011250105,1.0 0.267949192431123,0.8134732861516 0.172909084714798,0.618033988749895 0.097886967409693,0.415823381635519 0.043704798532389,0.209056926535307 0.010956209263453,0.0 0.0))"

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 13"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test handling of compound linestring.


def test_ogr_oci_14():

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
11,
SDO_GEOMETRY(
2002,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,4,2, 1,2,1, 3,2,2), -- compound line string
SDO_ORDINATE_ARRAY(10,10, 10,14, 6,10, 14,10)
)
)
""")

    expected_wkt = "LINESTRING (10 10,10 14,9.58188614692939 13.9780875814731,9.16835323672896 13.9125904029352,8.76393202250021 13.8042260651806,8.3730534276968 13.6541818305704,8.0 13.4641016151378,7.64885899083011 13.2360679774998,7.32347757456457 12.9725793019096,7.02742069809042 12.6765224254354,6.76393202250021 12.3511410091699,6.53589838486224 12.0,6.3458181694296 11.6269465723032,6.19577393481939 11.2360679774998,6.08740959706478 10.831646763271,6.02191241852691 10.4181138530706,6 10,6.02191241852691 9.58188614692939,6.08740959706478 9.16835323672896,6.19577393481939 8.76393202250021,6.3458181694296 8.3730534276968,6.53589838486225 8.0,6.76393202250021 7.64885899083011,7.02742069809042 7.32347757456457,7.32347757456457 7.02742069809042,7.64885899083011 6.76393202250021,8.0 6.53589838486225,8.3730534276968 6.3458181694296,8.76393202250021 6.19577393481939,9.16835323672896 6.08740959706478,9.58188614692939 6.02191241852691,10 6,10.4181138530706 6.02191241852691,10.831646763271 6.08740959706478,11.2360679774998 6.19577393481939,11.6269465723032 6.3458181694296,12.0 6.53589838486225,12.3511410091699 6.76393202250021,12.6765224254354 7.02742069809042,12.9725793019096 7.32347757456457,13.2360679774998 7.64885899083011,13.4641016151378 8.0,13.6541818305704 8.3730534276968,13.8042260651806 8.76393202250021,13.9125904029352 9.16835323672896,13.9780875814731 9.58188614692939,14 10)"

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 11"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test handling of compound polygon.


def test_ogr_oci_15():

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL("""
INSERT INTO geom_test VALUES(
21,
SDO_GEOMETRY(
2003,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1005,2, 1,1003,1, 3,1003,2),
SDO_ORDINATE_ARRAY(-10,10, 10,10, 0,0, -10,10)
)
)
""")

    expected_wkt = "POLYGON ((-10 10,10 10,9.94521895368273 8.95471536732347,9.78147600733806 7.92088309182241,9.51056516295153 6.90983005625053,9.13545457642601 5.932633569242,8.66025403784439 5.0,8.09016994374947 4.12214747707527,7.43144825477394 3.30869393641142,6.69130606358858 2.56855174522606,5.87785252292473 1.90983005625053,5.0 1.33974596215561,4.067366430758 0.864545423573992,3.09016994374947 0.489434837048465,2.07911690817759 0.218523992661945,1.04528463267653 0.054781046317267,0.0 0.0,-1.04528463267653 0.054781046317267,-2.07911690817759 0.218523992661943,-3.09016994374947 0.489434837048464,-4.067366430758 0.86454542357399,-5 1.33974596215561,-5.87785252292473 1.90983005625053,-6.69130606358858 2.56855174522606,-7.43144825477394 3.30869393641142,-8.09016994374947 4.12214747707527,-8.66025403784439 5.0,-9.13545457642601 5.932633569242,-9.51056516295153 6.90983005625053,-9.78147600733806 7.92088309182241,-9.94521895368273 8.95471536732346,-10 10))"

    with gdaltest.oci_ds.ExecuteSQL(
        "select * from geom_test where ora_fid = 21"
    ) as sql_lyr:

        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, expected_wkt)


###############################################################################
# Test deleting an existing layer.


def test_ogr_oci_16():

    target_index = -1
    lc = gdaltest.oci_ds.GetLayerCount()

    for i in range(lc):
        lyr = gdaltest.oci_ds.GetLayer(i)
        if lyr.GetName() == "TESTSRS2":
            target_index = i
            break

    lyr = None

    assert target_index != -1, "did not find testsrs2 layer"

    result = gdaltest.oci_ds.DeleteLayer(target_index)
    assert result == 0, "DeleteLayer() failed."

    lyr = gdaltest.oci_ds.GetLayerByName("testsrs2")
    assert lyr is None, "apparently failed to remove testsrs2 layer"


###############################################################################
# Test that synctodisk actually sets the layer bounds metadata.


def test_ogr_oci_17():

    gdaltest.oci_ds.ExecuteSQL("DELLAYER:xpoly")

    ######################################################
    # Create Oracle Layer
    gdaltest.oci_lyr = gdaltest.oci_ds.CreateLayer("xpoly")

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        gdaltest.oci_lyr,
        [("AREA", ogr.OFTReal), ("EAS_ID", ogr.OFTInteger), ("PRFEDEA", ogr.OFTString)],
    )

    ######################################################
    # Copy in poly.shp

    shp_ds = ogr.Open("data/poly.shp")
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    gdaltest.poly_feat = []

    for feat in shp_lyr:

        gdaltest.poly_feat.append(feat)

        dst_feat = ogr.Feature(feature_def=gdaltest.oci_lyr.GetLayerDefn())
        dst_feat.SetFrom(feat)
        gdaltest.oci_lyr.CreateFeature(dst_feat)

    ######################################################
    # Create a distinct connection to the same database to monitor the
    # metadata table.

    oci_ds2 = ogr.Open(os.environ["OCI_DSNAME"])

    sql_lyr = oci_ds2.ExecuteSQL(
        "select column_name from user_sdo_geom_metadata where table_name = 'XPOLY'"
    )
    assert sql_lyr.GetFeatureCount() <= 0, "user_sdo_geom_metadata already populated!"

    oci_ds2.ReleaseResultSet(sql_lyr)

    result = gdaltest.oci_ds.SyncToDisk()
    assert result == 0, "SyncToDisk() failed."

    sql_lyr = oci_ds2.ExecuteSQL(
        "select column_name from user_sdo_geom_metadata where table_name = 'XPOLY'"
    )
    assert sql_lyr.GetFeatureCount() != 0, "user_sdo_geom_metadata still not populated!"

    oci_ds2.ReleaseResultSet(sql_lyr)

    oci_ds2 = None


###############################################################################
# Test layer geometry types


def test_ogr_oci_18():

    wkts = [
        "POINT (0 1)",
        "LINESTRING (0 1,2 3)",
        "POLYGON ((0 0,1 0,1 1,0 1,0 0))",
        "MULTIPOINT (0 1)",
        "MULTILINESTRING ((0 1,2 3))",
        "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))",
        "GEOMETRYCOLLECTION (POINT (0 1))",
        "POINT (0 1 2)",
        "LINESTRING (0 1 2,3 4 5)",
        "POLYGON ((0 0 10,1 0 10,1 1 10,0 1 10,0 0 10))",
    ]
    for wkt in wkts:
        g = ogr.CreateGeometryFromWkt(wkt)
        geomtype = g.GetGeometryType()
        strgeomtype = wkt[0 : wkt.find(" ")]
        if geomtype & ogr.wkb25DBit:
            dim = 3
            strgeomtype = strgeomtype + "3"
        else:
            dim = 2
        lyr = gdaltest.oci_ds.CreateLayer(
            "test_%s" % strgeomtype, geom_type=geomtype, options=["DIM=%d" % dim]
        )
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(g)
        lyr.CreateFeature(feat)
        lyr.SyncToDisk()
    lyr = gdaltest.oci_ds.CreateLayer("test_NONE", geom_type=ogr.wkbNone)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    lyr.SyncToDisk()

    oci_ds2 = ogr.Open(os.environ["OCI_DSNAME"])
    for wkt in wkts:
        g = ogr.CreateGeometryFromWkt(wkt)
        strgeomtype = wkt[0 : wkt.find(" ")]
        if strgeomtype == "GEOMETRYCOLLECTION":
            geomtype = ogr.wkbUnknown
        else:
            geomtype = g.GetGeometryType()
        if geomtype & ogr.wkb25DBit:
            strgeomtype = strgeomtype + "3"

        lyr = oci_ds2.GetLayerByName("test_%s" % strgeomtype)
        assert lyr.GetGeomType() == geomtype, wkt
        feat = lyr.GetNextFeature()
        assert feat.GetGeometryRef().ExportToWkt() == wkt

    dsname = os.environ["OCI_DSNAME"]
    if "@" not in dsname:
        dsname = dsname + "@:test_NONE"
    else:
        dsname = dsname + ":test_NONE"

    oci_ds2 = ogr.Open(dsname)
    lyr = oci_ds2.GetLayerByName("test_NONE")
    assert lyr.GetGeomType() == ogr.wkbNone


###############################################################################
# Test date / datetime


def test_ogr_oci_datetime_no_tz():

    lyr = gdaltest.oci_ds.CreateLayer("testdate", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("MYDATE", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("MYDATETIME", ogr.OFTDateTime))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATE", "2015/02/03")
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44.12345")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44.12345+0530")  # Timezone lost
    lyr.CreateFeature(feat)
    lyr.SyncToDisk()

    with gdaltest.oci_ds.ExecuteSQL(
        "SELECT MYDATE, MYDATETIME FROM testdate"
    ) as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTDate
        assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTDateTime
        assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetTZFlag() == ogr.TZFLAG_UNKNOWN
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == "2015/02/03"
        assert f.GetField(1) == "2015/02/03 11:33:44"
        f = sql_lyr.GetNextFeature()
        assert f.GetField(1) == "2015/02/03 11:33:44.123"
        f = sql_lyr.GetNextFeature()
        assert f.GetField(1) == "2015/02/03 11:33:44.123"  # Timezone lost


###############################################################################
# Test date / datetime with time zone


def test_ogr_oci_datetime_with_tz():

    lyr = gdaltest.oci_ds.CreateLayer(
        "testdate_with_tz",
        geom_type=ogr.wkbNone,
        options=["TIMESTAMP_WITH_TIME_ZONE=YES"],
    )
    lyr.CreateField(ogr.FieldDefn("MYDATETIME", ogr.OFTDateTime))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44.567+0530")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44-0530")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44+00")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("MYDATETIME", "2015/02/03 11:33:44")  # UTC assumed...
    lyr.CreateFeature(feat)
    lyr.SyncToDisk()

    with gdaltest.oci_ds.ExecuteSQL(
        "SELECT MYDATETIME FROM testdate_with_tz"
    ) as sql_lyr:
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTDateTime
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetTZFlag() == ogr.TZFLAG_MIXED_TZ
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == "2015/02/03 11:33:44.567+0530"
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == "2015/02/03 11:33:44-0530"
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == "2015/02/03 11:33:44+00"
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == "2015/02/03 11:33:44+00"  # UTC assumed...


###############################################################################
# Test not nullable fields


def test_ogr_oci_20():

    lyr = gdaltest.oci_ds.CreateLayer(
        "ogr_oci_20", geom_type=ogr.wkbPoint, options=["GEOMETRY_NULLABLE=NO", "DIM=2"]
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    ret = lyr.CreateFeature(f)
    f = None
    assert ret == 0

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
    lyr.SyncToDisk()

    # Test with nullable geometry
    lyr = gdaltest.oci_ds.CreateLayer(
        "ogr_oci_20bis", geom_type=ogr.wkbPoint, options=["DIM=2"]
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    ret = lyr.CreateFeature(f)
    f = None
    assert ret == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    f = None
    assert ret == 0
    lyr.SyncToDisk()

    oci_ds2 = ogr.Open(os.environ["OCI_DSNAME"])

    lyr = oci_ds2.GetLayerByName("ogr_oci_20")
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

    lyr = oci_ds2.GetLayerByName("ogr_oci_20bis")
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef() is not None
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef() is None


###############################################################################
# Test default values


def test_ogr_oci_21():

    lyr = gdaltest.oci_ds.CreateLayer(
        "ogr_oci_21", geom_type=ogr.wkbPoint, options=["DIM=2"]
    )

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

    # field_defn = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    # field_defn.SetDefault("CURRENT_DATE")
    # lyr.CreateField(field_defn)

    # field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    # field_defn.SetDefault("CURRENT_TIME")
    # lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_string", "c")
    f.SetField("field_int", 456)
    f.SetField("field_real", 4.56)
    f.SetField("field_datetime", "2015/06/30 12:34:56")
    f.SetField("field_datetime2", "2015/06/30 12:34:56")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    f = None

    # Transition from BoundCopy to UnboundCopy
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    f = None

    lyr.SyncToDisk()

    oci_ds2 = ogr.Open(os.environ["OCI_DSNAME"])

    lyr = oci_ds2.GetLayerByName("ogr_oci_21")
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
    assert f.GetField("field_string") == "c"

    f = lyr.GetNextFeature()
    assert f.GetField("field_string") == "a'b"
    assert f.GetField("field_int") == 123
    assert f.GetField("field_real") == 1.23
    assert f.IsFieldNull("field_nodefault")
    assert f.IsFieldSet("field_datetime")
    assert f.GetField("field_datetime2") == "2015/06/30 12:34:56"
