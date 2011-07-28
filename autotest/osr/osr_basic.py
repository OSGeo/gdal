#!/usr/bin/env python
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr


###############################################################################
# Create a UTM WGS84 coordinate system and check various items. 

def osr_basic_1():

    utm_srs = osr.SpatialReference()
    # Southern hemisphere
    utm_srs.SetUTM( 11, 0 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    if utm_srs.GetUTMZone() != -11:
        return 'fail'

    # Northern hemisphere
    utm_srs.SetUTM( 11 )

    if utm_srs.GetUTMZone() != 11:
        return 'fail'

    parm_list = \
              [ (osr.SRS_PP_CENTRAL_MERIDIAN, -117.0),
                (osr.SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                (osr.SRS_PP_SCALE_FACTOR, 0.9996),
                (osr.SRS_PP_FALSE_EASTING, 500000.0),
                (osr.SRS_PP_FALSE_NORTHING, 0.0) ]

    for parm in parm_list:
        value = utm_srs.GetProjParm( parm[0], -1111 )
        if abs(value-parm[1]) > .00000000000010:
            gdaltest.post_reason( 'got %g for %s instead of %g.' \
                                  % (value, parm[0], parm[1] ) )
            return 'fail'

    auth_list = [ ('GEOGCS', '4326'),
                  ('DATUM', '6326') ]

    for auth in auth_list:
        if utm_srs.GetAuthorityName( auth[0] ) != 'EPSG':
            gdaltest.post_reason( 'Got authority %s instead of EPSG for %s' \
                                  % (utm_srs.GetAuthorityName( auth[0] ),
                                     auth[0]) )
            return 'fail'
        
        if str(utm_srs.GetAuthorityCode( auth[0] )) != auth[1]:
            gdaltest.post_reason( 'Got code %s instead of %s for %s' \
                                  % (utm_srs.GetAuthorityName( auth[0] ),
                                     auth[1], auth[0]) )
            return 'fail'

    return 'success'

###############################################################################
# Simple default NAD83 State Plane zone.

def osr_basic_2():
    
    srs = osr.SpatialReference()
    srs.SetStatePlane( 403, 1 )  # California III NAD83.
    #print srs.ExportToPrettyWkt()

    parm_list = \
              [ (osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
                (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
                (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
                (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
                (osr.SRS_PP_FALSE_EASTING, 2000000.0),
                (osr.SRS_PP_FALSE_NORTHING, 500000.0) ]

    for parm in parm_list:
        value = srs.GetProjParm( parm[0], -1111 )
        if not gdaltest.approx_equal(parm[1], value):
            gdaltest.post_reason( 'got %.16g for %s instead of %.16g.' \
                                  % (value, parm[0], parm[1] ) )
            return 'fail'

    auth_list = [ ('GEOGCS', '4269'),
                  ('DATUM', '6269'),
                  ('PROJCS', '26943'),
                  ('PROJCS|UNIT', '9001') ]

    for auth in auth_list:
        if srs.GetAuthorityName( auth[0] ) != 'EPSG':
            gdaltest.post_reason( 'Got authority %s instead of EPSG for %s' \
                                  % (srs.GetAuthorityName( auth[0] ),
                                     auth[0]) )
            return 'fail'
        
        if str(srs.GetAuthorityCode( auth[0] )) != auth[1]:
            gdaltest.post_reason( 'Got code %s instead of %s for %s' \
                                  % (srs.GetAuthorityCode( auth[0] ),
                                     auth[1], auth[0]) )
            return 'fail'

    return 'success'

###############################################################################
# NAD83 State Plane zone, but overridden to be in Feet.

def osr_basic_3():
    
    srs = osr.SpatialReference()

    # California III NAD83 (feet)
    srs.SetStatePlane( 403, 1, 'Foot', 0.3048006096012192 )
    #print srs.ExportToPrettyWkt()
    
    parm_list = \
              [ (osr.SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333),
                (osr.SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667),
                (osr.SRS_PP_LATITUDE_OF_ORIGIN, 36.5),
                (osr.SRS_PP_CENTRAL_MERIDIAN, -120.5),
                (osr.SRS_PP_FALSE_EASTING, 6561666.666666667),
                (osr.SRS_PP_FALSE_NORTHING, 1640416.666666667) ]

    for parm in parm_list:
        value = srs.GetProjParm( parm[0], -1111 )
        if not gdaltest.approx_equal(parm[1], value):
            gdaltest.post_reason( 'got %.16g for %s instead of %.16g.' \
                                  % (value, parm[0], parm[1] ) )
            return 'fail'

    auth_list = [ ('GEOGCS', '4269'),
                  ('DATUM', '6269') ]

    for auth in auth_list:
        if srs.GetAuthorityName( auth[0] ) != 'EPSG':
            gdaltest.post_reason( 'Got authority %s instead of EPSG for %s' \
                                  % (srs.GetAuthorityName( auth[0] ),
                                     auth[0]) )
            return 'fail'
        
        if str(srs.GetAuthorityCode( auth[0] )) != auth[1]:
            gdaltest.post_reason( 'Got code %s instead of %s for %s' \
                                  % (srs.GetAuthorityCode( auth[0] ),
                                     auth[1], auth[0]) )
            return 'fail'

    if srs.GetAuthorityName( 'PROJCS' ) is not None:
        gdaltest.post_reason( 'Got a PROJCS Authority but we shouldnt' )
        return 'fail'

    if str(srs.GetAuthorityCode( 'PROJCS|UNIT' )) is '9001':
        gdaltest.post_reason( 'Got METER authority code on linear units.' )
        return 'fail'

    if srs.GetLinearUnitsName() != 'Foot':
        gdaltest.post_reason( 'Didnt get Foot linear units' )
        return 'fail'
    
    return 'success'


###############################################################################
# Translate a coordinate system with nad shift into to PROJ.4 and back
# and verify that the TOWGS84 parameters are preserved.

def osr_basic_4():
    
    srs = osr.SpatialReference()
    srs.SetGS( cm = -117.0, fe = 100000.0, fn = 100000 )
    srs.SetGeogCS( 'Test GCS', 'Test Datum', 'WGS84', 
                   osr.SRS_WGS84_SEMIMAJOR, osr.SRS_WGS84_INVFLATTENING )
    
    srs.SetTOWGS84( 1, 2, 3 )

    if srs.GetTOWGS84() != (1,2,3,0,0,0,0):
        gdaltest.post_reason( 'GetTOWGS84() result is wrong.' )
        return 'fail'

    proj4 = srs.ExportToProj4()
    
    srs2 = osr.SpatialReference()
    srs2.ImportFromProj4( proj4 )

    if srs2.GetTOWGS84() != (1,2,3,0,0,0,0):
        gdaltest.post_reason( 'GetTOWGS84() result is wrong after PROJ.4 conversion.' )
        return 'fail'

    return 'success'

###############################################################################
# Test URN support for OGC:CRS84.

def osr_basic_5():

    wkt_1 = osr.GetUserInputAsWKT( 'urn:ogc:def:crs:OGC:1.3:CRS84' )
    wkt_2 = osr.GetUserInputAsWKT( 'WGS84' )
    if wkt_1 != wkt_2:
        gdaltest.post_reason( 'CRS84 lookup not as expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Test URN support for EPSG

def osr_basic_6():

    # Without version
    wkt_1 = osr.GetUserInputAsWKT( 'urn:x-ogc:def:crs:EPSG::4326' )
    if wkt_1.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_1.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        print(wkt_1)
        gdaltest.post_reason( 'EPSG:4326 urn lookup not as expected.' )
        return 'fail'

    # With a version
    wkt_1 = osr.GetUserInputAsWKT( 'urn:x-ogc:def:crs:EPSG:6.6:4326' )
    if wkt_1.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_1.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        print(wkt_1)
        print(wkt_2)
        gdaltest.post_reason( 'EPSG:4326 urn lookup not as expected.' )
        return 'fail'

    # Without version, but with no repeated :. Probably illegal from my understanding
    # of http://www.opengeospatial.org/ogcUrnPolicy, but found quite often in the wild
    # especially in content returned by GeoServer
    wkt_1 = osr.GetUserInputAsWKT( 'urn:x-ogc:def:crs:EPSG:4326' )
    if wkt_1.find('GEOGCS["WGS 84",DATUM["WGS_1984"') == -1 or wkt_1.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        print(wkt_1)
        print(wkt_2)
        gdaltest.post_reason( 'EPSG:4326 urn lookup not as expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Test URN support for auto projection.

def osr_basic_7():

    wkt_1 = osr.GetUserInputAsWKT( 'urn:ogc:def:crs:OGC::AUTO42001:-117:33' )
    wkt_2 = 'PROJCS["UTM Zone 11, Northern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1,AUTHORITY["EPSG","9001"]]]'
    if wkt_1 != wkt_2:
        print(wkt_1)
        print(wkt_2)
        gdaltest.post_reason( 'AUTO42001 urn lookup not as expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Test the SetLinearUnitsAndUpdateParameters() method.

def osr_basic_8():

    srs = osr.SpatialReference()

    srs.SetFromUserInput( '+proj=tmerc +x_0=1000 +datum=WGS84 +units=m' )
    srs.SetLinearUnits( 'Foot', 0.3048 )
    fe = srs.GetProjParm( 'false_easting' )

    if fe != 1000.0:
        gdaltest.post_reason( 'false easting was unexpectedly updated.')
        return 'fail'

    if not 'SetLinearUnitsAndUpdateParameters' in dir(srs):
        return 'skip'
    
    srs.SetFromUserInput( '+proj=tmerc +x_0=1000 +datum=WGS84 +units=m' )
    srs.SetLinearUnitsAndUpdateParameters( 'Foot', 0.3048 )
    fe = srs.GetProjParm( 'false_easting' )

    if fe == 1000.0:
        gdaltest.post_reason( 'false easting was unexpectedly not updated.')
        return 'fail'

    if abs(fe-3280.840) > 0.01:
        print(fe)
        gdaltest.post_reason( 'wrong updated false easting value.' )
        return 'fail'

    return 'success'

###############################################################################
# Test the Validate() method.

def osr_basic_9():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("PROJCS[\"unnamed\",GEOGCS[\"unnamed ellipse\",DATUM[\"unknown\",SPHEROID[\"unnamed\",6378137,0]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Mercator_2SP\"],PARAMETER[\"standard_parallel_1\",0],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",0],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"Meter\",1],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"]]")
    if srs.Validate() != 0:
        return 'fail'

    return 'success'


###############################################################################
# Test the Validate() method on PROJCS with AXIS definition (#2739)

def osr_basic_10():

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
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-63],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    AUTHORITY["EPSG","2038"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]""")

    if srs.Validate() != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test the IsSame() method (and the IsSameGeogCS() method through that)

def osr_basic_11():

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

    if srs1.IsSame( srs2 ):
        return 'success'

    return 'fail'

###############################################################################
# Test URN support for OGC:CRS84.

def osr_basic_12():

    wkt_1 = osr.GetUserInputAsWKT( 'CRS:84' )
    wkt_2 = osr.GetUserInputAsWKT( 'WGS84' )
    if wkt_1 != wkt_2:
        gdaltest.post_reason( 'CRS:84 lookup not as expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Test GEOCCS lookup in supporting data files.

def osr_basic_13():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4328 )

    expected_wkt = 'GEOCCS["WGS 84 (geocentric)",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Geocentric X",OTHER],AXIS["Geocentric Y",OTHER],AXIS["Geocentric Z",NORTH],AUTHORITY["EPSG","4328"]]'
    wkt = srs.ExportToWkt()

    if wkt != expected_wkt:
        gdaltest.post_reason( 'did not get expected GEOCCS WKT.' )
        print(wkt)
        return 'fail'

    if not srs.IsGeocentric():
        gdaltest.post_reason( 'srs not recognised as geocentric.' )
        return 'fail'

    if srs.Validate() != 0:
        gdaltest.post_reason( 'epsg geoccs import does not validate!' )
        return 'fail'
    
    return 'success'

###############################################################################
# Manually setup a simple geocentric/wgs84 srs.

def osr_basic_14():

    srs = osr.SpatialReference()
    srs.SetGeocCS( 'My Geocentric' )
    srs.SetWellKnownGeogCS( 'WGS84' )
    srs.SetLinearUnits( 'meter', 1.0 )

    expected_wkt = 'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]'
    wkt = srs.ExportToWkt()

    if wkt != expected_wkt:
        gdaltest.post_reason( 'did not get expected GEOCCS WKT.' )
        print(wkt)
        return 'fail'

    if not srs.IsGeocentric():
        gdaltest.post_reason( 'srs not recognised as geocentric.' )
        return 'fail'

    if srs.Validate() != 0:
        gdaltest.post_reason( 'geocentric srs not recognised as valid.' )
        return 'fail'

    return 'success'

###############################################################################
# Test validation and fixup methods.

def osr_basic_15():

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
    srs.SetFromUserInput( wkt )

    if srs.Validate() == 0:
        gdaltest.post_reason( 'Validate() fails to detect misordering.' )
        return 'fail'

    srs.Fixup()

    if srs.Validate() != 0:
        gdaltest.post_reason( 'Fixup() failed to correct srs.' )
        return 'fail'

    return 'success'

###############################################################################
# Test OSRSetGeocCS()

def osr_basic_16():

    # Nominal test : change citation of a GEOCCS
    srs = osr.SpatialReference()
    srs.SetFromUserInput( """GEOCCS["WGS 84 (geocentric)",
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    AXIS["Geocentric X",OTHER],
    AXIS["Geocentric Y",OTHER],
    AXIS["Geocentric Z",OTHER],
    AUTHORITY["EPSG","4328"]]""" )
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
        gdaltest.post_reason( 'Did not get expected result.' )
        return 'fail'

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
        gdaltest.post_reason( 'Did not get expected result.' )
        return 'fail'

    # Error expected. Cannot work on a PROJCS
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ret = srs.SetGeocCS("a")
    if ret == 0:
        gdaltest.post_reason('expected failure')
        print(srs)
        return 'fail'

    # Limit test : build GEOCCS from an invalid GEOGCS
    srs = osr.SpatialReference()
    srs.SetFromUserInput( """GEOGCS["foo"]""" )
    srs.SetGeocCS("bar")
    expect_wkt = """GEOCCS["bar"]"""
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        gdaltest.post_reason( 'Did not get expected result.' )
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [ 
    osr_basic_1,
    osr_basic_2,
    osr_basic_3,
    osr_basic_4,
    osr_basic_5,
    osr_basic_6,
    osr_basic_7,
    osr_basic_8,
    osr_basic_9,
    osr_basic_10,
    osr_basic_11,
    osr_basic_12,
    osr_basic_13,
    osr_basic_14,
    osr_basic_15,
    osr_basic_16,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_basic' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

