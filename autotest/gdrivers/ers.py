#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ERS format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
# Perform simple read test.

def ers_1():

    tst = gdaltest.GDALTest( 'ERS', 'srtm.ers', 1, 64074 )
    return tst.testOpen()

###############################################################################
# Create simple copy and check.

def ers_2():

    tst = gdaltest.GDALTest( 'ERS', 'float32.bil', 1, 27 )
    return tst.testCreateCopy( new_filename = 'tmp/float32.ers',
                               check_gt = 1, vsimem = 1 )
    
###############################################################################
# Test multi-band file.

def ers_3():

    tst = gdaltest.GDALTest( 'ERS', 'rgbsmall.tif', 2, 21053 )
    return tst.testCreate( new_filename = 'tmp/rgbsmall.ers' )
    
###############################################################################
# Test HeaderOffset case.

def ers_4():

    gt = (143.62125, 0.025000000000000001, 0.0, -39.40625, 0.0, -0.025)
    
    srs = """GEOGCS["GEOCENTRIC DATUM of AUSTRALIA",
    DATUM["GDA94",
        SPHEROID["GRS80",6378137,298.257222101]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""
 
    tst = gdaltest.GDALTest( 'ERS', 'ers_dem.ers', 1, 56588 )
    return tst.testOpen( check_prj = srs, check_gt = gt )
    
###############################################################################
# Confirm we can recognised signed 8bit data.

def ers_5():

    ds = gdal.Open( 'data/8s.ers' )
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')

    if md['PIXELTYPE'] != 'SIGNEDBYTE':
        gdaltest.post_reason( 'Failed to detect SIGNEDBYTE' )
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Confirm a copy preserves the signed byte info.

def ers_6():

    drv = gdal.GetDriverByName( 'ERS' )
    
    src_ds = gdal.Open( 'data/8s.ers' )

    ds = drv.CreateCopy( 'tmp/8s.ers', src_ds )
    
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')

    if md['PIXELTYPE'] != 'SIGNEDBYTE':
        gdaltest.post_reason( 'Failed to detect SIGNEDBYTE' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/8s.ers' )
    
    return 'success'
    
###############################################################################
# Test opening a file with everything in lower case.

def ers_7():

    ds = gdal.Open( 'data/caseinsensitive.ers' )

    desc = ds.GetRasterBand(1).GetDescription()

    if desc != 'RTP 1st Vertical Derivative':
        print(desc)
        gdaltest.post_reason( 'did not get expected values.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Create simple copy and check (greyscale) using progressive option.

def ers_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    ers_1,
    ers_2,
    ers_3,
    ers_4,
    ers_5,
    ers_6,
    ers_7,
    ers_cleanup
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'ers' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

