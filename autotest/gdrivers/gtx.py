#!/usr/bin/env python
###############################################################################
# $Id: gsc.py 16265 2009-02-08 11:15:27Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GSC driver
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2010, Frank Warmerdam
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test reading a small gtx file.

def gtx_1():

    tst = gdaltest.GDALTest( 'GTX', 'hydroc1.gtx', 1, 64183 )
    gt = (276.725, 0.05, 0.0, 42.775, 0.0, -0.05)
    return tst.testOpen( check_gt = gt, check_prj = 'WGS84' )

gdaltest_list = [
    gtx_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'gtx' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

