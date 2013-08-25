#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for BT driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
import array
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test CreateCopy() of int16.tif

def bt_1():

    tst = gdaltest.GDALTest( 'BT', 'int16.tif', 1, 4672 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testCreateCopy( vsimem = 1, check_srs = srs.ExportToWkt(),
                               check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test CreateCopy() of int32.tif

def bt_2():

    tst = gdaltest.GDALTest( 'BT', 'int32.tif', 1, 4672 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testCreateCopy( check_srs = srs.ExportToWkt(),
                               check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test CreateCopy() of float32.tif

def bt_3():

    tst = gdaltest.GDALTest( 'BT', 'float32.tif', 1, 4672 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testCreateCopy( check_srs = srs.ExportToWkt(),
                               check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test Create() of float32.tif

def bt_4():

    tst = gdaltest.GDALTest( 'BT', 'float32.tif', 1, 4672 )
    return tst.testCreate( out_bands = 1 )

###############################################################################
# Test testSetProjection() of float32.tif

def bt_5():

    tst = gdaltest.GDALTest( 'BT', 'float32.tif', 1, 4672 )
    return tst.testSetProjection()

###############################################################################
# Test testSetGeoTransform() of float32.tif

def bt_6():

    tst = gdaltest.GDALTest( 'BT', 'float32.tif', 1, 4672 )
    return tst.testSetGeoTransform()

###############################################################################
# Cleanup

def bt_cleanup():

    try:
        os.remove('tmp/int16.tif.prj')
        os.remove('tmp/int32.tif.prj')
        os.remove('tmp/float32.tif.prj')
    except:
        pass
    return 'success'


gdaltest_list = [
    bt_1,
    bt_2,
    bt_3,
    bt_4,
    bt_5,
    bt_6,
    bt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'bt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

