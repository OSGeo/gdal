#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test conventional CEOS driver.
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
import string
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# First 75K of an IRS (Indian Remote Sensing Satellite) LGSOWG scene.  Only
# contains 3 complete scanlines.  Bizarre little endian CEOS variant. (#1862)

def ceos_1():

    tst = gdaltest.GDALTest( 'CEOS', 'IMAGERY-75K.L-3', 4, 9956,
                             xoff = 0, yoff = 0, xsize = 5932, ysize = 3 )
    return tst.testOpen()

gdaltest_list = [
    ceos_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ceos' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

