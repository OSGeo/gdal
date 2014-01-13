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
from osgeo import gdal

###############################################################################
# Test handling NaN with GDT_Float32 data

def stats_nan_1():

    gdaltest.gtiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.gtiff_drv is None:
        return 'skip'
    
    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile('data/nan32.tif', 'tmp/nan32.tif')

    t = gdaltest.GDALTest( 'GTiff', 'tmp/nan32.tif', 1, 874, filename_absolute = 1 )
    ret = t.testOpen( check_approx_stat = stats, check_stat = stats )

    gdal.GetDriverByName('GTiff').Delete('tmp/nan32.tif')

    return ret

###############################################################################
# Test handling NaN with GDT_Float64 data

def stats_nan_2():

    if gdaltest.gtiff_drv is None:
        return 'skip'

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile('data/nan64.tif', 'tmp/nan64.tif')

    t = gdaltest.GDALTest( 'GTiff', 'tmp/nan64.tif', 1, 4414, filename_absolute = 1 )
    ret = t.testOpen( check_approx_stat = stats, check_stat = stats )

    gdal.GetDriverByName('GTiff').Delete('tmp/nan64.tif')

    return ret

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
# force their computation (#3572)

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
# Test read and copy of dataset with nan as nodata value (#3576)

def stats_nan_3():

    src_ds = gdal.Open('data/nan32_nodata.tif')
    nodata = src_ds.GetRasterBand(1).GetNoDataValue()
    if not gdaltest.isnan(nodata):
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    out_ds = gdaltest.gtiff_drv.CreateCopy('tmp/nan32_nodata.tif', src_ds)
    out_ds = None

    src_ds = None

    try:
        os.remove('tmp/nan32_nodata.tif.aux.xml')
    except:
        pass

    ds = gdal.Open('tmp/nan32_nodata.tif')
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    gdaltest.gtiff_drv.Delete('tmp/nan32_nodata.tif')
    if not gdaltest.isnan(nodata):
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a VRT with a complex source that define nan as band nodata
# and complex source nodata (#3576)

def stats_nan_4():

    ds = gdal.Open('data/nan32_nodata.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if cs != 874:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    if not gdaltest.isnan(nodata):
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a VRT with a complex source that define 0 as band nodata
# and complex source nodata (nan must be translated to 0 then) (#3576)

def stats_nan_5():

    ds = gdal.Open('data/nan32_nodata_nan_to_zero.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if cs != 978:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    if nodata != 0:
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'


###############################################################################
# Test reading a warped VRT with nan as src nodata and dest nodata (#3576)

def stats_nan_6():

    ds = gdal.Open('data/nan32_nodata_warp.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if cs != 874:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    if not gdaltest.isnan(nodata):
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a warped VRT with nan as src nodata and 0 as dest nodata (#3576)

def stats_nan_7():

    ds = gdal.Open('data/nan32_nodata_warp_nan_to_zero.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if cs != 978:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    if nodata != 0:
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'


###############################################################################
# Test reading a warped VRT with zero as src nodata and nan as dest nodata (#3576)

def stats_nan_8():

    ds = gdal.Open('data/nan32_nodata_warp_zero_to_nan.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if cs != 874:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    if not gdaltest.isnan(nodata):
        gdaltest.post_reason('expected nan, got %f' % nodata)
        return 'fail'

    return 'success'

###############################################################################
# Test statistics computation when nodata = +/- inf

def stats_nodata_inf():

    ds = gdal.GetDriverByName('HFA').Create('/vsimem/stats_nodata_inf.img', 3, 1,1, gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(gdaltest.neginf())
    import struct
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack('f', gdaltest.neginf()), buf_type = gdal.GDT_Float32 )
    ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, struct.pack('f', 1), buf_type = gdal.GDT_Float32 )
    ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, struct.pack('f', -2), buf_type = gdal.GDT_Float32 )

    cs = ds.GetRasterBand(1).Checksum()
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None
    
    gdal.GetDriverByName('HFA').Delete('/vsimem/stats_nodata_inf.img')

    if stats != [-2.0, 1.0, -0.5, 1.5]:
        gdaltest.post_reason('did not get expected stats')
        print(stats)
        return 'fail'

    return 'success'


###############################################################################
# Test deserialization of +inf/-inf values written by Linux and Windows

def stats_nodata_check(filename, expected_nodata):
    ds = gdal.Open(filename)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    if nodata != expected_nodata:
        gdaltest.post_reason('did not get expected nodata value')
        print(nodata)
        return 'fail'

    return 'success'

def stats_nodata_neginf_linux():
    return stats_nodata_check('data/stats_nodata_neginf.tif', gdaltest.neginf())

def stats_nodata_neginf_msvc():
    return stats_nodata_check('data/stats_nodata_neginf_msvc.tif', gdaltest.neginf())

def stats_nodata_posinf_linux():
    return stats_nodata_check('data/stats_nodata_posinf.tif', gdaltest.posinf())

def stats_nodata_posinf_msvc():
    return stats_nodata_check('data/stats_nodata_posinf_msvc.tif', gdaltest.posinf())

###############################################################################
# Run tests

gdaltest_list = [
    stats_nan_1,
    stats_nan_2,
    stats_signedbyte,
    stats_dont_force,
    stats_approx_nodata,
    stats_nan_3,
    stats_nan_4,
    stats_nan_5,
    stats_nan_6,
    stats_nan_7,
    stats_nan_8,
    stats_nodata_inf,
    stats_nodata_neginf_linux,
    stats_nodata_neginf_msvc,
    stats_nodata_posinf_linux,
    stats_nodata_posinf_msvc
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'gdal_stats' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

