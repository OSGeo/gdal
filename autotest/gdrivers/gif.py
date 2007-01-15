#!/usr/bin/env python
###############################################################################
# $Id: gif.py,v 1.5 2006/11/29 18:35:55 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GIF driver.
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
#  $Log: gif.py,v $
#  Revision 1.5  2006/11/29 18:35:55  fwarmerdam
#  Fixed up cleanup function.
#
#  Revision 1.4  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.3  2005/10/13 01:37:47  fwarmerdam
#  added /vsimem/ test
#
#  Revision 1.2  2005/08/05 17:48:20  fwarmerdam
#  fixed gif background check
#
#  Revision 1.1  2004/02/10 07:56:32  warmerda
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

def gif_1():

    tst = gdaltest.GDALTest( 'GIF', 'bug407.gif', 1, 57921 )
    return tst.testOpen()

###############################################################################
# Test lossless copying.

def gif_2():

    tst = gdaltest.GDALTest( 'GIF', 'bug407.gif', 1, 57921,
                             options = [ 'INTERLACING=NO' ] )

    return tst.testCreateCopy()
    
###############################################################################
# Verify the colormap, and nodata setting for test file. 

def gif_3():

    ds = gdal.Open( 'data/bug407.gif' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 16 \
       or cm.GetColorEntry(0) != (255,255,255,255) \
       or cm.GetColorEntry(1) != (255,255,208,255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if ds.GetRasterBand(1).GetNoDataValue() is not None:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if not md.has_key('GIF_BACKGROUND') or md['GIF_BACKGROUND'] != '0':
        print md
        gdaltest.post_reason( 'background metadata missing.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test creating an in memory copy.

def gif_4():

    tst = gdaltest.GDALTest( 'GIF', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Cleanup.

def gif_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    gif_1,
    gif_2,
    gif_3,
    gif_4,
    gif_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'gif' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

