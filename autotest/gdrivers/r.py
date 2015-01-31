#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test R driver support.
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

import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test on an ascii file.

def r_1():

    tst = gdaltest.GDALTest( 'R', 'r_test.asc', 2, 202 )

    return tst.testOpen()

###############################################################################
# Perform a simple read test on a binary (uncompressed) file.

def r_2():

    tst = gdaltest.GDALTest( 'R', 'r_test.rdb', 1, 202 )
    return tst.testOpen()

###############################################################################
# Verify a simple createcopy operation with 16bit data.

def r_3():

    tst = gdaltest.GDALTest( 'R', 'byte.tif', 1, 4672,
                             options = ['ASCII=YES'] )
    return tst.testCreateCopy()

###############################################################################
# Test creating a compressed binary stream and reading it back.

def r_4():

    tst = gdaltest.GDALTest( 'R', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( new_filename = 'tmp/r_4.rda' )

###############################################################################

gdaltest_list = [
    r_1,
    r_2,
    r_3,
    r_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'r' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

