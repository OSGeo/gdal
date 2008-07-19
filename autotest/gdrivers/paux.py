#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PAux driver.
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

def paux_1():

    tst = gdaltest.GDALTest( 'PAux', 'small16.raw', 2, 12816 )
    return tst.testOpen()

###############################################################################
# Test copying.

def paux_2():

    tst = gdaltest.GDALTest( 'PAux', 'byte.tif', 1, 4672)

    return tst.testCreateCopy( check_gt = 1 )
    
###############################################################################
# Test /vsimem based.

def paux_3():

    tst = gdaltest.GDALTest( 'PAux', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )
    
###############################################################################
# Cleanup.

def paux_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    paux_1,
    paux_2,
    paux_3,
    paux_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'paux' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

