#!/usr/bin/env python
###############################################################################
# $Id: osr_basic.py,v 1.8 2006/12/18 16:21:56 fwarmerdam Exp $
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
# 
#  $Log: osr_basic.py,v $
#  Revision 1.8  2006/12/18 16:21:56  fwarmerdam
#  Added test for x-ogc as well as ogc namespace.
#
#  Revision 1.7  2006/11/28 16:39:55  mloskot
#  Fixed typo in osr_basic_4() test.
#
#  Revision 1.6  2006/11/07 18:54:05  fwarmerdam
#  Added some urn processing tests.
#
#  Revision 1.5  2006/10/27 04:34:35  fwarmerdam
#  corrected licenses
#
#  Revision 1.4  2004/07/10 04:48:39  warmerda
#  Removed some GEOGCS|UNIT stuff ... changes with EPSG rev.
#
#  Revision 1.3  2003/06/21 23:24:07  warmerda
#  added proj4 TOWGS84 check
#
#  Revision 1.2  2003/05/30 18:17:34  warmerda
#  Added a bunch of state plane testing.
#
#  Revision 1.1  2003/03/25 15:22:31  warmerda
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr


###############################################################################
# Create a UTM WGS84 coordinate system and check various items. 

def osr_basic_1():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

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

    wkt_1 = osr.GetUserInputAsWKT( 'urn:x-ogc:def:crs:EPSG::4326' )
    wkt_2 = osr.GetUserInputAsWKT( 'EPSG:4326' )
    if wkt_1 != wkt_2:
        print wkt_1
        print wkt_2
        gdaltest.post_reason( 'EPSG:4326 urn lookup not as expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Test URN support for auto projection.

def osr_basic_7():

    wkt_1 = osr.GetUserInputAsWKT( 'urn:ogc:def:crs:OGC::AUTO42001:-117:33' )
    wkt_2 = 'PROJCS["UTM Zone 11, Northern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1,AUTHORITY["EPSG","9001"]]]'
    if wkt_1 != wkt_2:
        print wkt_1
        print wkt_2
        gdaltest.post_reason( 'AUTO42001 urn lookup not as expected.' )
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_basic_1,
    osr_basic_2,
    osr_basic_3,
    osr_basic_4,
    osr_basic_5,
    osr_basic_6,
    osr_basic_7,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_basic' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

