#!/usr/bin/env python
###############################################################################
# $Id: jpipkak.py 18659 2010-01-25 03:39:15Z warmerdam $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test reading with JPIPKAK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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
# Read test of simple byte reference data.

def jpipkak_1():

    return 'skip'

    gdaltest.jpipkak_drv = gdal.GetDriverByName( 'JPIPKAK' )
    if gdaltest.jpipkak_drv is None:
        return 'skip'

    ds = gdal.Open( 'jpip://216.150.195.220/JP2Server/qb_boulder_msi_uint')
    if ds is None:
        gdaltest.post_reason( 'failed to open jpip stream.' )
        return 'fail'

    target = ds.GetRasterBand(3).GetOverview(3)

    stats = target.GetStatistics(0,1)

    if abs(stats[2] - 6791.121) > 1.0 or abs(stats[3]-3046.536) > 1.0:
        print( stats )
        gdaltest.post_reason( 'did not get expected mean/stddev' )
        return 'fail'

    return 'success'

###############################################################################
# 

def jpipkak_2():

    return 'skip'

    if gdaltest.jpipkak_drv is None:
        return 'skip'

    
    ds = gdal.Open( 'jpip://216.150.195.220/JP2Server/qb_boulder_pan_byte' )
    if ds is None:
        gdaltest.post_reason( 'failed to open jpip stream.' )
        return 'fail'

    wkt = ds.GetProjectionRef()
    exp_wkt = 'PROJCS["WGS 84 / UTM zone 13N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-105],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","32613"]]'
    
    if not gdaltest.equal_srs_from_wkt( exp_wkt, wkt ):
        return 'fail'
    
    target = ds.GetRasterBand(1).GetOverview(3)

    stats = target.GetStatistics(0,1)

    if abs(stats[2] - 43.429) > 1.0 or abs(stats[3]-18.526) > 1.0:
        print( stats )
        gdaltest.post_reason( 'did not get expected mean/stddev' )
        return 'fail'

    return 'success'

###############################################################################
# Test an 11bit image. 

def jpipkak_3():

    return 'skip'

    if gdaltest.jpipkak_drv is None:
        return 'skip'

    
    ds = gdal.Open( 'jpip://216.150.195.220/JP2Server/qb_boulder_pan_11bit' )
    if ds is None:
        gdaltest.post_reason( 'failed to open jpip stream.' )
        return 'fail'

    target = ds.GetRasterBand(1)

    stats = target.GetStatistics(0,1)
    
    if abs(stats[2] - 483.501) > 1.0 or abs(stats[3]-117.972) > 1.0:
        print( stats )
        gdaltest.post_reason( 'did not get expected mean/stddev' )
        return 'fail'

    return 'success'

###############################################################################
# Test a 20bit image, reduced to 16bit during processing.

def jpipkak_4():

    return 'skip'

    if gdaltest.jpipkak_drv is None:
        return 'skip'

    
    ds = gdal.Open( 'jpip://216.150.195.220/JP2Server/qb_boulder_pan_20bit' )
    if ds is None:
        gdaltest.post_reason( 'failed to open jpip stream.' )
        return 'fail'

    target = ds.GetRasterBand(1)

    stats = target.GetStatistics(0,1)
    
    if abs(stats[2] - 5333.148) > 1.0 or abs(stats[3]-2522.023) > 1.0:
        print( stats )
        gdaltest.post_reason( 'did not get expected mean/stddev' )
        return 'fail'

    return 'success'

###############################################################################
# Test an overview level that will result in multiple fetches with subwindows.

def jpipkak_5():

    return 'skip'

    if gdaltest.jpipkak_drv is None:
        return 'skip'

    
    ds = gdal.Open( 'jpip://216.150.195.220/JP2Server/qb_boulder_pan_byte' )
    if ds is None:
        gdaltest.post_reason( 'failed to open jpip stream.' )
        return 'fail'

    target = ds.GetRasterBand(1).GetOverview(1)

    stats = target.GetStatistics(0,1)

    if abs(stats[2] - 42.462) > 1.0 or abs(stats[3]-20.611) > 1.0:
        print( stats )
        gdaltest.post_reason( 'did not get expected mean/stddev' )
        return 'fail'

    return 'success'

gdaltest_list = [
    jpipkak_1,
    jpipkak_2,
    jpipkak_3,
    jpipkak_4,
    jpipkak_5
 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jpipkak' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

