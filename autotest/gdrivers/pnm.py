#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PNM (Portable Anyware Map) Testing.
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
# Read Test grayscale (PGM)

def pnm_1():

    tst = gdaltest.GDALTest( 'PNM', 'byte.pgm', 1, 4672 )

    return tst.testOpen()

###############################################################################
# Write Test grayscale (PGM)

def pnm_2():

    tst = gdaltest.GDALTest( 'PNM', 'byte.pgm', 1, 4672 )

    return tst.testCreateCopy(vsimem = 1)

###############################################################################
# Read Test RGB (PPM)

def pnm_3():

    tst = gdaltest.GDALTest( 'PNM', 'rgbsmall.ppm', 2, 21053 )

    return tst.testOpen()

###############################################################################
# Write Test RGB (PPM)

def pnm_4():

    tst = gdaltest.GDALTest( 'PNM', 'rgbsmall.ppm', 2, 21053 )

    return tst.testCreateCopy()

gdaltest_list = [
    pnm_1,
    pnm_2,
    pnm_3,
    pnm_4
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'ppm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

