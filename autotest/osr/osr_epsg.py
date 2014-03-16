#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test aspects of EPSG code lookup.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr
import string

###############################################################################
#	Verify that EPSG:26591 picks up the entry from the pcs.override.csv
# 	file with the adjusted central_meridian.

def osr_epsg_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 26591 )

    if abs(srs.GetProjParm('central_meridian') - -3.4523333333333) > 0.000005:
        gdaltest.post_reason( 'Wrong central meridian, override missed?' )
        print(srs.ExportToPrettyWkt())
        return 'fail'
    
    return 'success'

###############################################################################
#	Check that EPSG:4312 lookup has the towgs84 values set properly
#	from gcs.override.csv.

def osr_epsg_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4312 )

    if abs(float(srs.GetAttrValue( 'TOWGS84', 6)) \
           - 2.4232) > 0.0005:
        gdaltest.post_reason( 'Wrong TOWGS84, override missed?' )
        print(srs.ExportToPrettyWkt())
        return 'fail'
    
    return 'success'

###############################################################################
#	Check that various EPSG lookups based on Pulvoko 1942 have the
#       towgs84 values set properly (#3579)

def osr_epsg_3():
    
    for epsg in [3120,2172,2173,2174,2175,3333,3334,3335,3329,3330,3331,3332,3328,4179]:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG( epsg )
    
        expected_towgs84 = [33.4,-146.6,-76.3,-0.359,-0.053,0.844,-0.84]
    
        for i in range(6):
            if abs(float(srs.GetAttrValue( 'TOWGS84', i)) \
                - expected_towgs84[i]) > 0.0005:
                gdaltest.post_reason( 'For EPSG:%d. Wrong TOWGS84, override missed?' % epsg )
                print(srs.ExportToPrettyWkt())
                return 'fail'
    
    return 'success'

###############################################################################
#   Check that EPSG:4326 is *not* considered as lat/long (#3813)

def osr_epsg_4():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    if srs.EPSGTreatsAsLatLong() :
        gdaltest.post_reason('not supposed to be treated as lat/long')
        return 'fail'

    if srs.ExportToWkt().find('AXIS') != -1:
        gdaltest.post_reason('should not have AXIS node')
        return 'fail'

    return 'success'

###############################################################################
#   Check that EPSGA:4326 is considered as lat/long

def osr_epsg_5():

    srs = osr.SpatialReference()
    srs.ImportFromEPSGA( 4326 )

    if not srs.EPSGTreatsAsLatLong() :
        gdaltest.post_reason('supposed to be treated as lat/long')
        return 'fail'

    if srs.ExportToWkt().find('AXIS') == -1:
        gdaltest.post_reason('should  have AXIS node')
        return 'fail'

    return 'success'

###############################################################################
#   Test datum shift for OSGB 36

def osr_epsg_6():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4277 )

    if srs.ExportToWkt().find('TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489]') == -1:
        gdaltest.post_reason('did not get expected TOWGS84')
        print(srs.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
#   Check that EPSG:2193 is *not* considered as N/E

def osr_epsg_7():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 2193 )

    if srs.EPSGTreatsAsNorthingEasting() :
        gdaltest.post_reason('not supposed to be treated as n/e')
        return 'fail'

    if srs.ExportToWkt().find('AXIS') != -1:
        gdaltest.post_reason('should not have AXIS node')
        return 'fail'

    return 'success'

###############################################################################
#   Check that EPSGA:2193 is considered as N/E

def osr_epsg_8():

    srs = osr.SpatialReference()
    srs.ImportFromEPSGA( 2193 )

    if not srs.EPSGTreatsAsNorthingEasting() :
        gdaltest.post_reason('supposed to be treated as n/e')
        return 'fail'

    if srs.ExportToWkt().find('AXIS') == -1:
        gdaltest.post_reason('should  have AXIS node')
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [ 
    osr_epsg_1,
    osr_epsg_2,
    osr_epsg_3,
    osr_epsg_4,
    osr_epsg_5,
    osr_epsg_6,
    osr_epsg_7,
    osr_epsg_8,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_epsg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

