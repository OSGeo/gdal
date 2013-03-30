#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RasterAttributeTables services from Python.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# Create a raster attribute table.

def rat_1():

    gdaltest.saved_rat = None
    
    try:
        rat = gdal.RasterAttributeTable()
    except:
        return 'skip'

    rat.CreateColumn( 'Value', gdal.GFT_Integer, gdal.GFU_MinMax )
    rat.CreateColumn( 'Count', gdal.GFT_Integer, gdal.GFU_PixelCount )

    rat.SetRowCount( 3 )
    rat.SetValueAsInt( 0, 0, 10 )
    rat.SetValueAsInt( 0, 1, 100 )
    rat.SetValueAsInt( 1, 0, 11 )
    rat.SetValueAsInt( 1, 1, 200 )
    rat.SetValueAsInt( 2, 0, 12 )
    rat.SetValueAsInt( 2, 1, 90 )

    rat2 = rat.Clone()
    
    if rat2.GetColumnCount() != 2:
        gdaltest.post_reason( 'wrong column count' )
        return 'fail'

    if rat2.GetRowCount() != 3:
        gdaltest.post_reason( 'wrong row count' )
        return 'fail'

    if rat2.GetNameOfCol(1) != 'Count':
        gdaltest.post_reason( 'wrong column name' )
        return 'fail'

    if rat2.GetUsageOfCol(1) != gdal.GFU_PixelCount:
        gdaltest.post_reason( 'wrong column usage' )
        return 'fail'

    if rat2.GetTypeOfCol(1) != gdal.GFT_Integer:
        gdaltest.post_reason( 'wrong column type' )
        return 'fail'

    if rat2.GetRowOfValue( 11.0 ) != 1:
        gdaltest.post_reason( 'wrong row for value' )
        return 'fail'

    if rat2.GetValueAsInt( 1, 1 ) != 200:
        gdaltest.post_reason( 'wrong field value.' )
        return 'fail'

    gdaltest.saved_rat = rat

    return 'success'

###############################################################################
# Save a RAT in a file, written to .aux.xml, read it back and check it.

def rat_2():

    if gdaltest.saved_rat is None:
        return 'skip'

    ds = gdal.GetDriverByName('PNM').Create('tmp/rat_2.pnm', 100, 90, 1,
                                            gdal.GDT_Byte )
    ds.GetRasterBand(1).SetDefaultRAT( gdaltest.saved_rat )

    ds = None

    ds = gdal.Open( 'tmp/rat_2.pnm', gdal.GA_Update )
    rat2 = ds.GetRasterBand(1).GetDefaultRAT()

    if rat2.GetColumnCount() != 2:
        gdaltest.post_reason( 'wrong column count' )
        return 'fail'

    if rat2.GetRowCount() != 3:
        gdaltest.post_reason( 'wrong row count' )
        return 'fail'

    if rat2.GetNameOfCol(1) != 'Count':
        gdaltest.post_reason( 'wrong column name' )
        return 'fail'

    if rat2.GetUsageOfCol(1) != gdal.GFU_PixelCount:
        gdaltest.post_reason( 'wrong column usage' )
        return 'fail'

    if rat2.GetTypeOfCol(1) != gdal.GFT_Integer:
        gdaltest.post_reason( 'wrong column type' )
        return 'fail'

    if rat2.GetRowOfValue( 11.0 ) != 1:
        gdaltest.post_reason( 'wrong row for value' )
        return 'fail'

    if rat2.GetValueAsInt( 1, 1 ) != 200:
        gdaltest.post_reason( 'wrong field value.' )
        return 'fail'

    # unset the RAT
    ds.GetRasterBand(1).SetDefaultRAT( None )

    ds = None

    ds = gdal.Open( 'tmp/rat_2.pnm' )
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    ds = None
    if rat is not None:
        gdaltest.post_reason( 'expected a NULL RAT.' )
        return 'fail'
    
    gdal.GetDriverByName('PNM').Delete( 'tmp/rat_2.pnm' )
    
    gdaltest.saved_rat = None

    return 'success'

##############################################################################

gdaltest_list = [
    rat_1,
    rat_2,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rat' )
    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

