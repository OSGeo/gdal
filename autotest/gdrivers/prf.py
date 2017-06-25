#!/usr/bin/env python
###############################################################################
# Purpose:  Tests for Racurs PHOTOMOD tiled format reader (http://www.racurs.ru)
# Author:   Andrew Sudorgin (drons [a] list dot ru)
###############################################################################
# Copyright (c) 2016, Andrew Sudorgin
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
from osgeo import gdal

###############################################################################

def prf_1():

    tst = gdaltest.GDALTest( 'prf', './PRF/ph.prf', 1, 43190 )
    return tst.testOpen( check_gt = (1, 2, 3, -7, 5, 6) )

def prf_2():

    ds = gdal.Open('./data/PRF/dem.x-dem')

    if ds.RasterXSize != 4330:
        gdaltest.post_reason('Invalid dataset width')
        return 'fail'

    if ds.RasterYSize != 4663:
        gdaltest.post_reason('Invalid dataset height')
        return 'fail'

    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != 'm':
        gdaltest.post_reason('Failed to read elevation units from x-dem')
        print(unittype)
        return 'fail'

    datatype = ds.GetRasterBand(1).DataType
    if datatype != gdal.GDT_Float32:
        gdaltest.post_reason('Failed to read datatype')
        return 'fail'

    expectedOvCount = 1
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        gdaltest.post_reason( 'did not get expected number of overviews')
        print( 'Overview count must be %d' % expectedOvCount )
        print( 'But GetOverviewCount returned %d' % ds.GetRasterBand(1).GetOverviewCount() )
        return 'fail'

    overview = ds.GetRasterBand(1).GetOverview(0)
    if overview.XSize != 1082:
        gdaltest.post_reason( 'Invalid dataset width %d' % overview.XSize )
        return 'fail'
    if overview.YSize != 1165:
        gdaltest.post_reason( 'Invalid dataset height %d' % overview.YSize )
        return 'fail'

    ds = None

    return 'success'

def prf_3():

    ds = gdal.Open('./data/PRF/ph.prf')

    expectedOvCount = 0
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        gdaltest.post_reason( 'did not get expected number of overviews')
        print( 'Overview count must be %d' % expectedOvCount )
        print( 'But GetOverviewCount returned %d' % ds.GetRasterBand(1).GetOverviewCount() )
        return 'fail'

    ds = None

    return 'success'

###############################################################################

gdaltest_list = [
    prf_1,
    prf_2,
    prf_3
]

if __name__ == '__main__':

    gdaltest.setup_run( 'prf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
