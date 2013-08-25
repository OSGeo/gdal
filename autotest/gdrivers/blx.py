#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test BLX support.
# Author:   Even Rouault < even dot rouault @ mines-paris dot org >
# 
###############################################################################
# Copyright (c) 2008, Even Rouault < even dot rouault @ mines-paris dot org >
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test reading a little-endian BLX

def blx_1():

    prj = 'WGS84'
    gt = [ 20.0004166, 0.0008333, 0.0, 50.0004166, 0.0, -0.0008333 ]
    tst = gdaltest.GDALTest( 'BLX', 's4103.blx', 1, 47024 )
    return tst.testOpen( check_prj = prj, check_gt = gt )


###############################################################################
# Test reading a big-endian BLX

def blx_2():

    prj = 'WGS84'
    gt = [ 20.0004166, 0.0008333, 0.0, 50.0004166, 0.0, -0.0008333 ]
    tst = gdaltest.GDALTest( 'BLX', 's4103.xlb', 1, 47024 )
    return tst.testOpen( check_prj = prj, check_gt = gt )


###############################################################################
# Test writing a little-endian BLX

def blx_3():

    tst = gdaltest.GDALTest( 'BLX', 's4103.xlb', 1, 47024 )
    return tst.testCreateCopy( check_gt = 1, check_srs = 1 )


###############################################################################
# Test writing a big-endian BLX

def blx_4():

    tst = gdaltest.GDALTest( 'BLX', 's4103.blx', 1, 47024, options = [ 'BIGENDIAN=YES' ] )
    return tst.testCreateCopy( check_gt = 1, check_srs = 1 )


###############################################################################
# Test overviews

def blx_5():

    ds = gdal.Open( 'data/s4103.blx' )

    band = ds.GetRasterBand(1)
    if band.GetOverviewCount() != 4:
        gdaltest.post_reason( 'did not get expected overview count' )
        return 'fail'

    cs = band.GetOverview(0).Checksum()
    if cs != 42981:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    cs = band.GetOverview(1).Checksum()
    if cs != 61363:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    cs = band.GetOverview(2).Checksum()
    if cs != 48060:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    cs = band.GetOverview(3).Checksum()
    if cs != 12058:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    return 'success'



gdaltest_list = [
    blx_1,
    blx_2,
    blx_3,
    blx_4,
    blx_5
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'blx' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

