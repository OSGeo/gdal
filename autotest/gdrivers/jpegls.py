#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  JPEGLS Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

def jpegls_1():

    try:
        if gdal.GetDriverByName('JPEGLS') is None:
            return 'skip'
    except:
        return 'skip'


    tst = gdaltest.GDALTest( 'JPEGLS', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################

def jpegls_2():

    try:
        if gdal.GetDriverByName('JPEGLS') is None:
            return 'skip'
    except:
        return 'skip'


    tst = gdaltest.GDALTest( 'JPEGLS', 'int16.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

gdaltest_list = [
    jpegls_1,
    jpegls_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'JPEGLS' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

