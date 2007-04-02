#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EOSAT FAST Format support.
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def fast_1():
    
    try:
        gdaltest.fast_drv = gdal.GetDriverByName( 'FAST' )
    except:
        gdaltest.fast_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Perform simple read test.

def fast_2():

    if gdaltest.fast_drv is None:
        return 'skip'

    # Actually, the band (a placeholder) is of 0 bytes size,
    # so the checksum is 0 expected.

    tst = gdaltest.GDALTest( 'fast', 'L71118038_03820020111_HPN.FST', 1, 0,
                             100, 100, 10, 10 )
    return tst.testOpen()

###############################################################################
# Verify metadata.

def fast_3():

    if gdaltest.fast_drv is None:
        return 'skip'

    gdaltest.fast_ds = gdal.Open( 'data/L71118038_03820020111_HPN.FST' )
    ds = gdaltest.fast_ds
    if ds is None:
        gdaltest.post_reason( 'Missing test dataset' )
        return 'fail'

    md = ds.GetMetadata()
    if md is None:
        gdaltest.post_reason( 'Missing metadata in test dataset' )
        return 'fail'

    if md['ACQUISITION_DATE'] != '20020111':
        gdaltest.post_reason( 'ACQUISITION_DATE wrong' )
        return 'fail'

    if md['SATELLITE'] != 'LANDSAT7':
        gdaltest.post_reason( 'SATELLITE wrong' )
        return 'fail'

    if md['SENSOR'] != 'ETM+':
        gdaltest.post_reason( 'SENSOR wrong' )
        return 'fail'

    # GAIN and BIAS expected values
    gb_expected = ( -6.199999809265137, 0.775686297697179 )

    gain = float(md['GAIN1'])
    if abs(gain - gb_expected[0]) > 0.0001:
            print 'expected:', gb_expected[0]
            print 'got:', gain

    bias = float(md['BIAS1'])
    if abs(bias - gb_expected[1]) > 0.0001:
            print 'expected:', gb_expected[1]
            print 'got:', bias

    return 'success'

###############################################################################
# Test geotransform data.

def fast_4():

    if gdaltest.fast_drv is None:
        return 'skip'

    ds = gdaltest.fast_ds
    if ds is None:
        gdaltest.post_reason( 'Missing test dataset' )
        return 'fail'

    gt = ds.GetGeoTransform()
    ds = None

    tolerance = 0.01
    if abs(gt[0] - 280342.5) > tolerance or abs(gt[1] - 15.0) > tolerance or \
       abs(gt[2] - 0.0) > tolerance or abs(gt[3] - 3621457.5) > tolerance or \
       abs(gt[4] - 0.0) > tolerance or abs(gt[5] + 15.0) > tolerance:
        gdaltest.post_reason( 'FAST geotransform wrong' )
        return 'fail'

    return 'success'


###############################################################################
# Test 2 bands dataset with checking projections and geotransform.

def fast_5():

    if gdaltest.fast_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'fast', 'L71230079_07920021111_HTM.FST', 2, 0,
                             100, 100, 10, 10 )
    
    # Expected parameters of the geotransform
    gt = (528417.25, 30.0, 0.0, 7071187.0, 0.0, -30.0)
    
    # Expected definition of the projection
    proj = """PROJCS["unnamed",
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
            AXIS["Lat",NORTH],
            AXIS["Long",EAST],
            AUTHORITY["EPSG","4326"]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-66],
        PARAMETER["scale_factor",1],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",10002288.3],
        UNIT["Meter",1]]"""
    
    return tst.testOpen( check_gt = gt, check_prj = proj )

###############################################################################
#

gdaltest_list = [
    fast_1,
    fast_2,
    fast_3,
    fast_4,
    fast_5 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'fast' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

