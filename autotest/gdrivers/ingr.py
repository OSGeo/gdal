#!/usr/bin/env python
###############################################################################
# $Id: idrisi.py,v 1.3 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for INGR.
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
# Read test of byte file.

def ingr_1():

    tst = gdaltest.GDALTest( 'INGR', '8bit_rgb.cot', 2, 4855 )
    return tst.testOpen()

###############################################################################
# Read uint32 file.

def ingr_2():

    tst = gdaltest.GDALTest( 'INGR', 'uint32.cot', 1, 4672 )
    return tst.testOpen()

###############################################################################
# Test paletted file, including checking the palette.

def ingr_3():

    tst = gdaltest.GDALTest( 'INGR', '8bit_pal.cot', 1, 4855 )
    result = tst.testOpen()
    if result != 'success':
        return result

    ds = gdal.Open( 'data/8bit_pal.cot' )
    ct = ds.GetRasterBand(1).GetRasterColorTable()
    if ct.GetCount() != 256 or ct.GetColorEntry(8) != (8,8,8,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def ingr_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    ingr_1,
    ingr_2,
    ingr_3,
    ingr_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ingr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

