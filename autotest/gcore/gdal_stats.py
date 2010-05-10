#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test core numeric operations and statistics calculations
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys
import shutil

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# Test handling NaN with GDT_Float32 data

def stats_nan_1():

    gdaltest.gtiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.gtiff_drv is None:
        return 'skip'

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    t = gdaltest.GDALTest( 'GTiff', 'nan32.tif', 1, 874 )
    return t.testOpen( check_approx_stat = stats, check_stat = stats )

###############################################################################
# Test handling NaN with GDT_Float64 data

def stats_nan_2():

    if gdaltest.gtiff_drv is None:
        return 'skip'

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    t = gdaltest.GDALTest( 'GTiff', 'nan64.tif', 1, 4414 )
    return t.testOpen( check_approx_stat = stats, check_stat = stats )


###############################################################################
# Test stats on signed byte (#3151)

def stats_signedbyte():

    if gdaltest.gtiff_drv is None:
        return 'skip'

    stats = (-128.0, 127.0, -0.2, 80.64)
    
    shutil.copyfile('data/stats_signed_byte.img', 'tmp/stats_signed_byte.img')

    t = gdaltest.GDALTest( 'HFA', 'tmp/stats_signed_byte.img', 1, 11, filename_absolute = 1 )
    ret = t.testOpen( check_approx_stat = stats, check_stat = stats, skip_checksum = 1 )
    
    gdal.GetDriverByName('HFA').Delete('tmp/stats_signed_byte.img')
    
    return ret


###############################################################################
# Test return of GetStatistics() when we don't have stats and don't
# force their cmoputation (#3572)

def stats_dont_force():

    ds = gdal.Open('data/byte.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    if stats != [0, 0, 0, -1]:
        gdaltest.post_reason('did not get expected stats')
        print(stats)
        return 'fail'

    return 'success'


###############################################################################
# Test statistics when stored nodata value doesn't accurately match the nodata
# value used in the imagery (#3573)

def stats_approx_nodata():

    shutil.copyfile('data/minfloat.tif', 'tmp/minfloat.tif')
    try:
        os.remove('tmp/minfloat.tif.aux.xml')
    except:
        pass

    ds = gdal.Open('tmp/minfloat.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove('tmp/minfloat.tif.aux.xml')

    ds = gdal.Open('tmp/minfloat.tif')
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove('tmp/minfloat.tif')

    if nodata != -3.4028234663852886e+38:
        gdaltest.post_reason('did not get expected nodata')
        print("%.18g" % nodata)
        return 'fail'

    if stats != [-3.0, 5.0, 1.0, 4.0]:
        gdaltest.post_reason('did not get expected stats')
        print(stats)
        return 'fail'

    if minmax != (-3.0, 5.0):
        gdaltest.post_reason('did not get expected minmax')
        print(minmax)
        return 'fail'

    return 'success'

###############################################################################
# Run tests

gdaltest_list = [
    stats_nan_1,
    stats_nan_2,
    stats_signedbyte,
    stats_dont_force,
    stats_approx_nodata
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'gdal_stats' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

