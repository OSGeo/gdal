#!/usr/bin/env python
###############################################################################
# $Id: dods.py,v 1.2 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DODS raster access.
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
#  $Log: dods.py,v $
#  Revision 1.2  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.1  2006/05/02 17:51:26  fwarmerdam
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Open DODS datasource.

def dods_1():
    gdaltest.dods_ds = None
    gdaltest.dods_dr = None
    
    try:
        gdaltest.dods_dr = gdal.GetDriverByName( 'DODS' )
    except:
        return 'skip'

    if gdaltest.dods_dr is None:
        return 'skip'

    gdaltest.dods_grid_ds = gdal.Open( 'http://ingrid.ldeo.columbia.edu/SOURCES/.ARCTIC/.STATION/dods?Theta[x][y]')

    if gdaltest.dods_grid_ds is None:
        gdaltest.dods_dr = None
        return 'fail'

    return 'success'

###############################################################################
# Simple read test on a single variable.

def dods_2():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'dods', 'http://ingrid.ldeo.columbia.edu/SOURCES/.ARCTIC/.STATION/dods?Theta', 1, 51213, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Access all grids at once.

def dods_3():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'dods', 'http://ingrid.ldeo.columbia.edu/SOURCES/.ARCTIC/.STATION/dods', 12, 51213, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Test explicit transpose.

def dods_4():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'dods', 'http://ingrid.ldeo.columbia.edu/SOURCES/.ARCTIC/.STATION/dods?Theta[x][y]', 1, 51985, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Test explicit flipping.

def dods_5():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'dods', 'http://ingrid.ldeo.columbia.edu/SOURCES/.ARCTIC/.STATION/dods?Theta[y][-x]', 1, 52851, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Check nodata value.

def dods_6():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    nd = gdaltest.dods_grid_ds.GetRasterBand(1).GetNoDataValue()
    if nd != -9999:
        gdaltest.post_reason( 'nodata value wrong or missing.' )
        print nd
        return 'fail'
    else:
        return 'success'

###############################################################################
# Cleanup

def dods_cleanup():
    if gdaltest.dods_dr is None:
        return 'skip'
    
    gdaltest.dods_dr = None
    gdaltest.dods_grid_ds = None

    return 'success'
    
gdaltest_list = [
    dods_1,
    dods_2,
    dods_3,
    dods_4,
    dods_5,
    dods_6,
    dods_cleanup ]


if __name__ == '__main__':

    gdaltest.setup_run( 'dods' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

