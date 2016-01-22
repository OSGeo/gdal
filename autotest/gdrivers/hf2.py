#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for HF2 driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
# 
# Permission is hereby granted, free of charge, to any person ohf2aining a
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
import struct
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test CreateCopy() of byte.tif

def hf2_1():

    tst = gdaltest.GDALTest( 'HF2', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1, check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test CreateCopy() of byte.tif with options

def hf2_2():

    tst = gdaltest.GDALTest( 'HF2', 'byte.tif', 1, 4672, options = ['COMPRESS=YES', 'BLOCKSIZE=10'] )
    ret = tst.testCreateCopy( new_filename = 'tmp/hf2_2.hfz' )
    try:
        os.remove('tmp/hf2_2.hfz.properties')
    except:
        pass
    return ret

###############################################################################
# Test CreateCopy() of float.img

def hf2_3():

    tst = gdaltest.GDALTest( 'HF2', 'float.img', 1, 23529 )
    return tst.testCreateCopy( check_minmax = 0 )

###############################################################################
# Test CreateCopy() of n43.dt0

def hf2_4():

    tst = gdaltest.GDALTest( 'HF2', 'n43.dt0', 1, 49187 )
    return tst.testCreateCopy()

###############################################################################
# Cleanup

def hf2_cleanup():

    return 'success'


gdaltest_list = [
    hf2_1,
    hf2_2,
    hf2_3,
    hf2_4,
    hf2_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'hf2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

