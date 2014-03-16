#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test AsyncReader interface
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# Test AsyncReader interface on the default (synchronous) implementation

def asyncreader_1():

    ds = gdal.Open('data/rgbsmall.tif')
    asyncreader = ds.BeginAsyncReader(0,0,ds.RasterXSize,ds.RasterYSize)
    buf = asyncreader.GetBuffer()
    result = asyncreader.GetNextUpdatedRegion(0)
    if result != [gdal.GARIO_COMPLETE, 0, 0, ds.RasterXSize,ds.RasterYSize]:
        gdaltest.post_reason('wrong return values for GetNextUpdatedRegion()')
        print(result)
        return 'fail'
    ds.EndAsyncReader(asyncreader)
    asyncreader = None

    out_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/asyncresult.tif', ds.RasterXSize,ds.RasterYSize,ds.RasterCount)
    out_ds.WriteRaster(0,0,ds.RasterXSize,ds.RasterYSize,buf)

    expected_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)]
    cs = [ out_ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)]

    ds = None
    out_ds = None
    gdal.Unlink('/vsimem/asyncresult.tif')

    for i in range(len(cs)):
        if cs[i] != expected_cs[i]:
            gdaltest.post_reason('did not get expected checksum for band %d' % (i+1))
            print(cs[i])
            print(expected_cs[i])
            return 'fail'

    return 'success'


gdaltest_list = [ asyncreader_1 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'asyncreader' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

