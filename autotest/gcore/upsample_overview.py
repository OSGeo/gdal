#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Overview read test.
# Author:   Andrew Sudorgin (drons [a] list dot ru)
#
###############################################################################
# Copyright (c) 2018, Andrew Sudorgin (drons [a] list dot ru)
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
import numpy

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# Comparison of the upsampled overview image with the original

def checkupsampled(sx, sy):

    drv_name = 'Gtiff'
    drv = gdal.GetDriverByName(drv_name)
    if drv is None:
        gdaltest.post_reason(drv_name + ' driver not found.')
        return 'fail'

    # Create dataset
    size = 256
    lod = 6
    scale = 1 << lod
    ds_name = 'tmp/ov_test1.tif'
    tst_name = 'tmp/ov_test2.tif'
    ds = drv.Create(ds_name, sx, sy, 1, gdal.GDT_Byte )

    chess = numpy.zeros((size,size), dtype = numpy.uint8)
    chess[0:size/2,0:size/2] = 255
    chess[size/2:size,size/2:size] = 255
    for y in range(sy/size):
        for x in range(sx/size):
            ds.GetRasterBand(1).WriteArray(chess, x*size, y*size)
    ds = None

    # Build overviews
    ds = gdal.Open(ds_name)
    if ds is None:
        gdaltest.post_reason(ds_name + ' driver not found.')
        return 'fail'
    expected_checksum = ds.GetRasterBand(1).Checksum()
    ds.BuildOverviews('AVERAGE', overviewlist = [2, 4, 8, 16, 32, 64])
    ds = None

    # Reopen & check
    ds = gdal.Open(ds_name)
    if ds.GetRasterBand(1).GetOverviewCount() < lod:
        gdaltest.post_reason('Overviews not found')
        return 'fail'

    #ov_ar = ds.GetRasterBand(1).GetOverview(lod-1).ReadAsArray()
    #print ov_ar.shape
    #print ov_ar

    # Upsample overview back to the original size
    tst = drv.Create(tst_name, sx, sy, 1, gdal.GDT_Byte )

    for y in range(sy/size):
        for x in range(sx/size):
            buff = numpy.zeros((size,size), dtype = numpy.uint8)
            ar = ds.GetRasterBand(1).ReadAsArray(0, 0, size, size,
                                                 buf_xsize = size/scale,
                                                 buf_ysize = size/scale)
            #print ar
            for i in range(size):
                for j in range(size):
                    buff[i,j] = ar[i/scale, j/scale]
            tst.GetRasterBand(1).WriteArray(buff, x*size, y*size)
    tst = None
    ds = None

    tst = gdal.Open(tst_name)
    if tst is None:
        gdaltest.post_reason(tst_name + ' driver not found.')
        return 'fail'
    checksum = tst.GetRasterBand(1).Checksum()

    drv.Delete(ds_name)
    drv.Delete(tst_name)

    if expected_checksum != checksum:
        gdaltest.post_reason('Invalid upsampled image.')
        return 'fail'

    return 'success'

def upsample_overview_1():
    return checkupsampled(1024, 1024)

def upsample_overview_2():
    return checkupsampled(1011, 1011)

###############################################################################

gdaltest_list = [
    upsample_overview_1,
    upsample_overview_2,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'upsample_overview' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
