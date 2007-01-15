#!/usr/bin/env python
###############################################################################
# $Id: pcidsk.py,v 1.2 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PCIDSK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: pcidsk.py,v $
#  Revision 1.2  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.1  2005/10/17 02:25:53  fwarmerdam
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of floating point reference data.

def pcidsk_1():

    tst = gdaltest.GDALTest( 'PCIDSK', 'utm.pix', 1, 39576 )
    return tst.testOpen()

###############################################################################
# Test lossless copying (16, multiband) via Create().

def pcidsk_2():

    tst = gdaltest.GDALTest( 'PCIDSK', 'rgba16.png', 2, 2042 )

    return tst.testCreate()
    
###############################################################################
# Test copying of georeferencing and projection.

def pcidsk_3():

    tst = gdaltest.GDALTest( 'PCIDSK', 'utm.pix', 1, 39576 )

    return tst.testCreateCopy( check_gt = 1, check_srs = 1 )

###############################################################################
# Test overview reading.

def pcidsk_4():

    ds = gdal.Open( 'data/utm.pix' )

    band = ds.GetRasterBand(1)
    if band.GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not get expected overview count' )
        return 'fail'

    cs = band.GetOverview(0).Checksum()
    if cs != 8368:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def pcidsk_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    pcidsk_1,
    pcidsk_2,
    pcidsk_3,
    pcidsk_4,
    pcidsk_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'pcidsk' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

