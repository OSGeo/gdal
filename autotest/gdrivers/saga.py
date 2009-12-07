#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAGA GIS Binary driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault, <even dot rouault at mines dash paris dot org>
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
# Test opening

def saga_1():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    return tst.testOpen()

###############################################################################
# Test CreateCopy()

def saga_2():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    ret = tst.testCreateCopy( new_filename = 'tmp/createcopy.sdat' )
    try:
        os.remove('tmp/createcopy.sgrd')
    except:
        pass
    return ret

###############################################################################
# Test Copy()

def saga_3():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    ret = tst.testCreate( new_filename = 'tmp/copy.sdat', out_bands = 1 )
    try:
        os.remove('tmp/copy.sgrd')
    except:
        pass
    return ret
    
gdaltest_list = [
    saga_1,
    saga_2,
    saga_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'saga' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

