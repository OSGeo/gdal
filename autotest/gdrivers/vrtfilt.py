#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRT driver based filtering. 
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
# Verify simple 3x3 averaging filter. 

def vrtfilt_1():

    tst = gdaltest.GDALTest( 'VRT', 'avfilt.vrt', 1, 21890 )
    return tst.testOpen()

###############################################################################
# Verify simple 3x3 averaging filter (normalized) on a dataset with nodata

def vrtfilt_2():

    ds = gdal.Open('data/test_vrt_filter_nodata.tif')
    checksum = ds.GetRasterBand(1).Checksum()
    ds = None

    # This is a black&white checkboard, where black = nodata
    # Thus averaging it and taking nodata into account will not change it
    tst = gdaltest.GDALTest( 'VRT', 'avfilt_nodata.vrt', 1, checksum )
    return tst.testOpen()

###############################################################################
# Cleanup.

def vrtfilt_cleanup():
    return 'success'

gdaltest_list = [
    vrtfilt_1,
    vrtfilt_2,
    vrtfilt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtfilt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

