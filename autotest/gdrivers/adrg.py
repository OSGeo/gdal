#!/usr/bin/env python
###############################################################################
# $Id: adrg.py $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ADRG driver.
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

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of simple byte reference data.

def adrg_1():

    tst = gdaltest.GDALTest( 'ADRG', 'SMALL_ADRG/ABCDEF01.GEN', 1, 62833 )
    return tst.testOpen()

###############################################################################
# Test copying.

def adrg_2():

    drv = gdal.GetDriverByName( 'ADRG' )
    srcds = gdal.Open( 'data/SMALL_ADRG/ABCDEF01.GEN' )
    
    dstds = drv.CreateCopy( 'tmp/ABCDEF01.GEN', srcds )
    
    dstds = None
    
    drv.Delete( 'tmp/ABCDEF01.GEN' )

    return 'success'
    

###############################################################################
# Cleanup procedure

def adrg_cleanup():

    try:
        os.remove('data/SMALL_ADRG/ABCDEF01.GEN.aux.xml')
        os.remove('tmp/ABCDEF01.GEN.aux.xml')
        os.remove('tmp/ABCDEF01.IMG')
        os.remove('tmp/TRANSH01.THF')
    except:
        pass

    return 'success'

###############################################################################
gdaltest_list = [
    adrg_1,
    adrg_2,
    adrg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'adrg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

