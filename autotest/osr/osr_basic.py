#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Basic tests of OGRSpatialReference (OSR) operation, not including
#           support for actual reprojection or use of EPSG tables.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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



import gdaltest
from osgeo import osr
import pytest


###############################################################################
# Create a UTM WGS84 coordinate system and check various items.

def test_osr_basic_1():

    utm_srs = osr.SpatialReference()
    # Southern hemisphere
    utm_srs.SetUTM(11, 0)
    utm_srs.SetWellKnownGeogCS('WGS84')

    assert utm_srs.GetUTMZone() == -11

    # Northern hemisphere
    utm_srs.SetUTM(11)

    assert utm_srs.GetUTMZone() == 11

    parm_list = \
        [(osr.SRS_PP_CENTRAL_MERIDIAN, -117.0),
         (osr.SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
            (osr.SRS_PP_SCALE_FACTOR, 0.9996),
            (osr.SRS_PP_FALSE_EASTING, 500000.0),
            (osr.SRS_PP_FALSE_NORTHING, 0.0)]

    for parm in parm_list:
        value = utm_srs.GetProjParm(parm[0], -1111)
        assert abs(value - parm[1]) <= .00000000000010, ('got %g for %s instead of %g.'
                                 % (value, parm[0], parm[1]))

    auth_list = [('GEOGCS', '4326'),
                 ('DATUM', '6326')]

    for auth in auth_list:
        assert utm_srs.GetAuthorityName(auth[0]) == 'EPSG', \
            ('Got authority %s instead of EPSG for %s'
                                 % (utm_srs.GetAuthorityName(auth[0]),
                                     auth[0]))

        assert str(utm_srs.GetAuthorityCode(auth[0])) == auth[1], \
            ('Got code %s instead of %s for %s'
                                 % (utm_srs.GetAuthorityName(auth[0]),
                                     auth[1], auth[0]))

    
###############################################################################
# Simple default NAD83 State Plane zone.


def test_osr_basic_2():

    srs = osr.SpatialReference()
    srs.SetStatePlane(403, 1)  # California III NAD83.
    # print srs.ExportToPrettyWkt()

    parm_list = \
        [(osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
         (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
            (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
            (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
            (osr.SRS_PP_FALSE_EASTING, 2000000.0),
            (osr.SRS_PP_FALSE_NORTHING, 500000.0)]

    for parm in parm_list:
        value = srs.GetProjParm(parm[0], -1111)
        assert gdaltest.approx_equal(parm[1], value), \
            ('got %.16g for %s instead of %.16g.'
                                 % (value, parm[0], parm[1]))

    auth_list = [('GEOGCS', '4269'),
                 ('DATUM', '6269'),
                 ('PROJCS', '26943'),
                 ('PROJCS|UNIT', '9001')]

    for auth in auth_list:
        assert srs.GetAuthorityName(auth[0]) == 'EPSG', \
            ('Got authority %s instead of EPSG for %s'
                                 % (srs.GetAuthorityName(auth[0]),
                                    auth[0]))

        assert str(srs.GetAuthorityCode(auth[0])) == auth[1], \
            ('Got code %s instead of %s for %s'
                                 % (srs.GetAuthorityCode(auth[0]),
                                    auth[1], auth[0]))

    
###############################################################################
# NAD83 State Plane zone, but overridden to be in Feet.


def test_osr_basic_3():

    srs = osr.SpatialReference()

    # California III NAD83 (feet)
    srs.SetStatePlane(403, 1, 'Foot', 0.3048006096012192)
    # print srs.ExportToPrettyWkt()

    parm_list = \
        [(osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
         (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
            (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
            (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
            (osr.SRS_PP_FALSE_EASTING, 6561666.666666667),
            (osr.SRS_PP_FALSE_NORTHING, 1640416.666666667)]

    for parm in parm_list:
        value = srs.GetProjParm(parm[0], -1111)
        assert gdaltest.approx_equal(parm[1], value), \
            ('got %.16g for %s instead of %.16g.'
                                 % (value, parm[0], parm[1]))

    auth_list = [('GEOGCS', '4269'),
                 ('DATUM', '6269')]

    for auth in auth_list:
        assert srs.GetAuthorityName(auth[0]) == 'EPSG', \
            ('Got authority %s instead of EPSG for %s'
                                 % (srs.GetAuthorityName(auth[0]),
                                    auth[0]))

        assert str(srs.GetAuthorityCode(auth[0])) == auth[1], \
            ('Got code %s instead of %s for %s'
                                 % (srs.GetAuthorityCode(auth[0]),
                                    auth[1], auth[0]))

    assert srs.GetAuthorityName('PROJCS') is None, \
        'Got a PROJCS Authority but we should not'

    assert str(srs.GetAuthorityCode('PROJCS|UNIT')) != '9001', \
        'Got METER authority code on linear units.'

    assert srs.GetLinearUnitsName() == 'Foot', 'Didnt get Foot linear units'

    if srs.GetLinearUnits() != 0.3048006096012192:
        print('%.16g' % srs.GetLinearUnits())
        pytest.fail('Didnt get Foot linear units')

    if srs.GetTargetLinearUnits('PROJCS') != 0.3048006096012192:
        print('%.16g' % srs.GetTargetLinearUnits('PROJCS'))
        pytest.fail('Didnt get Foot linear units')

    if srs.GetTargetLinearUnits(None) != 0.3048006096012192:
        print('%.16g' % srs.GetTargetLinearUnits(None))
        pytest.fail('Didnt get Foot linear units')

    

###############################################################################
# Translate a coordinate system with nad shift into to PROJ.4 and back
# and verify that the TOWGS84 parameters are preserved.

def test_osr_basic_4():

    srs = osr.SpatialReference()
    srs.SetGS(cm=-117.0, fe=100000.0, fn=100000)
    srs.SetGeogCS('Test GCS', 'Test Datum', 'WGS84',
                  osr.SRS_WGS84_SEMIMAJOR, osr.SRS_WGS84_INVFLATTENING)

    srs.SetTOWGS84(1, 2, 3)

    assert srs.GetTOWGS84() == (1, 2, 3, 0, 0, 0, 0), 'GetTOWGS84() result is wrong.'

    proj4 = srs.ExportToProj4()

    srs2 = osr.SpatialReference()
    srs2.ImportFromProj4(proj4)

    assert srs2.GetTOWGS84() == (1, 2, 3, 0, 0, 0, 0), \
        'GetTOWGS84() result is wrong after PROJ.4 conversion.'

###############################################################################
# Test URN support for OGC:CRS84.


def test_osr_basic_5():

    wkt_1 = osr.GetUserInputAsWKT('urn:ogc:def:crs:OGC:1.3:CRS84')
    wkt_2 = osr.GetUserInputAsWKT('WGS84')
    assert wkt_1 == wkt_2, 'CRS84 lookup not as expected.'

###############################################################################
# Test URN support for EPSG


def test_osr_basic_6():

    # Without version
    wkt_1 = osr.GetUserInputAsWKT('urn:x-ogc:def:crs:EPSG::4326')
    assert not (wkt_1.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_1.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1), \
        'EPSG:4326 urn lookup not as expected.'

    # With a version
    wkt_2 = osr.GetUserInputAsWKT('urn:x-ogc:def:crs:EPSG:6.6:4326')
    if wkt_2.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_2.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        print(wkt_1)
        pytest.fail('EPSG:4326 urn lookup not as expected.')

    # Without version, but with no repeated :. Probably illegal from my understanding
    # of http://www.opengeospatial.org/ogcUrnPolicy, but found quite often in the wild
    # especially in content returned by GeoServer
    wkt_2 = osr.GetUserInputAsWKT('urn:x-ogc:def:crs:EPSG:4326')
    if wkt_2.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_2.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        print(wkt_1)
        pytest.fail('EPSG:4326 urn lookup not as expected.')

    
###############################################################################
# Test URN support for auto projection.


def test_osr_basic_7():

    wkt_1 = osr.GetUserInputAsWKT('urn:ogc:def:crs:OGC::AUTO42001:-117:33')
    wkt_2 = 'PROJCS["UTM Zone 11, Northern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1,AUTHORITY["EPSG","9001"]]]'
    assert wkt_1 == wkt_2, 'AUTO42001 urn lookup not as expected.'

###############################################################################
# Test the SetLinearUnitsAndUpdateParameters() method.


def test_osr_basic_8():

    srs = osr.SpatialReference()

    srs.SetFromUserInput('+proj=tmerc +x_0=1000 +datum=WGS84 +units=m')
    srs.SetLinearUnits('Foot', 0.3048)
    fe = srs.GetProjParm('false_easting')

    assert fe == 1000.0, 'false easting was unexpectedly updated.'

    if 'SetLinearUnitsAndUpdateParameters' not in dir(srs):
        pytest.skip()

    srs.SetFromUserInput('+proj=tmerc +x_0=1000 +datum=WGS84 +units=m')
    srs.SetLinearUnitsAndUpdateParameters('Foot', 0.3048)
    fe = srs.GetProjParm('false_easting')

    assert fe != 1000.0, 'false easting was unexpectedly not updated.'

    assert abs(fe - 3280.840) <= 0.01, 'wrong updated false easting value.'

###############################################################################
# Test the Validate() method.


def test_osr_basic_9():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("PROJCS[\"unnamed\",GEOGCS[\"unnamed ellipse\",DATUM[\"unknown\",SPHEROID[\"unnamed\",6378137,0]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Mercator_2SP\"],PARAMETER[\"standard_parallel_1\",0],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",0],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"Meter\",1],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"]]")
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
        AUTHORITY["EPSG","4140"]],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["central_meridian",-63],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    AUTHORITY["EPSG","2038"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    if srs1.IsSame(srs2):
        return

    pytest.fail()

###############################################################################
# Test URN support for OGC:CRS84.


def test_osr_basic_12():

    wkt_1 = osr.GetUserInputAsWKT('CRS:84')
    wkt_2 = osr.GetUserInputAsWKT('WGS84')
    assert wkt_1 == wkt_2, 'CRS:84 lookup not as expected.'

###############################################################################
# Test GEOCCS lookup in supporting data files.


def test_osr_basic_13():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4328)

    expected_wkt = 'GEOCCS["WGS 84 (geocentric) (deprecated)",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Geocentric X",OTHER],AXIS["Geocentric Y",OTHER],AXIS["Geocentric Z",NORTH],AUTHORITY["EPSG","4328"]]'
    wkt = srs.ExportToWkt()

    assert wkt == expected_wkt, 'did not get expected GEOCCS WKT.'

    assert srs.IsGeocentric(), 'srs not recognised as geocentric.'

    assert srs.Validate() == 0, 'epsg geoccs import does not validate!'

###############################################################################
# Manually setup a simple geocentric/wgs84 srs.


def test_osr_basic_14():

    srs = osr.SpatialReference()
    srs.SetGeocCS('My Geocentric')
    srs.SetWellKnownGeogCS('WGS84')
    srs.SetLinearUnits('meter', 1.0)

    expected_wkt = 'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]'
    wkt = srs.ExportToWkt()

    assert wkt == expected_wkt, 'did not get expected GEOCCS WKT.'

    assert srs.IsGeocentric(), 'srs not recognised as geocentric.'

    assert srs.Validate() == 0, 'geocentric srs not recognised as valid.'

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
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]"""

    srs = osr.SpatialReference()
    srs.SetFromUserInput(wkt)

    assert srs.Validate() != 0, 'Validate() fails to detect misordering.'

    srs.Fixup()

    assert srs.Validate() == 0, 'Fixup() failed to correct srs.'

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
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]""")
    srs.SetGeocCS("a")
    expect_wkt = """GEOCCS["a",
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        pytest.fail('Did not get expected result.')

    # Build GEOCCS from a valid GEOGCS
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetGeocCS("a")
    expect_wkt = """GEOCCS["a",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]]]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        pytest.fail('Did not get expected result.')

    # Error expected. Cannot work on a PROJCS
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ret = srs.SetGeocCS("a")
    if ret == 0:
        print(srs)
        pytest.fail('expected failure')

    # Limit test : build GEOCCS from an invalid GEOGCS
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["foo"]""")
    srs.SetGeocCS("bar")
    expect_wkt = """GEOCCS["bar"]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        pytest.fail('Did not get expected result.')

    
###############################################################################
# Test OGC URL support


def test_osr_basic_17():

    wkt_1 = osr.GetUserInputAsWKT('urn:ogc:def:crs:EPSG::4326')
    wkt_2 = osr.GetUserInputAsWKT('http://www.opengis.net/def/crs/EPSG/0/4326')
    assert wkt_1 == wkt_2, 'CRS URL parsing not as expected.'

###############################################################################
# Test OGC URL support for compound CRS


def test_osr_basic_18():

    # This is a dummy one, but who cares
    wkt = osr.GetUserInputAsWKT('http://www.opengis.net/def/crs-compound?1=http://www.opengis.net/def/crs/EPSG/0/4326&2=http://www.opengis.net/def/crs/EPSG/0/4326')
    assert wkt.startswith('COMPD_CS'), 'CRS URL parsing not as expected.'

###############################################################################
# Test well known GCS names against their corresponding EPSG definitions (#6080)


def test_osr_basic_19():

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS84')

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4326)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS72')

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4322)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('NAD27')

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4267)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('NAD83')

    sr_ref = osr.SpatialReference()
    sr_ref.ImportFromEPSG(4269)

    assert sr.ExportToWkt() == sr_ref.ExportToWkt()

###############################################################################
# Test GetAxisName() and GetAxisOrientation() and GetAngularUnitsName()


def test_osr_basic_20():

    sr = osr.SpatialReference()
    sr.ImportFromEPSGA(4326)

    assert sr.GetAxisName(None, 0) == 'Latitude'

    assert sr.GetAxisOrientation(None, 0) == osr.OAO_North

    assert sr.GetAxisName('GEOGCS', 1) == 'Longitude'

    assert sr.GetAxisOrientation('GEOGCS', 1) == osr.OAO_East

    assert sr.GetAngularUnitsName() == 'degree'

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
    PARAMETER["standard_parallel_1",4.45405154589751]]"""

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
    PARAMETER["false_northing",900000]]"""

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
    PARAMETER["false_northing",900000]]"""

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
    PARAMETER["false_northing",6602157.83881033]]"""
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
    PARAMETER["false_northing",6600000]]""")

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
    PARAMETER["false_northing",6600000]]"""
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
    PARAMETER["false_northing",6600000]]""")

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
    PARAMETER["false_northing",6637093.292952879]]"""
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
    PARAMETER["false_northing",6637093.292952879]]"""
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
    PARAMETER["false_northing",4185861.369]]"""
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
        PARAMETER["false_northing",0]]""")

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
    PARAMETER["false_northing",0]]"""
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
            UNIT["degree",0.0174532925199433]]""")
    sr2 = sr.ConvertToOtherProjection('Mercator_1SP')
    assert sr2 is None

    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0]]""")

    sr2 = sr.ConvertToOtherProjection(None)
    assert sr2 is None

    sr2 = sr.ConvertToOtherProjection('foo')
    assert sr2 is None

    sr2 = sr.ConvertToOtherProjection('Mercator_1SP')
    assert sr2.IsSame(sr) != 0

    # Mercator_1SP -> Mercator_2SP: Negative scale factor
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",-0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0]]""")
    sr2 = sr.ConvertToOtherProjection('Mercator_2SP')
    assert sr2 is None

    # Mercator_1SP -> Mercator_2SP: Invalid eccentricity
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,0.1]],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Mercator_1SP"],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.5],
        PARAMETER["false_easting",0],
        PARAMETER["false_northing",0]]""")
    sr2 = sr.ConvertToOtherProjection('Mercator_2SP')
    assert sr2 is None

    # Mercator_2SP -> Mercator_1SP: Invalid standard_parallel_1
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",100],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]""")
    sr2 = sr.ConvertToOtherProjection('Mercator_1SP')
    assert sr2 is None

    # Mercator_2SP -> Mercator_1SP: Invalid eccentricity
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,0.1]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",60],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]""")
    sr2 = sr.ConvertToOtherProjection('Mercator_1SP')
    assert sr2 is None

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
    PARAMETER["false_northing",4185861.369]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_2SP')
    assert sr2 is None

    # LCC_1SP -> LCC_2SP: Invalid eccentricity
    sr = osr.SpatialReference()
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
    PARAMETER["false_northing",4185861.369]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_2SP')
    assert sr2 is None

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
    PARAMETER["false_northing",4185861.369]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_2SP')
    assert sr2 is None

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
    PARAMETER["false_northing",4185861.369]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_2SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : Invalid standard_parallel_1
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",246.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : Invalid standard_parallel_2
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",246.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : Invalid latitude_of_origin
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",246.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : abs(stdp1) == abs(stdp2)
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",1],
    PARAMETER["standard_parallel_2",-1],
    PARAMETER["latitude_of_origin",10],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : stdp1 ~= stdp2 ~= 0
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,298.257222101]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",.0000000000000001],
    PARAMETER["standard_parallel_2",.0000000000000002],
    PARAMETER["latitude_of_origin",10],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

    # LCC_2SP -> LCC_1SP : Invalid eccentricity
    sr.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["RGF93",
        DATUM["Reseau_Geodesique_Francais_1993",
            SPHEROID["GRS 1980",6378137,0.1]],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",46.4567],
    PARAMETER["standard_parallel_2",46.4567],
    PARAMETER["latitude_of_origin",46.123],
    PARAMETER["central_meridian",3],
    PARAMETER["false_easting",700000],
    PARAMETER["false_northing",6600000]]""")
    sr2 = sr.ConvertToOtherProjection('Lambert_Conformal_Conic_1SP')
    assert sr2 is None

###############################################################################
# Test corner cases of osr.SetGeocCS()


def test_osr_basic_setgeogcs():

    sr = osr.SpatialReference()
    sr.SetGeogCS(None, None, None, 0, 0, None, 0, None, 0)
    assert sr.ExportToWkt() == 'GEOGCS["unnamed",DATUM["unknown",SPHEROID["unnamed",0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'

    sr.SetGeogCS('a', 'b', 'c', 1, 2, 'd', 3, 'e', 4)
    assert sr.ExportToWkt() == 'GEOGCS["a",DATUM["b",SPHEROID["c",1,2]],PRIMEM["d",3],UNIT["e",4]]'

    sr.SetUTM(31)
    sr.SetGeogCS(None, None, None, 0, 0, None, 0, None, 0)
    assert sr.ExportToWkt() == 'PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["unnamed",DATUM["unknown",SPHEROID["unnamed",0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",0.01308996938995747],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    sr.ImportFromWkt('FOO["bar",GEOGCS[]]')
    assert sr.SetGeogCS(None, None, None, 0, 0, None, 0, None, 0) != 0, sr.ExportToWkt()
    assert sr.ExportToWkt() == 'FOO["bar",GEOGCS[]]'

###############################################################################



