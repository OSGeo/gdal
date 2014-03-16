#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GRIB driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Do a simple checksum on our test file (with a faked imagery.tif).

def grib_1():

    gdaltest.grib_drv = gdal.GetDriverByName('GRIB')
    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'ds.mint.bin', 2, 46927 )
    return tst.testOpen()


###############################################################################
# Test a small GRIB 1 sample file.

def grib_2():

    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'Sample_QuikSCAT.grb', 4, 50714 )
    return tst.testOpen()

###############################################################################
# This file has different raster sizes for some of the products, which
# we sort-of-support per ticket Test a small GRIB 1 sample file.

def grib_3():

    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'bug3246.grb', 4, 4081 )
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    msg = gdal.GetLastErrorMsg()
    if msg.find('data access may be incomplete') == -1 \
       or gdal.GetLastErrorType() != 2:
        gdaltest.post_reason( 'did not get expected warning.' )
    
    return result

###############################################################################
# Check nodata

def grib_4():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/ds.mint.bin')
    if ds.GetRasterBand(1).GetNoDataValue() != 9999:
        return 'fail'

    return 'success'

gdaltest_list = [
    grib_1,
    grib_2,
    grib_3,
    grib_4
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'grib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

