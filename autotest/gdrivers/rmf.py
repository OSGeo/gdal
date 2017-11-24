#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Raster Matrix Format used in GISes "Panorama"/"Integratsia".
# Author:   Andrey Kiselev <dron@ak4719.spb.edu>
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
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
import gdal

###############################################################################
# Perform simple read tests.

def rmf_1():

    tst = gdaltest.GDALTest( 'rmf', 'byte.rsw', 1, 4672 )
    return tst.testOpen( check_gt = (440720, 60, 0, 3751320, 0, -60) )

def rmf_2():

    tst = gdaltest.GDALTest( 'rmf', 'byte-lzw.rsw', 1, 4672 )
    return tst.testOpen( check_gt = (440720, 60, 0, 3751320, 0, -60) )

def rmf_3():

    tst = gdaltest.GDALTest( 'rmf', 'float64.mtw', 1, 4672 )
    return tst.testOpen( check_gt = (440720, 60, 0, 3751320, 0, -60) )

def rmf_4():

    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall.rsw', 1, 21212 )
    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall.rsw', 2, 21053 )
    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall.rsw', 3, 21349 )
    return tst.testOpen( check_gt = (-44.840320, 0.003432, 0,
                                     -22.932584, 0, -0.003432) )

def rmf_5():

    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall-lzw.rsw', 1, 21212 )
    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall-lzw.rsw', 2, 21053 )
    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall-lzw.rsw', 3, 21349 )
    return tst.testOpen( check_gt = (-44.840320, 0.003432, 0,
                                     -22.932584, 0, -0.003432) )

def rmf_6():

    tst = gdaltest.GDALTest( 'rmf', 'big-endian.rsw', 1, 7782 )
    tst = gdaltest.GDALTest( 'rmf', 'big-endian.rsw', 2, 8480 )
    tst = gdaltest.GDALTest( 'rmf', 'big-endian.rsw', 3, 4195 )
    return tst.testOpen()

###############################################################################
# Create simple copy and check.

def rmf_7():

    tst = gdaltest.GDALTest( 'rmf', 'byte.rsw', 1, 4672 )

    return tst.testCreateCopy( check_srs = 1, check_gt = 1, vsimem = 1 )

def rmf_8():

    tst = gdaltest.GDALTest( 'rmf', 'rgbsmall.rsw', 2, 21053 )

    return tst.testCreateCopy( check_srs = 1, check_gt = 1 )

###############################################################################
# Create RMFHUGE=YES

def rmf_9():

    tst = gdaltest.GDALTest( 'rmf', 'byte.rsw', 1, 4672, options = ['RMFHUGE=YES'] )

    return tst.testCreateCopy( check_srs = 1, check_gt = 1, vsimem = 1 )

###############################################################################
# Compressed DEM

def rmf_10():

    tst = gdaltest.GDALTest( 'rmf', 't100.mtw', 1, 6388 )

    return tst.testOpen()

###############################################################################
# Overviews

def rmf_11():

    test_fn = '/vsigzip/data/overviews.rsw.gz'
    src_ds = gdal.Open(test_fn)

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band1 = src_ds.GetRasterBand(1)

    if band1.GetOverviewCount() != 3:
        gdaltest.post_reason( 'overviews is missing' )
        return 'fail'

    ovr_n = ( 0, 1, 2 )
    ovr_size = ( 16, 64, 256 )
    ovr_checksum = ( 3192, 51233, 32756 )

    for i in ovr_n:
        ovr_band = band1.GetOverview(i)
        if ovr_band.XSize != ovr_size[i] or ovr_band.YSize != ovr_size[i]:
            msg = 'overview wrong size: overview %d, size = %d * %d,' % (i, ovr_band.XSize, ovr_band.YSize)
            gdaltest.post_reason( msg )
            return 'fail'

        if ovr_band.Checksum() != ovr_checksum[i]:
            msg = 'overview wrong checkum: overview %d, checksum = %d,' % (i, ovr_band.Checksum())
            gdaltest.post_reason( msg )
            return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [
    rmf_1,
    rmf_2,
    rmf_3,
    rmf_4,
    rmf_5,
    rmf_6,
    rmf_7,
    rmf_8,
    rmf_9,
    rmf_10,
    rmf_11
]

if __name__ == '__main__':

    gdaltest.setup_run( 'rmf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
