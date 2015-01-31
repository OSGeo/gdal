#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GMT driver support.
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

import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def gmt_1():

    gdaltest.gmt_drv = gdal.GetDriverByName( 'GMT' )

    if gdaltest.gmt_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GMT', 'gmt_1.grd', 1, 34762 )

    gt = (59.958333333333336,0.083333333333333,0.0,
          25.041666666666668,0.0,-0.083333333333333)
    
    return tst.testOpen( check_gt = gt )

###############################################################################
# Verify a simple createcopy operation with 16bit data.

def gmt_2():

    if gdaltest.gmt_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GMT', 'int16.tif', 1, 4672 )
    return tst.testCreateCopy( check_gt = 1 )

###############################################################################

gdaltest_list = [
    gmt_1,
    gmt_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'gmt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

