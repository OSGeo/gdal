#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PGCHIP Testing.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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
# 
def pgchip_init():
    try:
        gdaltest.pgchipDriver = gdal.GetDriverByName('PGCHIP')
    except:
        gdaltest.pgchipDriver = None

    if gdaltest.pgchipDriver is None:
        return 'skip'

    return 'success'

###############################################################################
# 

def pgchip_1():

    if gdaltest.pgchipDriver is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')
    ds = gdaltest.pgchipDriver.CreateCopy('PG:dbname=autotest\%layer=test_chip', ds_src)

    tst = gdaltest.GDALTest( 'PGCHIP', 'PG:dbname=autotest\%layer=test_chip', 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def pgchip_2():

    if gdaltest.pgchipDriver is None:
        return 'skip'

    ds_src = gdal.Open('../gcore/data/uint16.tif')
    ds = gdaltest.pgchipDriver.CreateCopy('PG:dbname=autotest\%layer=test_chip', ds_src)

    tst = gdaltest.GDALTest( 'PGCHIP', 'PG:dbname=autotest\%layer=test_chip', 1, 4672, filename_absolute = 1 )

    return tst.testOpen()


gdaltest_list = [
    pgchip_init,
    pgchip_1,
    pgchip_2
    ]



if __name__ == '__main__':

    gdaltest.setup_run( 'PGCHIP' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

