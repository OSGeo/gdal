#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some PROJ.4 specific translation issues.
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
import string
import gdal

sys.path.append( '../pymod' )

import gdaltest
import osr
import ogr

###############################################################################
# Test the the +k_0 flag works as well as +k when consuming PROJ.4 format.
# This is from Bugzilla bug 355.
#

def osr_proj4_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromProj4( '+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000 +k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000 +a=6377340.189000 +rf=299.324965 +towgs84=482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15' )

    if abs(srs.GetProjParm( osr.SRS_PP_SCALE_FACTOR )-1.000035) > 0.0000005:
        gdaltest.post_reason( '+k_0 not supported on import from PROJ.4?' )
        return 'fail'

    return 'success'

###############################################################################
# Verify that we can import strings with parameter values that are exponents
# and contain a plus sign.  As per bug 355 in GDAL/OGR's bugzilla. 
#

def osr_proj4_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromProj4( "+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0 +lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n +a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat" )

    if abs(srs.GetProjParm( osr.SRS_PP_FALSE_EASTING )-609601.219) > 0.0005:
        gdaltest.post_reason( 'Parsing exponents not supported?' )
        return 'fail'

    return 'success'

###############################################################################
# Verify that empty srs'es don't cause a crash (#1718).
#

def osr_proj4_3():
    
    srs = osr.SpatialReference()

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        proj4 = srs.ExportToProj4()
        gdal.PopErrorHandler()
        
    except RuntimeError:
        gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('No translation') != -1:
        return 'success'

    gdaltest.post_reason( 'empty srs not handled properly' )
    return 'fail'

###############################################################################
# Verify that unrecognised projections return an error, not those
# annoying ellipsoid-only results.
#

def osr_proj4_4():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( '+proj=utm +zone=11 +datum=WGS84' )
    srs.SetAttrValue( 'PROJCS|PROJECTION', 'FakeTransverseMercator' )
    
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        proj4 = srs.ExportToProj4()
        gdal.PopErrorHandler()
        
    except RuntimeError:
        gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('No translation') != -1:
        return 'success'

    gdaltest.post_reason( 'unknown srs not handled properly' )
    return 'fail'

###############################################################################
# Verify that prime meridians are preserved when round tripping. (#1940)
#

def osr_proj4_5():
    
    srs = osr.SpatialReference()

    srs.ImportFromProj4( '+proj=lcc +lat_1=46.8 +lat_0=46.8 +lon_0=0 +k_0=0.99987742 +x_0=600000 +y_0=2200000 +a=6378249.2 +b=6356515 +towgs84=-168,-60,320,0,0,0,0 +pm=paris +units=m +no_defs' )

    if abs(float(srs.GetAttrValue('PRIMEM',1)) - 2.3372291667) > 0.00000001:
        gdaltest.post_reason('prime meridian lost?')
        return 'fail'

    if abs(srs.GetProjParm('central_meridian')) != 0.0:
        gdaltest.post_reason( 'central meridian altered?' )
        return 'fail'

    p4 = srs.ExportToProj4()
    srs2 = osr.SpatialReference()
    srs2.ImportFromProj4( p4 )

    if not srs.IsSame(srs2):
        gdaltest.post_reason( 'round trip via PROJ.4 damaged srs?' )
        print(srs.ExportToPrettyWkt())
        print(srs2.ExportToPrettyWkt())
    
    return 'success'

###############################################################################
# Confirm handling of non-zero latitude of origin mercator (#3026)
#

def osr_proj4_6():

    expect_proj4 = '+proj=merc +lon_0=0 +lat_ts=46.1333331 +x_0=1000 +y_0=2000 +ellps=WGS84 +datum=WGS84 +units=m +no_defs '
    
    wkt = """PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",46.1333331],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",1000],
    PARAMETER["false_northing",2000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    
    srs = osr.SpatialReference()
    srs.ImportFromWkt(wkt)
    proj4 = srs.ExportToProj4()

    if proj4 != expect_proj4:
        print('Got:', proj4)
        print('Expected:', expect_proj4)
        gdaltest.post_reason( 'Failed to translate non-zero lat-of-origin mercator.' )
        return 'fail'

    # Translate back - should be mercator 1sp

    expect_wkt = """PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",46.1333331],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",1000],
    PARAMETER["false_northing",2000],
    UNIT["Meter",1]]"""

    srs.SetFromUserInput( proj4 )
    wkt = srs.ExportToPrettyWkt()
    if wkt != expect_wkt:
        print('Got:   ',wkt)
        print('Expect:',expect_wkt)
        gdaltest.post_reason( 'did not get expected mercator_2sp result.' )
        return 'fail'

    return 'success'

###############################################################################
# Confirm handling of somerc (#3032).
#

def osr_proj4_7():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 23700 )

    proj4 = srs.ExportToProj4()
    expected = '+proj=somerc +lat_0=47.14439372222222 +lon_0=19.04857177777778 +k_0=0.99993 +x_0=650000 +y_0=200000 +ellps=GRS67 +towgs84=52.17,-71.82,-14.9,0,0,0,0 +units=m +no_defs '
    if proj4 != expected:
        gdaltest.post_reason( 'did not get expected proj.4 translation of somerc' )
        print('')
        print('Got:     "%s"' % proj4)
        print('Expected:"%s"' % expected)
        return 'fail'

    srs.ImportFromProj4( proj4 )
    
    expected = """PROJCS["unnamed",
    GEOGCS["GRS 67(IUGG 1967)",
        DATUM["unknown",
            SPHEROID["GRS67",6378160,298.247167427]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Hotine_Oblique_Mercator"],
    PARAMETER["latitude_of_center",47.14439372222222],
    PARAMETER["longitude_of_center",19.04857177777778],
    PARAMETER["azimuth",90],
    PARAMETER["rectified_grid_angle",90],
    PARAMETER["scale_factor",0.99993],
    PARAMETER["false_easting",650000],
    PARAMETER["false_northing",200000],
    UNIT["Meter",1]]"""
    
    srs_expected = osr.SpatialReference( wkt = expected )
    if not srs.IsSame(srs_expected):
        gdaltest.post_reason( 'did not get expected wkt.' )
        print('Got: ', srs.ExportToPrettyWkt())
        return 'fail'
    
    return 'success'

###############################################################################
# Check EPSG:3857, confirm google mercator hackery.
#

def osr_proj4_8():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 3857 )

    proj4 = srs.ExportToProj4()
    expected = '+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs'
    if proj4 != expected:
        gdaltest.post_reason( 'did not get expected EPSG:3857 (google mercator) result.' )
        print(proj4)
        return 'fail'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 3785 )

    proj4 = srs.ExportToProj4()
    if proj4 != expected:
        gdaltest.post_reason( 'did not get expected EPSG:3785 (google mercator) result.' )
        print(proj4)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_proj4_1,
    osr_proj4_2,
    osr_proj4_3,
    osr_proj4_4,
    osr_proj4_5,
    osr_proj4_6,
    osr_proj4_7,
    osr_proj4_8,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_proj4' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

