#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Overview Support (mostly a GeoTIFF issue).
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
import gdal
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Create a 3 band floating point GeoTIFF file so we can build overviews on it
# later.  Build overviews on it. 

def tiff_ovr_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('data/mfloat32.vrt')
    gdaltest.tiff_drv.CreateCopy( 'tmp/mfloat32.tif', src_ds,
                                  options = ['INTERLEAVE=PIXEL'] )
    src_ds = None

    ds = gdal.Open( 'tmp/mfloat32.tif' )
    err = ds.BuildOverviews( overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('Overview missing on target file.')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Open file and verify some characteristics of the overviews. 

def tiff_ovr_2():

    src_ds = gdal.Open( 'tmp/mfloat32.tif' )

    if src_ds.GetRasterBand(2).GetOverviewCount() != 2:
        gdaltest.post_reason( 'overviews missing after re-open?' )
        return 'fail'

    ovr_band = src_ds.GetRasterBand(3).GetOverview(0)
    if ovr_band.XSize != 10 or ovr_band.YSize != 10:
        gdaltest.post_reason('first overview wrong size.')
        return 'fail'

    if ovr_band.Checksum() != 1087:
        print ovr_band.Checksum()
        gdaltest.post_reason( 'first overview wrong checkum' )
        return 'fail'

    ovr_band = src_ds.GetRasterBand(3).GetOverview(1)
    if ovr_band.XSize != 5 or ovr_band.YSize != 5:
        gdaltest.post_reason('first overview wrong size.')
        return 'fail'

    if ovr_band.Checksum() != 328:
        print ovr_band.Checksum()
        gdaltest.post_reason( 'first overview wrong checkum' )
        return 'fail'

    src_ds = None

    return 'success'

###############################################################################
# Open target file in update mode, and create internal overviews.

def tiff_ovr_3():

    os.unlink( 'tmp/mfloat32.tif.ovr' )
    
    src_ds = gdal.Open( 'tmp/mfloat32.tif', gdal.GA_Update )

    err = src_ds.BuildOverviews( overviewlist = [2, 4] )
    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    if src_ds.GetRasterBand(2).GetOverviewCount() != 2:
        gdaltest.post_reason( 'overviews missing after re-open?' )
        return 'fail'

    ovr_band = src_ds.GetRasterBand(3).GetOverview(0)
    if ovr_band.XSize != 10 or ovr_band.YSize != 10:
        gdaltest.post_reason('first overview wrong size.')
        return 'fail'

    if ovr_band.Checksum() != 1087:
        print ovr_band.Checksum()
        gdaltest.post_reason( 'first overview wrong checkum' )
        return 'fail'

    ovr_band = src_ds.GetRasterBand(3).GetOverview(1)
    if ovr_band.XSize != 5 or ovr_band.YSize != 5:
        gdaltest.post_reason('first overview wrong size.')
        return 'fail'

    if ovr_band.Checksum() != 328:
        print ovr_band.Checksum()
        gdaltest.post_reason( 'first overview wrong checkum' )
        return 'fail'

    src_ds = None

    return 'success'

###############################################################################
# Test generation 

def tiff_ovr_4():

    shutil.copyfile( 'data/oddsize_1bit2b.tif', 'tmp/ovr4.tif' )

    wrk_ds = gdal.Open('tmp/ovr4.tif',gdal.GA_Update)
    wrk_ds.BuildOverviews( 'AVERAGE_BIT2GRAYSCALE', overviewlist = [2,4] )
    wrk_ds = None

    wrk_ds = gdal.Open('tmp/ovr4.tif')

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    if not md.has_key('RESAMPLING') \
       or md['RESAMPLING'] != 'AVERAGE_BIT2GRAYSCALE':
        gdaltest.post_reason( 'Did not get expected RESAMPLING metadata.' )
        return 'fail'

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0,0,ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    for i in range(pix_count):
        sum = sum + ord(ovimage[i])
    average = sum / pix_count
    exp_average = 154.8144
    if abs(average - exp_average) > 0.1:
        print average
        gdaltest.post_reason( 'got wrong average for overview image' )
        return 'fail'

    # read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(0,0,frband.XSize,frband.YSize,
                                ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    for i in range(pix_count):
        sum = sum + ord(ovimage[i])
    average = sum / pix_count
    exp_average = 0.6096
    
    if abs(average - exp_average) > 0.01:
        print average
        gdaltest.post_reason( 'got wrong average for downsampled image' )
        return 'fail'

    wrk_ds = None

    return 'success'
    

###############################################################################
# Cleanup

def tiff_ovr_cleanup():
    gdaltest.tiff_drv.Delete( 'tmp/mfloat32.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr4.tif' )
    gdaltest.tiff_drv = None

    return 'success'

gdaltest_list = [
    tiff_ovr_1,
    tiff_ovr_2,
    tiff_ovr_3,
    tiff_ovr_4,
    tiff_ovr_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_ovr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

