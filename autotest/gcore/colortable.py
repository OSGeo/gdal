#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the GDALColorTable.  Mostly this tests
#           the python binding. 
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# Create a color table.

def colortable_1():

    gdaltest.test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    gdaltest.test_ct = gdal.ColorTable()
    for i in range(len(gdaltest.test_ct_data)):
        gdaltest.test_ct.SetColorEntry( i, gdaltest.test_ct_data[i] )

    return 'success' 

###############################################################################
# verify contents.

def colortable_2():

    for i in range(len(gdaltest.test_ct_data)):
        g_data = gdaltest.test_ct.GetColorEntry( i )
        o_data = gdaltest.test_ct_data[i]

        for j in range(4):
            if len(o_data) <= j:
                o_v = 255
            else:
                o_v = o_data[j]
                
            if g_data[j] != o_v:
                gdaltest.post_reason( 'color table mismatch' )
                return 'fail'

    return 'success' 

###############################################################################
# Test CreateColorRamp()

def colortable_3():

    ct = gdal.ColorTable()
    try:
        ct.CreateColorRamp
    except:
        return 'skip'

    ct.CreateColorRamp(0,(255,0,0),255,(0,0,255))

    if ct.GetColorEntry(0) != (255,0,0,255):
        return 'fail'

    if ct.GetColorEntry(255) != (0,0,255,255):
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def colortable_cleanup():
    gdaltest.test_ct = None
    return 'success'

gdaltest_list = [
    colortable_1,
    colortable_2,
    colortable_3,
    colortable_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'colortable' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

