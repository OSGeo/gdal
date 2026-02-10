#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Basic tests of OGRSpatialReference (OSR) operation, not including
#           support for actual reprojection or use of EPSG tables.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import subprocess
import sys
from threading import Thread

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Create a UTM WGS84 coordinate system and check various items.


def test_osr_basic_1():

    utm_srs = osr.SpatialReference()
    # Southern hemisphere
    utm_srs.SetUTM(11, 0)
    utm_srs.SetWellKnownGeogCS("WGS84")

    assert utm_srs.GetUTMZone() == -11

    # Northern hemisphere
    utm_srs.SetUTM(11)

    assert utm_srs.GetUTMZone() == 11

    parm_list = [
        (osr.SRS_PP_CENTRAL_MERIDIAN, -117.0),
        (osr.SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
        (osr.SRS_PP_SCALE_FACTOR, 0.9996),
        (osr.SRS_PP_FALSE_EASTING, 500000.0),
        (osr.SRS_PP_FALSE_NORTHING, 0.0),
    ]

    for param in parm_list:
        value = utm_srs.GetProjParm(param[0], -1111)
        assert value == pytest.approx(
            param[1], abs=0.00000000000010
        ), "got %g for %s instead of %g." % (value, param[0], param[1])

    auth_list = [("GEOGCS", "4326"), ("DATUM", "6326")]

    for auth in auth_list:
        assert (
            utm_srs.GetAuthorityName(auth[0]) == "EPSG"
        ), "Got authority %s instead of EPSG for %s" % (
            utm_srs.GetAuthorityName(auth[0]),
            auth[0],
        )

        assert (
            str(utm_srs.GetAuthorityCode(auth[0])) == auth[1]
        ), "Got code %s instead of %s for %s" % (
            utm_srs.GetAuthorityName(auth[0]),
            auth[1],
            auth[0],
        )


###############################################################################
# Simple default NAD83 State Plane zone.


def test_osr_basic_2():

    srs = osr.SpatialReference()
    srs.SetStatePlane(403, 1)  # California III NAD83.

    parm_list = [
        (osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
        (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
        (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
        (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
        (osr.SRS_PP_FALSE_EASTING, 2000000.0),
        (osr.SRS_PP_FALSE_NORTHING, 500000.0),
    ]

    for param in parm_list:
        value = srs.GetProjParm(param[0], -1111)
        assert gdaltest.approx_equal(
            param[1], value
        ), "got %.16g for %s instead of %.16g." % (value, param[0], param[1])

    auth_list = [
        ("GEOGCS", "4269"),
        ("DATUM", "6269"),
        ("PROJCS", "26943"),
        ("PROJCS|UNIT", "9001"),
    ]

    for auth in auth_list:
        assert (
            srs.GetAuthorityName(auth[0]) == "EPSG"
        ), "Got authority %s instead of EPSG for %s" % (
            srs.GetAuthorityName(auth[0]),
            auth[0],
        )

        assert (
            str(srs.GetAuthorityCode(auth[0])) == auth[1]
        ), "Got code %s instead of %s for %s" % (
            srs.GetAuthorityCode(auth[0]),
            auth[1],
            auth[0],
        )


###############################################################################
# NAD83 State Plane zone, but overridden to be in Feet.


def test_osr_basic_3():

    srs = osr.SpatialReference()

    # California III NAD83 (feet)
    srs.SetStatePlane(403, 1, "Foot", 0.3048006096012192)
    # print srs.ExportToPrettyWkt()

    parm_list = [
        (osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
        (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
        (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
        (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
        (osr.SRS_PP_FALSE_EASTING, 6561666.666666667),
        (osr.SRS_PP_FALSE_NORTHING, 1640416.666666667),
    ]

    for param in parm_list:
        value = srs.GetProjParm(param[0], -1111)
        assert gdaltest.approx_equal(
            param[1], value
        ), "got %.16g for %s instead of %.16g." % (value, param[0], param[1])

    auth_list = [("GEOGCS", "4269"), ("DATUM", "6269")]

    for auth in auth_list:
        assert (
            srs.GetAuthorityName(auth[0]) == "EPSG"
        ), "Got authority %s instead of EPSG for %s" % (
            srs.GetAuthorityName(auth[0]),
            auth[0],
        )

        assert (
            str(srs.GetAuthorityCode(auth[0])) == auth[1]
        ), "Got code %s instead of %s for %s" % (
            srs.GetAuthorityCode(auth[0]),
            auth[1],
            auth[0],
        )

    assert (
        srs.GetAuthorityName("PROJCS") is None
    ), "Got a PROJCS Authority but we should not"

    assert (
        str(srs.GetAuthorityCode("PROJCS|UNIT")) != "9001"
    ), "Got METER authority code on linear units."

    assert srs.GetLinearUnitsName() == "Foot", "Didn't get Foot linear units"

    assert srs.GetLinearUnits() == pytest.approx(0.3048006096012192, 1e-16)

    assert srs.GetTargetLinearUnits("PROJCS") == pytest.approx(
        0.3048006096012192, 1e-16
    )

    assert srs.GetTargetLinearUnits(None) == pytest.approx(0.3048006096012192, 1e-16)


###############################################################################
# Translate a coordinate system with nad shift into to PROJ.4 and back
# and verify that the TOWGS84 parameters are preserved.


def test_osr_basic_4():

    srs = osr.SpatialReference()
    srs.SetGS(cm=-117.0, fe=100000.0, fn=100000)
    srs.SetLinearUnits("meter", 1)
    srs.SetGeogCS(
        "Test GCS",
        "Test Datum",
        "WGS84",
        osr.SRS_WGS84_SEMIMAJOR,
        osr.SRS_WGS84_INVFLATTENING,
    )

    srs.SetTOWGS84(1, 2, 3)

    assert srs.GetTOWGS84() == (1, 2, 3, 0, 0, 0, 0), "GetTOWGS84() result is wrong."

    proj4 = srs.ExportToProj4()

    srs2 = osr.SpatialReference()
    srs2.ImportFromProj4(proj4)

    assert srs2.GetTOWGS84() == (
        1,
        2,
        3,
        0,
        0,
        0,
        0,
    ), "GetTOWGS84() result is wrong after PROJ.4 conversion."


###############################################################################
# Test URN support for OGC:CRS84.


def test_osr_basic_5():

    wkt_1 = osr.GetUserInputAsWKT("urn:ogc:def:crs:OGC:1.3:CRS84")
    assert 'GEOGCS["WGS 84' in wkt_1
    assert 'AXIS["Longitude",EAST],AXIS["Latitude",NORTH]' in wkt_1
    assert "4326" not in wkt_1

    wkt_2 = osr.GetUserInputAsWKT("WGS84")
    assert 'GEOGCS["WGS 84' in wkt_2
    assert 'AXIS["Latitude",NORTH],AXIS["Longitude",EAST]' in wkt_2
    assert "4326" in wkt_2


###############################################################################
# Test URN support for EPSG


def test_osr_basic_6():

    # Without version
    wkt_1 = osr.GetUserInputAsWKT("urn:x-ogc:def:crs:EPSG::4326")
    assert not (
        wkt_1.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1
        or wkt_1.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1
    ), "EPSG:4326 urn lookup not as expected."

    # With a version
    wkt_2 = osr.GetUserInputAsWKT("urn:x-ogc:def:crs:EPSG:6.6:4326")
    if (
        wkt_2.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1
        or wkt_2.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1
    ):
        print(wkt_1)
        pytest.fail("EPSG:4326 urn lookup not as expected.")

    # Without version, but with no repeated :. Probably illegal from my understanding
    # of http://www.opengeospatial.org/ogcUrnPolicy, but found quite often in the wild
    # especially in content returned by GeoServer
    wkt_2 = osr.GetUserInputAsWKT("urn:x-ogc:def:crs:EPSG:4326")
    if (
        wkt_2.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1
        or wkt_2.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1
    ):
        print(wkt_1)
        pytest.fail("EPSG:4326 urn lookup not as expected.")


###############################################################################
# Test URN support for auto projection.


def test_osr_basic_7():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("urn:ogc:def:crs:OGC::AUTO42001:-117:33")

    srs_ref = osr.SpatialReference()
    srs_ref.ImportFromEPSG(32611)
    assert srs.IsSame(srs_ref)


###############################################################################
# Test the SetLinearUnitsAndUpdateParameters() method.


def test_osr_basic_8():

    srs = osr.SpatialReference()

    srs.SetFromUserInput("+proj=tmerc +x_0=1000 +datum=WGS84 +units=m")
    srs.SetLinearUnits("Foot", 0.3048)
    fe = srs.GetProjParm("false_easting")

    assert fe == 1000.0, "false easting was unexpectedly updated."

    srs.SetFromUserInput("+proj=tmerc +x_0=1000 +datum=WGS84 +units=m")
    srs.SetLinearUnitsAndUpdateParameters("Foot", 0.3048)
    fe = srs.GetProjParm("false_easting")

    assert fe != 1000.0, "false easting was unexpectedly not updated."

    assert fe == pytest.approx(3280.840, abs=0.01), "wrong updated false easting value."


###############################################################################
# Test the Validate() method with a WKT string with leading space.


def test_osr_basic_9():

    srs = osr.SpatialReference()
    assert (
        srs.SetFromUserInput(
            ' PROJCS["unnamed",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",6378137,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_2SP"],PARAMETER["standard_parallel_1",0],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1],EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs"]]'
        )
        == ogr.OGRERR_NONE
    )
    assert srs.Validate() == 0


###############################################################################
# Test the Validate() method on PROJCS with AXIS definition (#2739)


def test_osr_basic_10():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""PROJCS["NAD83(CSRS98) / UTM zone 20N (deprecated)",
    GEOGCS["NAD83(CSRS98)",
        DATUM["NAD83_Canadian_Spatial_Reference_System",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6140"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4140"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-63],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    AUTHORITY["EPSG","2038"]]""")

    assert srs.Validate() == 0


###############################################################################
# Test the IsSame() method (and the IsSameGeogCS() method through that)


def test_osr_basic_11():

    srs1 = osr.SpatialReference()
    srs1.SetFromUserInput("""PROJCS["NAD83(CSRS98) / UTM zone 20N (deprecated)",
    GEOGCS["NAD83(CSRS98)",
        DATUM["NAD83_Canadian_Spatial_Reference_System",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6140"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4140"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-63],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","2038"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    srs2 = osr.SpatialReference()
    srs2.SetFromUserInput("""PROJCS["NAD83(CSRS98) / UTM zone 20N (deprecated)",
    GEOGCS["NAD83(CSRS98)",
        DATUM["NAD83_Canadian_Spatial_Reference_System",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6140"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4140"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["central_meridian",-63],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","2038"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    assert srs1.IsSame(srs2)


###############################################################################
# Test URN support for OGC:CRS84.


def test_osr_basic_12():

    wkt_1 = osr.GetUserInputAsWKT("CRS:84")
    wkt_2 = osr.GetUserInputAsWKT("WGS84")
    assert (
        wkt_1
        == 'GEOGCS["WGS 84 (CRS84)",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Longitude",EAST],AXIS["Latitude",NORTH],AUTHORITY["OGC","CRS84"]]'
    )
    assert (
        wkt_2
        == 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'
    )


###############################################################################
# Test GEOCCS lookup in supporting data files.


def test_osr_basic_13():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4328)
    with gdaltest.config_option("OSR_USE_NON_DEPRECATED", "NO"):
        srs.ImportFromEPSG(4328)

    expected_wkt = 'GEOCCS["WGS 84 (geocentric) (deprecated)",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Geocentric X",OTHER],AXIS["Geocentric Y",OTHER],AXIS["Geocentric Z",NORTH],AUTHORITY["EPSG","4328"]]'
    wkt = srs.ExportToWkt()

    assert wkt == expected_wkt, "did not get expected GEOCCS WKT."

    assert srs.IsGeocentric(), "srs not recognised as geocentric."

    assert srs.Validate() == 0, "epsg geoccs import does not validate!"


###############################################################################
# Manually setup a simple geocentric/wgs84 srs.


def test_osr_basic_14():

    srs = osr.SpatialReference()
    srs.SetGeocCS("My Geocentric")
    srs.SetWellKnownGeogCS("WGS84")
    srs.SetLinearUnits("meter", 1.0)

    expected_wkt = 'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1],AXIS["Geocentric X",OTHER],AXIS["Geocentric Y",OTHER],AXIS["Geocentric Z",NORTH]]'
    wkt = srs.ExportToWkt()

    assert wkt == expected_wkt, "did not get expected GEOCCS WKT."

    assert srs.IsGeocentric(), "srs not recognised as geocentric."

    assert srs.Validate() == 0, "geocentric srs not recognised as valid."


###############################################################################
# Test validation and fixup methods.


def test_osr_basic_15():

    wkt = """GEOCCS["WGS 84 (geocentric)",
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    UNIT["metre",1],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]"""

    srs = osr.SpatialReference()
    srs.SetFromUserInput(wkt)

    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
# Test OSRSetGeocCS()


def test_osr_basic_16():

    # Nominal test : change citation of a GEOCCS
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOCCS["WGS 84 (geocentric)",
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    UNIT["metre",1],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]""")
    srs.SetGeocCS("a")
    expect_wkt = """GEOCCS["a",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["metre",1],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",NORTH],
    AUTHORITY["EPSG","4328"]]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print("Got:%s" % wkt)
        print("Expected:%s" % expect_wkt)
        pytest.fail("Did not get expected result.")

    # Build GEOCCS from a valid GEOGCS
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetGeocCS("a")
    expect_wkt = """GEOCCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",NORTH]]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print("Got:%s" % wkt)
        print("Expected:%s" % expect_wkt)
        pytest.fail("Did not get expected result.")

    # Error expected. Cannot work on a PROJCS
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    with pytest.raises(Exception):
        srs.SetGeocCS("a")

    # Limit test : build GEOCCS from an invalid GEOGCS
    srs = osr.SpatialReference()
    with pytest.raises(Exception):
        srs.SetFromUserInput("""GEOGCS["foo"]""")


###############################################################################
# Test OGC URL support


def test_osr_basic_17():

    wkt_1 = osr.GetUserInputAsWKT("urn:ogc:def:crs:EPSG::4326")
    wkt_2 = osr.GetUserInputAsWKT("http://www.opengis.net/def/crs/EPSG/0/4326")
    assert wkt_1 == wkt_2, "CRS URL parsing not as expected."


###############################################################################
# Test OGC URL support for compound CRS


def test_osr_basic_18():

    wkt = osr.GetUserInputAsWKT(
        "http://www.opengis.net/def/crs-compound?1=http://www.opengis.net/def/crs/EPSG/0/4326&2=http://www.opengis.net/def/crs/EPSG/0/3855"
    )
    assert wkt.startswith("COMPD_CS"), "CRS URL parsing not as expected."


###############################################################################
# Test well known GCS names against their corresponding EPSG definitions (#6080)


def test_osr_basic_19():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4326)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS72")

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4322)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("NAD27")

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4267)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("NAD83")

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4269)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()


###############################################################################
# Test GetAxisName() and GetAxisOrientation() and GetAngularUnitsName()


def test_osr_basic_20():

    sr = osr.SpatialReference()
    sr.ImportFromEPSGA(4326)

    assert sr.GetAxesCount() == 2

    assert sr.GetAxisName(None, 0) == "Geodetic latitude"

    assert sr.GetAxisOrientation(None, 0) == osr.OAO_North

    assert sr.GetAxisName("GEOGCS", 1) == "Geodetic longitude"

    assert sr.GetAxisOrientation("GEOGCS", 1) == osr.OAO_East

    assert sr.GetAngularUnitsName() == "degree"

    sr = osr.SpatialReference()
    sr.SetFromUserInput("EPSG:4326+5773")

    assert sr.GetAxisName(None, 0) == "Geodetic latitude"

    assert sr.GetAxisOrientation(None, 0) == osr.OAO_North

    assert sr.GetAxisName(None, 1) == "Geodetic longitude"

    assert sr.GetAxisOrientation(None, 1) == osr.OAO_East

    assert sr.GetAxisName(None, 2) == "Gravity-related height"

    assert sr.GetAxisOrientation(None, 2) == osr.OAO_Up


###############################################################################
# Test IsSame() with equivalent forms of Mercator_1SP and Mercator_2SP


def test_osr_basic_21():

    wkt1 = """PROJCS["unnamed",
    GEOGCS["Segara (Jakarta)",
        DATUM["Gunung_Segara_Jakarta",
            SPHEROID["Bessel 1841",6377397.155,299.1528128]],
        PRIMEM["Jakarta",106.8077194444444],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["central_meridian",110],
    PARAMETER["false_easting",3900000],
    PARAMETER["false_northing",900000],
    PARAMETER["standard_parallel_1",4.45405154589751],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""

    wkt2 = """PROJCS["unnamed",
    GEOGCS["Segara (Jakarta)",
        DATUM["Gunung_Segara_Jakarta",
            SPHEROID["Bessel 1841",6377397.155,299.1528128]],
        PRIMEM["Jakarta",106.8077194444444],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",110],
    PARAMETER["scale_factor",0.997],
    PARAMETER["false_easting",3900000],
    PARAMETER["false_northing",900000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""

    wkt2_not_equivalent = """PROJCS["unnamed",
    GEOGCS["Segara (Jakarta)",
        DATUM["Gunung_Segara_Jakarta",
            SPHEROID["Bessel 1841",6377397.155,299.1528128]],
        PRIMEM["Jakarta",106.8077194444444],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",110],
    PARAMETER["scale_factor",0.998],
    PARAMETER["false_easting",3900000],
    PARAMETER["false_northing",900000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""

    sr1 = osr.SpatialReference()
    sr1.ImportFromWkt(wkt1)
    sr2 = osr.SpatialReference()
    sr2.ImportFromWkt(wkt2)

    assert sr1.IsSame(sr2) != 0

    assert sr2.IsSame(sr1) != 0

    sr2_not_equivalent = osr.SpatialReference()
    sr2_not_equivalent.ImportFromWkt(wkt2_not_equivalent)

    assert sr1.IsSame(sr2_not_equivalent) != 1


###############################################################################
# Test LCC_2SP -> LCC_1SP -> LCC_2SP


def test_osr_basic_22():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",49],
    PARAMETER["standard_parallel_2",44],
    PARAMETER["latitude_of_origin",46.5],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["X",EAST],
    AXIS["Y",NORTH],
    AUTHORITY["EPSG","2154"]]""")

    sr2 = sr.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP)
    expected_sr2_wkt = """PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.5194302239868],
    PARAMETER["central_meridian",3],
    PARAMETER["scale_factor",0.9990510286374693],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6602157.83881033],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr2 = osr.SpatialReference()
    expected_sr2.ImportFromWkt(expected_sr2_wkt)

    assert sr2.IsSame(expected_sr2) != 0

    # Back to LCC_2SP
    sr3 = sr2.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
    assert sr3.IsSame(sr) != 0

    # Particular case of LCC_2SP with phi0=phi1=phi2
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.5],
    PARAMETER["standard_parallel_2",46.5],
    PARAMETER["latitude_of_origin",46.5],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    sr2 = sr.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP)
    expected_sr2_wkt = """PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.5],
    PARAMETER["central_meridian",3],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr2 = osr.SpatialReference()
    expected_sr2.ImportFromWkt(expected_sr2_wkt)

    assert sr2.IsSame(expected_sr2) != 0

    sr3 = sr2.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
    assert sr3.IsSame(sr) != 0

    # Particular case of LCC_2SP with phi0 != phi1 and phi1=phi2
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    sr2 = sr.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP)
    expected_sr2_wkt = """PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.4567],
    PARAMETER["central_meridian",3],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6637093.292952879],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr2 = osr.SpatialReference()
    expected_sr2.ImportFromWkt(expected_sr2_wkt)

    assert sr2.IsSame(expected_sr2) != 0

    sr3 = sr2.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
    expected_sr3_wkt = """PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6171"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4171"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",46.4567],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6637093.292952879],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr3 = osr.SpatialReference()
    expected_sr3.ImportFromWkt(expected_sr3_wkt)
    assert sr3.IsSame(expected_sr3) != 0


###############################################################################
# Test LCC_1SP -> LCC_2SP -> LCC_1SP


def test_osr_basic_23():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269,
                AUTHORITY["EPSG","7011"]],
            TOWGS84[-168,-60,320,0,0,0,0],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.33722917,
            AUTHORITY["EPSG","8903"]],
        UNIT["grad",0.01570796326794897,
            AUTHORITY["EPSG","9105"]],
        AUTHORITY["EPSG","4807"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.85],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["X",EAST],
    AXIS["Y",NORTH],
    AUTHORITY["EPSG","27584"]]""")

    sr2 = sr.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
    expected_sr2_wkt = """PROJCS["unnamed",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269,
                AUTHORITY["EPSG","7011"]],
            TOWGS84[-168,-60,320,0,0,0,0],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.33722917,
            AUTHORITY["EPSG","8903"]],
        UNIT["grad",0.01570796326794897,
            AUTHORITY["EPSG","9105"]],
        AUTHORITY["EPSG","4807"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",47.51962607709162],
    PARAMETER["standard_parallel_2",46.17820871246364],
    PARAMETER["latitude_of_origin",46.85],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr2 = osr.SpatialReference()
    expected_sr2.ImportFromWkt(expected_sr2_wkt)

    assert sr2.IsSame(expected_sr2) != 0

    # Back to LCC_2SP
    sr3 = sr2.ConvertToOtherProjection(osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP)
    assert sr3.IsSame(sr) != 0


###############################################################################
# Test Mercator_1SP -> Mercator_2SP -> Mercator_1SP


def test_osr_basic_24():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563,
                    AUTHORITY["EPSG","7030"]],
                AUTHORITY["EPSG","6326"]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433,
                AUTHORITY["EPSG","9122"]],
            AUTHORITY["EPSG","4326"]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")

    sr2 = sr.ConvertToOtherProjection(osr.SRS_PT_MERCATOR_2SP)
    expected_sr2_wkt = """PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",60.08325228676391],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr2 = osr.SpatialReference()
    expected_sr2.ImportFromWkt(expected_sr2_wkt)

    assert sr2.IsSame(expected_sr2) != 0

    # Back to LCC_2SP
    sr3 = sr2.ConvertToOtherProjection(osr.SRS_PT_MERCATOR_1SP)
    assert sr3.IsSame(sr) != 0


###############################################################################
# Test corner cases of ConvertToOtherProjection()


def test_osr_basic_25():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("""GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433]]""")
    sr2 = sr.ConvertToOtherProjection("Mercator_1SP")
    assert sr2 is None

    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")

    sr2 = sr.ConvertToOtherProjection(None)
    assert sr2 is None

    sr2 = sr.ConvertToOtherProjection("foo")
    assert sr2 is None

    sr2 = sr.ConvertToOtherProjection("Mercator_1SP")
    assert sr2.IsSame(sr) != 0

    # Mercator_1SP -> Mercator_2SP: Negative scale factor
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",-0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Mercator_2SP") is None

    # Mercator_1SP -> Mercator_2SP: Invalid eccentricity
    sr = osr.SpatialReference()
    with gdaltest.disable_exceptions():
        sr.SetFromUserInput("""PROJCS["unnamed",
            GEOGCS["WGS 84",
                DATUM["WGS_1984",
                    SPHEROID["WGS 84",6378137,0.1]],
                PRIMEM["Greenwich",0,
                    AUTHORITY["EPSG","8901"]],
                UNIT["degree",0.0174532925199433]],
            PROJECTION["Mercator_1SP"],
            PARAMETER["central_meridian",0],
            PARAMETER["scale_factor",0.5],
            PARAMETER["false_easting",0],
            PARAMETER["false_northing",0],
            UNIT["metre",1],
            AXIS["Easting",EAST],
            AXIS["Northing",NORTH]]""")
        assert sr.ConvertToOtherProjection("Mercator_2SP") is None

    # Mercator_2SP -> Mercator_1SP: Invalid standard_parallel_1
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",100],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Mercator_1SP") is None

    # Mercator_2SP -> Mercator_1SP: Invalid eccentricity
    sr = osr.SpatialReference()
    with gdaltest.disable_exceptions():
        sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,0.1]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_2SP"],
        PARAMETER["standard_parallel_1",60],
        PARAMETER["central_meridian",0],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")
        assert sr.ConvertToOtherProjection("Mercator_1SP") is None

    # LCC_1SP -> LCC_2SP: Negative scale factor
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269]],
        PRIMEM["Paris",2.33722917],
        UNIT["grad",0.01570796326794897]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.85],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",-0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_2SP") is None

    # LCC_1SP -> LCC_2SP: Invalid eccentricity
    sr = osr.SpatialReference()
    with gdaltest.disable_exceptions():
        sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["NTF (Paris)",
            DATUM["Nouvelle_Triangulation_Francaise_Paris",
                SPHEROID["Clarke 1880 (IGN)",6378249.2,0.1]],
            PRIMEM["Paris",2.33722917],
            UNIT["grad",0.01570796326794897]],
        PROJECTION["Lambert_Conformal_Conic_1SP"],
        PARAMETER["latitude_of_origin",46.85],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.99994471],
        PARAMETER["false_easting",234.358],
        PARAMETER["false_northing",4185861.369],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_2SP") is None

    # LCC_1SP -> LCC_2SP: Invalid latitude_of_origin
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269]],
        PRIMEM["Paris",2.33722917],
        UNIT["grad",0.01570796326794897]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",200],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_2SP") is None

    # LCC_1SP -> LCC_2SP: latitude_of_origin == 0
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269]],
        PRIMEM["Paris",2.33722917],
        UNIT["grad",0.01570796326794897]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_2SP") is None

    # LCC_2SP -> LCC_1SP : Invalid standard_parallel_1
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",246.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None

    # LCC_2SP -> LCC_1SP : Invalid standard_parallel_2
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",246.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None

    # LCC_2SP -> LCC_1SP : Invalid latitude_of_origin
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",246.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None

    # LCC_2SP -> LCC_1SP : abs(stdp1) == abs(stdp2)
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",1],
    PARAMETER["standard_parallel_2",-1],
    PARAMETER["latitude_of_origin",10],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None

    # LCC_2SP -> LCC_1SP : stdp1 ~= stdp2 ~= 0
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",.0000000000000001],
    PARAMETER["standard_parallel_2",.0000000000000002],
    PARAMETER["latitude_of_origin",10],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000],
    UNIT["metre",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")
    with gdaltest.disable_exceptions():
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None

    # LCC_2SP -> LCC_1SP : Invalid eccentricity
    with gdaltest.disable_exceptions():
        sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["RGF93",
            DATUM["Reseau_Geodesique_Francais_1993",
                SPHEROID["GRS 1980",6378137,0.1]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Lambert_Conformal_Conic_2SP"],
        PARAMETER["standard_parallel_1",46.4567],
        PARAMETER["standard_parallel_2",46.4567],
        PARAMETER["latitude_of_origin",46.123],
        PARAMETER["central_meridian",3],
        PARAMETER["false_easting",700000],
        PARAMETER["false_northing",6600000],
        UNIT["metre",1],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]""")
        assert sr.ConvertToOtherProjection("Lambert_Conformal_Conic_1SP") is None


###############################################################################
# EPSG 4979, but overridden to be in Feet.


def test_osr_basic_26():

    srs = osr.SpatialReference()

    srs.ImportFromEPSG(4979)

    srs.SetLinearUnits("Foot", 0.3048)

    assert srs.GetLinearUnits() == 0.3048


###############################################################################
# Test corner cases of osr.SetGeocCS()


def test_osr_basic_setgeogcs():

    sr = osr.SpatialReference()
    sr.SetGeogCS(None, None, None, 1, 2, None, 0, None, 0)
    assert (
        sr.ExportToWkt()
        == 'GEOGCS["unnamed",DATUM["unnamed",SPHEROID["unnamed",1,2]],PRIMEM["Reference meridian",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    )

    sr.SetGeogCS("a", "b", "c", 1, 2, "d", 3, "e", 4)
    assert (
        sr.ExportToWkt()
        == 'GEOGCS["a",DATUM["b",SPHEROID["c",1,2]],PRIMEM["d",3],UNIT["e",4],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    )

    sr.SetUTM(31)
    sr.SetGeogCS(None, None, None, 1, 2, None, 0, None, 0)
    assert (
        sr.ExportToWkt()
        == 'PROJCS["unnamed",GEOGCS["unnamed",DATUM["unnamed",SPHEROID["unnamed",1,2]],PRIMEM["Reference meridian",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    )


###############################################################################
# Test other authorities than EPSG, e.g. IGNF:XXXX
#


def test_osr_basic_set_from_user_input_IGNF():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("IGNF:LAMB93") == 0

    assert (
        srs.GetAuthorityName(None) == "IGNF" and srs.GetAuthorityCode(None) == "LAMB93"
    )


def test_osr_basic_set_from_user_input_IGNF_non_existing_code():

    srs = osr.SpatialReference()
    with pytest.raises(Exception):
        srs.SetFromUserInput("IGNF:non_existing_code")


def test_osr_basic_set_from_user_input_non_existing_authority():

    srs = osr.SpatialReference()
    with pytest.raises(Exception):
        srs.SetFromUserInput("non_existing_auth:1234")


###############################################################################
# Test IAU: CRS


@pytest.mark.require_proj(8, 2)
def test_osr_basic_set_from_user_input_IAU():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("IAU:49910") == ogr.OGRERR_NONE

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("IAU:2015:49910") == ogr.OGRERR_NONE

    srs = osr.SpatialReference()
    with pytest.raises(Exception):
        srs.SetFromUserInput("IAU:0000:49910")


def test_osr_basic_set_from_user_input_GEODCRS():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""GEODCRS["WGS 84",
    DATUM["World Geodetic System 1984",
        ELLIPSOID["WGS 84",6378137,298.257223563,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]],
    AREA["World"],
    BBOX[-90,-180,90,180],
    ID["EPSG",4326]]""") == 0
    assert srs.Validate() == 0


def test_osr_basic_set_from_user_input_GEOGCRS():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""GEOGCRS["WGS 84",
    DATUM["World Geodetic System 1984",
        ELLIPSOID["WGS 84",6378137,298.257223563,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]],
    USAGE[
        SCOPE["unknown"],
        AREA["World"],
        BBOX[-90,-180,90,180]],
    ID["EPSG",4326]]""") == 0
    assert srs.Validate() == 0


def test_osr_basic_set_from_user_input_PROJCRS():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""PROJCRS["WGS 84 / UTM zone 31N",
    BASEGEODCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]]],
    CONVERSION["UTM zone 31N",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",3,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8807]]],
    CS[Cartesian,2],
        AXIS["(E)",east,
            ORDER[1],
            LENGTHUNIT["metre",1]],
        AXIS["(N)",north,
            ORDER[2],
            LENGTHUNIT["metre",1]],
    AREA["World - N hemisphere - 0°E to 6°E - by country"],
    BBOX[0,0,84,6],
    ID["EPSG",32631]]""") == 0
    assert srs.Validate() == 0


def test_osr_basic_set_from_user_input_COMPOUNDCRS():

    srs = osr.SpatialReference()
    assert (
        srs.SetFromUserInput(
            """COMPOUNDCRS["KKJ / Finland Uniform Coordinate System + N60 height",
    PROJCRS["KKJ / Finland Uniform Coordinate System",
        BASEGEODCRS["KKJ",
            DATUM["Kartastokoordinaattijarjestelma (1966)",
                ELLIPSOID["International 1924",6378388,297,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,
                ANGLEUNIT["degree",0.0174532925199433]]],
        CONVERSION["Finland Uniform Coordinate System",
            METHOD["Transverse Mercator",
                ID["EPSG",9807]],
            PARAMETER["Latitude of natural origin",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",27,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",1,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",3500000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",0,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]]],
        CS[Cartesian,2],
            AXIS["northing (X)",north,
                ORDER[1],
                LENGTHUNIT["metre",1]],
            AXIS["easting (Y)",east,
                ORDER[2],
                LENGTHUNIT["metre",1]]],
    VERTCRS["N60 height",
        VDATUM["Helsinki 1960"],
        CS[vertical,1],
            AXIS["gravity-related height (H)",up,
                LENGTHUNIT["metre",1]]],
    AREA["Finland - onshore"],
    BBOX[59.75,19.24,70.09,31.59],
    ID["EPSG",3901]]"""
        )
        == 0
    )
    assert srs.Validate() == 0


def test_osr_basic_export_to_sfsql():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")
    assert (
        sr.ExportToWkt(["FORMAT=SFSQL"])
        == 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'
    )


def test_osr_basic_export_to_wkt1_esri():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")
    assert (
        sr.ExportToWkt(["FORMAT=WKT1_ESRI"])
        == 'GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]]'
    )


def test_osr_basic_export_to_wkt1_gdal():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")
    assert (
        sr.ExportToWkt(["FORMAT=WKT1_GDAL"])
        == 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'
    )


def test_osr_basic_export_to_wkt2_2015():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")
    assert (
        sr.ExportToWkt(["FORMAT=WKT2_2015"])
        == 'GEODCRS["WGS 84",DATUM["World Geodetic System 1984",ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],CS[ellipsoidal,2],AXIS["geodetic latitude (Lat)",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],AXIS["geodetic longitude (Lon)",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",4326]]'
    )


def test_osr_basic_export_to_wkt2_2018():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS("WGS84")
    assert (
        sr.ExportToWkt(["FORMAT=WKT2_2018"])
        == 'GEOGCRS["WGS 84",DATUM["World Geodetic System 1984",ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],CS[ellipsoidal,2],AXIS["geodetic latitude (Lat)",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],AXIS["geodetic longitude (Lon)",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",4326]]'
    )


def test_osr_get_name():

    sr = osr.SpatialReference()
    assert sr.GetName() is None
    sr.SetWellKnownGeogCS("WGS84")
    assert sr.GetName() == "WGS 84"


def test_SetPROJSearchPath():

    # Do the test in a new thread, so that the EPSG code cache which is thread
    # locale is not used, and we can effectively test that the new search path
    # is used
    def threaded_function(arg):
        sr = osr.SpatialReference()
        try:
            sr.ImportFromEPSG(32631)
            arg[0] = True
        except Exception:
            arg[0] = False

    backup_search_paths = osr.GetPROJSearchPaths()
    try:
        arg = [-1]

        thread = Thread(target=threaded_function, args=(arg,))
        thread.start()
        thread.join()
        assert arg[0] == True

        osr.SetPROJSearchPath("/i_do/not/exist")

        thread = Thread(target=threaded_function, args=(arg,))
        thread.start()
        thread.join()
        assert arg[0] == False
    finally:
        osr.SetPROJSearchPaths(backup_search_paths)

    sr = osr.SpatialReference()
    assert sr.ImportFromEPSG(32631) == 0


def test_Set_PROJ_DATA_config_option():

    # OSRSetPROJSearchPaths() is only taken into priority over other methods
    # starting with PROJ >= 6.1

    # Do the test in a new thread, so that the EPSG code cache which is thread
    # locale is not used, and we can effectively test that the new search path
    # is used
    def threaded_function(arg):
        sr = osr.SpatialReference()
        try:
            sr.ImportFromEPSG(32631)
            arg[0] = True
        except Exception:
            arg[0] = False

    backup_search_paths = osr.GetPROJSearchPaths()
    # conftest.py set 2 paths: autotest/gcore/tmp/proj_db_tmpdir and autotest/proj_grids
    assert len(backup_search_paths) == 2
    try:
        gdal.SetConfigOption("PROJ_DATA", "/i_do/not/exist")
        arg = [-1]
        thread = Thread(target=threaded_function, args=(arg,))
        thread.start()
        thread.join()
        assert arg[0] == False

        gdal.SetConfigOption("PROJ_DATA", backup_search_paths[0])
        arg = [-1]
        thread = Thread(target=threaded_function, args=(arg,))
        thread.start()
        thread.join()
        assert arg[0] == True

        gdal.SetConfigOption("PROJ_DATA", None)
    finally:
        osr.SetPROJSearchPaths(backup_search_paths)

    sr = osr.SpatialReference()
    assert sr.ImportFromEPSG(32631) == 0


###############################################################################


@pytest.mark.skipif(
    gdaltest.is_travis_branch("sanitize"), reason="fails on sanitize for unknown reason"
)
def test_Set_PROJ_DATA_config_option_sub_proccess_config_option_ok():

    backup_search_paths = osr.GetPROJSearchPaths()
    # conftest.py set 2 paths: autotest/gcore/tmp/proj_db_tmpdir and autotest/proj_grids
    assert len(backup_search_paths) == 2
    subprocess.check_call(
        [
            sys.executable,
            "osr_basic_subprocess.py",
            "config_option_ok",
            backup_search_paths[0],
        ]
    )


###############################################################################


@pytest.mark.skipif(
    gdaltest.is_travis_branch("sanitize"), reason="fails on sanitize for unknown reason"
)
def test_Set_PROJ_DATA_config_option_sub_proccess_config_option_ko():

    backup_search_paths = osr.GetPROJSearchPaths()
    # conftest.py set 2 paths: autotest/gcore/tmp/proj_db_tmpdir and autotest/proj_grids
    assert len(backup_search_paths) == 2
    subprocess.check_call(
        [
            sys.executable,
            "osr_basic_subprocess.py",
            "config_option_ko",
            "/i_do/not/exist",
        ]
    )


def test_osr_import_projjson():

    sr = osr.SpatialReference()
    projjson = '{"$schema":"https://proj.org/schemas/v0.1/projjson.schema.json","type":"GeographicCRS","name":"WGS 84","datum":{"type":"GeodeticReferenceFrame","name":"World Geodetic System 1984","ellipsoid":{"name":"WGS 84","semi_major_axis":6378137,"inverse_flattening":298.257223563}},"coordinate_system":{"subtype":"ellipsoidal","axis":[{"name":"Geodetic latitude","abbreviation":"Lat","direction":"north","unit":"degree"},{"name":"Geodetic longitude","abbreviation":"Lon","direction":"east","unit":"degree"}]},"area":"World","bbox":{"south_latitude":-90,"west_longitude":-180,"north_latitude":90,"east_longitude":180},"id":{"authority":"EPSG","code":4326}}'
    ret = sr.SetFromUserInput(projjson)
    assert ret == 0

    broken_projjson = projjson[0:-10]
    with pytest.raises(Exception):
        sr.SetFromUserInput(broken_projjson)


def test_osr_export_projjson():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    assert sr.ExportToPROJJSON() != ""


def test_osr_promote_to_3D():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    assert sr.PromoteTo3D() == 0
    assert sr.GetAuthorityCode(None) == "4979"

    assert sr.DemoteTo2D() == 0
    assert sr.GetAuthorityCode(None) == "4326"


def test_osr_strip_vertical():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3901)  # "KKJ / Finland Uniform Coordinate System + N60 height"
    assert sr.IsCompound()

    assert sr.StripVertical() == 0
    assert not sr.IsCompound()
    assert (
        sr.GetAuthorityCode(None) == "2393"
    )  # KKJ / Finland Uniform Coordinate System


def test_osr_SetVerticalPerspective():

    sr = osr.SpatialReference()
    sr.SetVerticalPerspective(1, 2, 0, 3, 4, 5)
    assert (
        sr.ExportToProj4()
        == "+proj=nsper +lat_0=1 +lon_0=2 +h=3 +x_0=4 +y_0=5 +datum=WGS84 +units=m +no_defs"
    )
    if osr.GetPROJVersionMajor() > 6 or osr.GetPROJVersionMinor() >= 3:
        assert sr.GetAttrValue("PROJECTION") in "Vertical Perspective"
        assert sr.GetNormProjParm("Longitude of topocentric origin") == 2


def test_osr_create_in_one_thread_destroy_in_other():
    def threaded_function(arg):
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(32631)
        arg[0] = sr

    arg = [None]

    thread = Thread(target=threaded_function, args=(arg,))
    thread.start()
    thread.join()
    assert arg[0]
    del arg[0]


def test_osr_SpatialReference_invalid_wkt_in_constructor():

    with pytest.raises(RuntimeError):
        osr.SpatialReference("invalid")


###############################################################################
# Check GetUTMZone() on a Projected 3D CRS


def test_osr_GetUTMZone_Projected3D():

    utm_srs = osr.SpatialReference()
    # Southern hemisphere
    utm_srs.SetUTM(11, 0)
    utm_srs.SetWellKnownGeogCS("WGS84")

    assert utm_srs.GetUTMZone() == -11

    utm_srs.PromoteTo3D()

    assert utm_srs.GetUTMZone() == -11


###############################################################################
# Check GetProjParm() on a Projected 3D CRS


def test_osr_GetProjParm_Projected3D():

    utm_srs = osr.SpatialReference()
    # Southern hemisphere
    utm_srs.SetUTM(11, 0)
    utm_srs.SetWellKnownGeogCS("WGS84")
    utm_srs.PromoteTo3D()

    parm_list = [
        (osr.SRS_PP_CENTRAL_MERIDIAN, -117.0),
        (osr.SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
        (osr.SRS_PP_SCALE_FACTOR, 0.9996),
        (osr.SRS_PP_FALSE_EASTING, 500000.0),
        (osr.SRS_PP_FALSE_NORTHING, 10000000.0),
    ]

    for param in parm_list:
        value = utm_srs.GetProjParm(param[0], -1111)
        assert value == pytest.approx(
            param[1], abs=0.00000000000010
        ), "got %g for %s instead of %g." % (value, param[0], param[1])


###############################################################################
def test_SetPROJAuxDbPaths():
    # This test use auxiliary database created with proj 6.3.2
    # (tested up to 8.0.0) and can be sensitive to future
    # database structure change.
    #
    # See PR https://github.com/OSGeo/gdal/pull/3590
    subprocess.check_call(
        [sys.executable, "osr_basic_subprocess.py", "aux_db_test"],
        env=os.environ.copy(),
    )


###############################################################################
# Test IsDynamic()


@pytest.mark.require_proj(7, 2)
def test_osr_basic_is_dynamic():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(7665)  # WGS 84 (G1762) (3D)
    assert srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4171)  # RGF93
    assert not srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)  # WGS84 (generic), using datum ensemble
    assert srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=0,0,0")
    assert not srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258)  # ETRS89 (generic), using datum ensemble
    # Just run it. Prior to PROJ 9.6, this was not a dynamic CRS
    # Since PROJ 9.6, it is a dynamic CRS.
    srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Latitude",NORTH],
    AXIS["Longitude",EAST],
    AUTHORITY["EPSG","4326"]]""")
    assert srs.IsDynamic()

    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:9057+3855")  # WGS 84 (G1762) + EGM2008 height
    assert srs.IsDynamic()


###############################################################################
# Test SetCoordinateEpoch() / GetCoordinateEpoch


def test_osr_basic_set_get_coordinate_epoch():

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")

    srs.SetCoordinateEpoch(2021.3)
    assert srs.GetCoordinateEpoch() == 2021.3

    clone = srs.Clone()
    assert clone.GetCoordinateEpoch() == 2021.3
    assert srs.IsSame(clone)

    clone.SetCoordinateEpoch(0)
    assert not srs.IsSame(clone)
    assert srs.IsSame(clone, ["IGNORE_COORDINATE_EPOCH=YES"])


###############################################################################
# Test exporting a projection method that is WKT2-only (#4133)


def test_osr_basic_export_equal_earth_to_wkt():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(8859)
    wkt = srs.ExportToWkt()
    assert wkt
    assert wkt == srs.ExportToWkt(["FORMAT=WKT2"])
    assert 'METHOD["Equal Earth",' in wkt
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test too long user input


def test_osr_basic_set_from_user_input_too_long():

    srs = osr.SpatialReference()
    with pytest.raises(Exception):
        srs.SetFromUserInput("+proj=pipeline " + "+step +proj=longlat " * 100000)

    with pytest.raises(Exception):
        srs.SetFromUserInput("AUTO:" + "x" * 100000)

    with pytest.raises(Exception):
        srs.SetFromUserInput("http://opengis.net/def/crs/" + "x" * 100000)


###############################################################################
# Test GetAxesCount()


def test_osr_basic_get_axes_count():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=tmerc +datum=WGS84")
    assert srs.GetAxesCount() == 2

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=tmerc +ellps=GRS80 +towgs84=0,0,0")
    assert srs.GetAxesCount() == 2

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4979)
    assert srs.GetAxesCount() == 3

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=tmerc +datum=WGS84 +geoidgrids=foo.gtx")
    assert srs.GetAxesCount() == 3


###############################################################################
# Test exporting a CRS type that is WKT2-only (#3927)


def test_osr_basic_export_derived_projected_crs_to_wkt():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""DERIVEDPROJCRS["unnamed",
    BASEPROJCRS["unnamed",
        BASEGEOGCRS["WGS 84",
            DATUM["World Geodetic System 1984",
                ELLIPSOID["WGS 84",6378137,298.257223563,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,
                ANGLEUNIT["degree",0.0174532925199433]],
            ID["EPSG",4326]],
        CONVERSION["Oblique Stereographic",
            METHOD["Oblique Stereographic",
                ID["EPSG",9809]],
            PARAMETER["Latitude of natural origin",35.91600629551671,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",-84.21682058830596,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.9999411285026271,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",760932.0392583184,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",177060.0539497079,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]]]],
    DERIVINGCONVERSION["unnamed",
        METHOD["PROJ affine"],
        PARAMETER["xoff",-25.365221999119967,
            SCALEUNIT["unity",1]],
        PARAMETER["yoff",-30.51036324049346,
            SCALEUNIT["unity",1]],
        PARAMETER["zoff",31.80827133745305,
            SCALEUNIT["unity",1]],
        PARAMETER["s11",3.280833333333336,
            SCALEUNIT["unity",1]],
        PARAMETER["s12",6.938893903907228e-18,
            SCALEUNIT["unity",1]],
        PARAMETER["s13",0,
            SCALEUNIT["unity",1]],
        PARAMETER["s21",-6.938893903907228e-18,
            SCALEUNIT["unity",1]],
        PARAMETER["s22",3.280833333333336,
            SCALEUNIT["unity",1]],
        PARAMETER["s23",0,
            SCALEUNIT["unity",1]],
        PARAMETER["s31",0.000144198502849885,
            SCALEUNIT["unity",1]],
        PARAMETER["s32",-0.00016681800457995442,
            SCALEUNIT["unity",1]],
        PARAMETER["s33",3.2808333333333355,
            SCALEUNIT["unity",1]]],
        CS[Cartesian,3],
        AXIS["(E)",east,
            ORDER[1],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["(N)",north,
            ORDER[2],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["ellipsoidal height (h)",up,
            ORDER[3],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]]]""")
    wkt = srs.ExportToWkt()
    assert wkt
    assert wkt == srs.ExportToWkt(["FORMAT=WKT2"])
    assert wkt.startswith("DERIVEDPROJCRS")
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test osr.GetPROJEnableNetwork / osr.SetPROJEnableNetwork


@pytest.mark.require_proj(7)
def test_osr_basic_proj_network():

    initial_value = osr.GetPROJEnableNetwork()
    try:
        new_val = not initial_value
        osr.SetPROJEnableNetwork(new_val)
        assert osr.GetPROJEnableNetwork() == new_val
    finally:
        osr.SetPROJEnableNetwork(initial_value)


###############################################################################


def test_osr_basic_get_linear_units_compound_engineering_crs():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""COMPOUNDCRS["None",
    ENGCRS["None",
        EDATUM[""],
        CS[Cartesian,2],
            AXIS["easting",east,
                ORDER[1],
                LENGTHUNIT["US survey foot",0.304800609601219,
                    ID["EPSG",9003]]],
            AXIS["northing",north,
                ORDER[2],
                LENGTHUNIT["US survey foot",0.304800609601219,
                    ID["EPSG",9003]]]],
    VERTCRS["Local Meter",
        VDATUM["unknown"],
        CS[vertical,1],
            AXIS["up",up,
                LENGTHUNIT["metre",1,
                    ID["EPSG",9001]]]]]""")

    assert srs.GetLinearUnits() == pytest.approx(0.304800609601219)
    assert srs.GetLinearUnitsName() == "US survey foot"


###############################################################################
# Test EPSG:horizontal_code+geographic_code


@pytest.mark.require_proj(9)
def test_osr_basic_epsg_horizontal_and_ellipsoidal_height():

    sr = osr.SpatialReference()
    assert sr.SetFromUserInput("EPSG:3157+4617") == ogr.OGRERR_NONE
    assert sr.GetAxesCount() == 3


###############################################################################
# Test SRS_PP_CENTRAL_MERIDIAN with a CRS that has only a WKT2 representation


def test_osr_basic_eqearth_central_meridian():

    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        "+proj=eqearth +lon_0=150 +x_0=0 +y_0=0 +R=6371008.7714 +units=m +type=crs"
    )
    assert srs.GetProjParm(osr.SRS_PP_CENTRAL_MERIDIAN, 0) == 150


###############################################################################
# Test osr.ExceptionMgr()


def test_osr_exceptions():

    with pytest.raises(Exception):
        with osr.ExceptionMgr():
            srs = osr.SpatialReference()
            srs.ImportFromEPSG(0)


###############################################################################
# Test SetFromUserInput() with COORDINATEMETADATA[]


@pytest.mark.require_proj(9, 2)
def test_osr_basic_set_from_user_input_COORDINATEMETADATA_with_epoch():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""COORDINATEMETADATA[
    GEOGCRS["WGS 84 (G1762)",
        DYNAMIC[
            FRAMEEPOCH[2005]],
        DATUM["World Geodetic System 1984 (G1762)",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        CS[ellipsoidal,2],
            AXIS["geodetic latitude (Lat)",north,
                ORDER[1],
                ANGLEUNIT["degree",0.0174532925199433]],
            AXIS["geodetic longitude (Lon)",east,
                ORDER[2],
                ANGLEUNIT["degree",0.0174532925199433]],
        USAGE[
            SCOPE["Geodesy. Navigation and positioning using GPS satellite system."],
            AREA["World."],
            BBOX[-90,-180,90,180]],
        ID["EPSG",9057]],
    EPOCH[2025]]""") == ogr.OGRERR_NONE
    assert srs.GetName() == "WGS 84 (G1762)"
    assert srs.GetCoordinateEpoch() == 2025.0


###############################################################################
# Test SetFromUserInput() with COORDINATEMETADATA[]


@pytest.mark.require_proj(9, 2)
def test_osr_basic_set_from_user_input_COORDINATEMETADATA_without_epoch():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""COORDINATEMETADATA[
    GEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        CS[ellipsoidal,2],
            AXIS["geodetic latitude (Lat)",north,
                ORDER[1],
                ANGLEUNIT["degree",0.0174532925199433]],
            AXIS["geodetic longitude (Lon)",east,
                ORDER[2],
                ANGLEUNIT["degree",0.0174532925199433]],
        USAGE[
            SCOPE["Geodesy. Navigation and positioning using GPS satellite system."],
            AREA["World."],
            BBOX[-90,-180,90,180]],
        ID["EPSG",9057]]]""") == ogr.OGRERR_NONE
    assert srs.GetName() == "WGS 84"
    assert srs.GetCoordinateEpoch() == 0.0


###############################################################################
# Test SetProjCS() on a boundCRS


def test_osr_basic_SetProjCS_Bound():

    srs = osr.SpatialReference()
    proj4str = "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=GRS80 +towgs84=1,2,3,0,0,0,0 +units=m +no_defs"
    srs.SetFromUserInput(proj4str)
    srs.SetProjCS("Projected CRS name")
    assert srs.GetName() == "Projected CRS name"
    assert srs.ExportToProj4() == proj4str


###############################################################################


def test_osr_basic_warning_exceptions():

    python_exe = sys.executable
    cmd = '%s -c "from osgeo import osr; ' % python_exe + (
        "osr.SpatialReference();" ' " '
    )
    try:
        _, err = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))
    assert "FutureWarning: Neither osr.UseExceptions()" in err


def test_osr_basic_wkt_format_configuration_option():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32145)

    with gdal.config_option("OSR_WKT_FORMAT", "WKT1"):
        wkt1 = srs.ExportToWkt()

    with gdal.config_option("OSR_WKT_FORMAT", "WKT2"):
        wkt2 = srs.ExportToWkt()

    assert "BBOX" not in wkt1
    assert "BBOX" in wkt2


def test_osr_basic_default_axis_mapping_strategy():

    with gdal.config_option(
        "OSR_DEFAULT_AXIS_MAPPING_STRATEGY", "TRADITIONAL_GIS_ORDER"
    ):
        crs1 = osr.SpatialReference()
        crs1.ImportFromEPSG(4326)

    crs2 = osr.SpatialReference()
    crs2.ImportFromEPSG(4326)

    assert crs1.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER
    assert crs2.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT


###############################################################################


@pytest.mark.require_proj(9, 4)
def test_osr_basic_urn_coordinateMetadata():

    srs = osr.SpatialReference()
    assert (
        srs.SetFromUserInput(
            "urn:ogc:def:coordinateMetadata:NRCAN::NAD83_CSRS_1997_MTM7_HT2_1997"
        )
        == ogr.OGRERR_NONE
    )
    assert srs.GetName() == "NAD83(CSRS)v7 / MTM zone 7 + CGVD28 height"
    assert srs.GetCoordinateEpoch() == 1997.0


###############################################################################


@pytest.mark.require_proj(9, 4)
def test_osr_basic_set_from_user_input_EPSG_8254_at_something():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:8255 @ 2002.5")  # NAD83(CSRS)v7
    assert srs.GetName() == "NAD83(CSRS)v7"
    assert srs.GetCoordinateEpoch() == 2002.5


###############################################################################


@pytest.mark.require_proj(9, 4)
def test_osr_basic_has_point_motion_operation():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(8255)  # NAD83(CSRS)v7
    assert srs.HasPointMotionOperation()


###############################################################################


# Test workaround for https://github.com/OSGeo/PROJ/pull/4166
def test_osr_basic_export_wkt_utm_south():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=utm +zone=1 +south +datum=WGS84")

    assert 'ID["EPSG",16101]' in srs.ExportToWkt(["FORMAT=WKT2_2019"])

    j = json.loads(srs.ExportToPROJJSON())
    assert j["conversion"]["id"]["code"] == 16101


###############################################################################


def test_osr_basic_GetAuthorityListFromDatabase():

    ret = osr.GetAuthorityListFromDatabase()
    assert "EPSG" in ret
    assert "PROJ" in ret


###############################################################################


@pytest.mark.parametrize(
    "args,kwargs",
    (
        pytest.param([], {"epsg": 4326}, id="epsg"),
        pytest.param(["+proj=utm +zone=18 +datum=WGS84"], {}, id="proj4"),
        pytest.param(
            [
                'PROJCS["NAD83 / Vermont (ftUS)",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",42.5],PARAMETER["central_meridian",-72.5],PARAMETER["scale_factor",0.999964286],PARAMETER["false_easting",1640416.6667],PARAMETER["false_northing",0],UNIT["US survey foot",0.304800609601219,AUTHORITY["EPSG","9003"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","5646"]]'
            ],
            {},
            id="wkt",
        ),
        pytest.param(
            [
                '{\n  "$schema": "https://proj.org/schemas/v0.7/projjson.schema.json",\n  "type": "ProjectedCRS",\n  "name": "NAD83 / Vermont (ftUS)",\n  "base_crs": {\n    "name": "NAD83",\n    "datum": {\n      "type": "GeodeticReferenceFrame",\n      "name": "North American Datum 1983",\n      "ellipsoid": {\n        "name": "GRS 1980",\n        "semi_major_axis": 6378137,\n        "inverse_flattening": 298.257222101\n      }\n    },\n    "coordinate_system": {\n      "subtype": "ellipsoidal",\n      "axis": [\n        {\n          "name": "Geodetic latitude",\n          "abbreviation": "Lat",\n          "direction": "north",\n          "unit": "degree"\n        },\n        {\n          "name": "Geodetic longitude",\n          "abbreviation": "Lon",\n          "direction": "east",\n          "unit": "degree"\n        }\n      ]\n    },\n    "id": {\n      "authority": "EPSG",\n      "code": 4269\n    }\n  },\n  "conversion": {\n    "name": "SPCS83 Vermont zone (US survey foot)",\n    "method": {\n      "name": "Transverse Mercator",\n      "id": {\n        "authority": "EPSG",\n        "code": 9807\n      }\n    },\n    "parameters": [\n      {\n        "name": "Latitude of natural origin",\n        "value": 42.5,\n        "unit": "degree",\n        "id": {\n          "authority": "EPSG",\n          "code": 8801\n        }\n      },\n      {\n        "name": "Longitude of natural origin",\n        "value": -72.5,\n        "unit": "degree",\n        "id": {\n          "authority": "EPSG",\n          "code": 8802\n        }\n      },\n      {\n        "name": "Scale factor at natural origin",\n        "value": 0.999964286,\n        "unit": "unity",\n        "id": {\n          "authority": "EPSG",\n          "code": 8805\n        }\n      },\n      {\n        "name": "False easting",\n        "value": 1640416.6667,\n        "unit": {\n          "type": "LinearUnit",\n          "name": "US survey foot",\n          "conversion_factor": 0.304800609601219\n        },\n        "id": {\n          "authority": "EPSG",\n          "code": 8806\n        }\n      },\n      {\n        "name": "False northing",\n        "value": 0,\n        "unit": {\n          "type": "LinearUnit",\n          "name": "US survey foot",\n          "conversion_factor": 0.304800609601219\n        },\n        "id": {\n          "authority": "EPSG",\n          "code": 8807\n        }\n      }\n    ]\n  },\n  "coordinate_system": {\n    "subtype": "Cartesian",\n    "axis": [\n      {\n        "name": "Easting",\n        "abbreviation": "X",\n        "direction": "east",\n        "unit": {\n          "type": "LinearUnit",\n          "name": "US survey foot",\n          "conversion_factor": 0.304800609601219\n        }\n      },\n      {\n        "name": "Northing",\n        "abbreviation": "Y",\n        "direction": "north",\n        "unit": {\n          "type": "LinearUnit",\n          "name": "US survey foot",\n          "conversion_factor": 0.304800609601219\n        }\n      }\n    ]\n  },\n  "scope": "Engineering survey, topographic mapping.",\n  "area": "United States (USA) - Vermont - counties of Addison; Bennington; Caledonia; Chittenden; Essex; Franklin; Grand Isle; Lamoille; Orange; Orleans; Rutland; Washington; Windham; Windsor.",\n  "bbox": {\n    "south_latitude": 42.72,\n    "west_longitude": -73.44,\n    "north_latitude": 45.03,\n    "east_longitude": -71.5\n  },\n  "id": {\n    "authority": "EPSG",\n    "code": 5646\n  }\n}'
            ],
            {},
            id="projjson text",
        ),
        pytest.param(
            [
                {
                    "$schema": "https://proj.org/schemas/v0.7/projjson.schema.json",
                    "type": "ProjectedCRS",
                    "name": "NAD83 / Vermont (ftUS)",
                    "base_crs": {
                        "name": "NAD83",
                        "datum": {
                            "type": "GeodeticReferenceFrame",
                            "name": "North American Datum 1983",
                            "ellipsoid": {
                                "name": "GRS 1980",
                                "semi_major_axis": 6378137,
                                "inverse_flattening": 298.257222101,
                            },
                        },
                        "coordinate_system": {
                            "subtype": "ellipsoidal",
                            "axis": [
                                {
                                    "name": "Geodetic latitude",
                                    "abbreviation": "Lat",
                                    "direction": "north",
                                    "unit": "degree",
                                },
                                {
                                    "name": "Geodetic longitude",
                                    "abbreviation": "Lon",
                                    "direction": "east",
                                    "unit": "degree",
                                },
                            ],
                        },
                        "id": {"authority": "EPSG", "code": 4269},
                    },
                    "conversion": {
                        "name": "SPCS83 Vermont zone (US survey foot)",
                        "method": {
                            "name": "Transverse Mercator",
                            "id": {"authority": "EPSG", "code": 9807},
                        },
                        "parameters": [
                            {
                                "name": "Latitude of natural origin",
                                "value": 42.5,
                                "unit": "degree",
                                "id": {"authority": "EPSG", "code": 8801},
                            },
                            {
                                "name": "Longitude of natural origin",
                                "value": -72.5,
                                "unit": "degree",
                                "id": {"authority": "EPSG", "code": 8802},
                            },
                            {
                                "name": "Scale factor at natural origin",
                                "value": 0.999964286,
                                "unit": "unity",
                                "id": {"authority": "EPSG", "code": 8805},
                            },
                            {
                                "name": "False easting",
                                "value": 1640416.6667,
                                "unit": {
                                    "type": "LinearUnit",
                                    "name": "US survey foot",
                                    "conversion_factor": 0.304800609601219,
                                },
                                "id": {"authority": "EPSG", "code": 8806},
                            },
                            {
                                "name": "False northing",
                                "value": 0,
                                "unit": {
                                    "type": "LinearUnit",
                                    "name": "US survey foot",
                                    "conversion_factor": 0.304800609601219,
                                },
                                "id": {"authority": "EPSG", "code": 8807},
                            },
                        ],
                    },
                    "coordinate_system": {
                        "subtype": "Cartesian",
                        "axis": [
                            {
                                "name": "Easting",
                                "abbreviation": "X",
                                "direction": "east",
                                "unit": {
                                    "type": "LinearUnit",
                                    "name": "US survey foot",
                                    "conversion_factor": 0.304800609601219,
                                },
                            },
                            {
                                "name": "Northing",
                                "abbreviation": "Y",
                                "direction": "north",
                                "unit": {
                                    "type": "LinearUnit",
                                    "name": "US survey foot",
                                    "conversion_factor": 0.304800609601219,
                                },
                            },
                        ],
                    },
                    "scope": "Engineering survey, topographic mapping.",
                    "area": "United States (USA) - Vermont - counties of Addison; Bennington; Caledonia; Chittenden; Essex; Franklin; Grand Isle; Lamoille; Orange; Orleans; Rutland; Washington; Windham; Windsor.",
                    "bbox": {
                        "south_latitude": 42.72,
                        "west_longitude": -73.44,
                        "north_latitude": 45.03,
                        "east_longitude": -71.5,
                    },
                    "id": {"authority": "EPSG", "code": 5646},
                }
            ],
            {},
            id="projjson dict",
        ),
    ),
)
def test_osr_basic_constructor(args, kwargs):

    srs = osr.SpatialReference(*args, **kwargs)

    assert srs.ExportToWkt() != ""


@pytest.mark.parametrize("arg", ("", None))
def test_osr_basic_constructor_empty(arg):

    srs = osr.SpatialReference(arg)
    assert srs is not None


def test_osr_basic_constructor_overspecified():

    # The arguments provided here don't make any sense.
    # But SpatialReference.SetFromUserInput doesn't seem to complain if we pass it
    # unexpected options, so we have no way to catch this for now.

    with pytest.raises(ValueError, match="Unexpected argument"):
        osr.SpatialReference(proj4="+proj=utm +zone=18 +datum=WGS84", epsg=4326)


def test_osr_basic_constructor_invalid_kwarg():

    with pytest.raises(ValueError, match="Unexpected argument"):
        osr.SpatialReference(description="VT State Plane")
