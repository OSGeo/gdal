#!/usr/bin/env python
###############################################################################
# $Id: png.py,v 1.4 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PNG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: png.py,v $
#  Revision 1.4  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.3  2005/09/15 02:36:24  fwarmerdam
#  added RGB nodata test
#
#  Revision 1.2  2005/09/12 19:00:27  fwarmerdam
#  added 16bit rgba test case
#
#  Revision 1.1  2004/08/26 21:36:16  warmerda
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of simple byte reference data.

def png_1():

    tst = gdaltest.GDALTest( 'PNG', 'test.png', 1, 57921 )
    return tst.testOpen()

###############################################################################
# Test lossless copying.

def png_2():

    tst = gdaltest.GDALTest( 'PNG', 'test.png', 1, 57921 )

    return tst.testCreateCopy()
    
###############################################################################
# Verify the geotransform, colormap, and nodata setting for test file. 

def png_3():

    ds = gdal.Open( 'data/test.png' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 16 \
       or cm.GetColorEntry(0) != (255,255,255,0) \
       or cm.GetColorEntry(1) != (255,255,208,255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if int(ds.GetRasterBand(1).GetNoDataValue()) != 0:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    # This geotransform test is also verifying the fix for bug 1414, as
    # the world file is in a mixture of numeric representations for the
    # numbers.  (mixture of "," and "." in file)

    gt_expected = (700000.305, 0.38, 0.01, 4287500.695, -0.01, -0.38)

    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - gt_expected[i]) > 0.0001:
            print 'expected:', gt_expected
            print 'got:', gt
            
            gdaltest.post_reason( 'Mixed locale world file read improperly.' )
            return 'fail'

    return 'success'
    
###############################################################################
# Test RGB mode creation and reading.

def png_4():

    tst = gdaltest.GDALTest( 'PNG', 'rgb.ntf', 3, 21349 )

    return tst.testCreateCopy()

###############################################################################
# Test RGBA 16bit read support.

def png_5():

    tst = gdaltest.GDALTest( 'PNG', 'rgba16.png', 3, 1815 )
    return tst.testOpen()

###############################################################################
# Test RGBA 16bit mode creation and reading.

def png_6():

    tst = gdaltest.GDALTest( 'PNG', 'rgba16.png', 4, 4873 )

    return tst.testCreateCopy()

###############################################################################
# Test RGB NODATA_VALUES metadata write (and read) support.
# This is handled via the tRNS block in PNG.

def png_7():

    drv = gdal.GetDriverByName( 'PNG' )
    srcds = gdal.Open( 'data/tbbn2c16.png' )
    
    dstds = drv.CreateCopy( 'tmp/png7.png', srcds )
    srcds = None

    dstds = gdal.Open( 'tmp/png7.png' )
    md = dstds.GetMetadata()
    dstds = None

    if md['NODATA_VALUES'] != '32639 32639 32639':
        gdaltest.post_reason( 'NODATA_VALUES wrong' )
        return 'fail'

    return 'success'


###############################################################################
# Cleanup.

def png_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    png_1,
    png_2,
    png_3,
    png_4,
    png_5,
    png_6,
    png_7,
    png_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'png' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

