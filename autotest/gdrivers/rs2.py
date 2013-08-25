#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RS2 driver
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test reading a - fake - RS2 dataset. Note: the XML file was written by studying
# the code of the driver. It is really not meant as being used by other readers. If RS2 code
# evolves, this might break the test legitimately !

def rs2_1():

    tst = gdaltest.GDALTest( 'RS2', 'product.xml', 1, 4672 )
    return tst.testOpen()


def rs2_2():

    tst = gdaltest.GDALTest( 'RS2', 'RADARSAT_2_CALIB:BETA0:data/product.xml', 1, 4848, filename_absolute=1 )
    return tst.testOpen()

gdaltest_list = [
    rs2_1,
    rs2_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rs2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

