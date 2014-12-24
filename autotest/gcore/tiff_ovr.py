#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Overview Support (mostly a GeoTIFF issue).
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr
from osgeo import gdal
import shutil
import string
import array
import stat

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Check the overviews

def tiff_ovr_check(src_ds):
    for i in (1, 2, 3):
        if src_ds.GetRasterBand(i).GetOverviewCount() != 2:
            gdaltest.post_reason( 'overviews missing' )
            return 'fail'

        ovr_band = src_ds.GetRasterBand(i).GetOverview(0)
        if ovr_band.XSize != 10 or ovr_band.YSize != 10:
            msg = 'overview wrong size: band %d, overview 0, size = %d * %d,' % (i, ovr_band.XSize, ovr_band.YSize)
            gdaltest.post_reason( msg )
            return 'fail'

        if ovr_band.Checksum() != 1087:
            msg = 'overview wrong checkum: band %d, overview 0, checksum = %d,' % (i, ovr_band.Checksum())
            gdaltest.post_reason( msg )
            return 'fail'

        ovr_band = src_ds.GetRasterBand(i).GetOverview(1)
        if ovr_band.XSize != 5 or ovr_band.YSize != 5:
            msg = 'overview wrong size: band %d, overview 1, size = %d * %d,' % (i, ovr_band.XSize, ovr_band.YSize)
            gdaltest.post_reason( msg )
            return 'fail'

        if ovr_band.Checksum() != 328:
            msg = 'overview wrong checkum: band %d, overview 1, checksum = %d,' % (i, ovr_band.Checksum())
            gdaltest.post_reason( msg )
            return 'fail'
    return 'success'

###############################################################################
# Create a 3 band floating point GeoTIFF file so we can build overviews on it
# later.  Build overviews on it. 

def tiff_ovr_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('data/mfloat32.vrt')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdaltest.tiff_drv.CreateCopy( 'tmp/mfloat32.tif', src_ds,
                                  options = ['INTERLEAVE=PIXEL'] )
    src_ds = None

    ds = gdal.Open( 'tmp/mfloat32.tif' )
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    err = ds.BuildOverviews( overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    ret = tiff_ovr_check(ds)

    ds = None

    return ret


###############################################################################
# Open file and verify some characteristics of the overviews. 

def tiff_ovr_2():

    src_ds = gdal.Open( 'tmp/mfloat32.tif' )

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'


    ret = tiff_ovr_check(src_ds)

    src_ds = None

    return ret

###############################################################################
# Open target file in update mode, and create internal overviews.

def tiff_ovr_3():

    os.unlink( 'tmp/mfloat32.tif.ovr' )

    src_ds = gdal.Open( 'tmp/mfloat32.tif', gdal.GA_Update )

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    err = src_ds.BuildOverviews( overviewlist = [2, 4] )
    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    ret = tiff_ovr_check(src_ds)

    src_ds = None

    return ret

###############################################################################
# Re-open target file and check overviews

def tiff_ovr_3bis():
    return tiff_ovr_2()

###############################################################################
# Test generation 

def tiff_ovr_4():

    shutil.copyfile( 'data/oddsize_1bit2b.tif', 'tmp/ovr4.tif' )

    wrk_ds = gdal.Open('tmp/ovr4.tif',gdal.GA_Update)
    
    if wrk_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    wrk_ds.BuildOverviews( 'AVERAGE_BIT2GRAYSCALE', overviewlist = [2,4] )
    wrk_ds = None

    wrk_ds = gdal.Open('tmp/ovr4.tif')

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    if 'RESAMPLING' not in md \
       or md['RESAMPLING'] != 'AVERAGE_BIT2GRAYSCALE':
        gdaltest.post_reason( 'Did not get expected RESAMPLING metadata.' )
        return 'fail'

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0,0,ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    is_bytes = False
    try:
        if (isinstance(ovimage, bytes) and not isinstance(ovimage, str)):
            is_bytes = True
    except:
        pass

    if is_bytes is True:
        for i in range(pix_count):
            sum = sum + ovimage[i]
    else:
        for i in range(pix_count):
            sum = sum + ord(ovimage[i])
            
    average = sum / pix_count
    exp_average = 154.8144
    if abs(average - exp_average) > 0.1:
        print(average)
        gdaltest.post_reason( 'got wrong average for overview image' )
        return 'fail'

    # read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(0,0,frband.XSize,frband.YSize,
                                ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    if is_bytes is True:
        for i in range(pix_count):
            sum = sum + ovimage[i]
    else:
        for i in range(pix_count):
            sum = sum + ord(ovimage[i])
    average = sum / pix_count
    exp_average = 0.6096
    
    if abs(average - exp_average) > 0.01:
        print(average)
        gdaltest.post_reason( 'got wrong average for downsampled image' )
        return 'fail'

    wrk_ds = None

    return 'success'
    

###############################################################################
# Test average overview generation with nodata.

def tiff_ovr_5():

    shutil.copyfile( 'data/nodata_byte.tif', 'tmp/ovr5.tif' )

    wrk_ds = gdal.Open('tmp/ovr5.tif',gdal.GA_ReadOnly)
    
    if wrk_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    wrk_ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'
    
###############################################################################
# Same as tiff_ovr_5 but with USE_RDD=YES to force external overview

def tiff_ovr_6():

    shutil.copyfile( 'data/nodata_byte.tif', 'tmp/ovr6.tif' )
    
    oldOption = gdal.GetConfigOption('USE_RRD', 'NO')
    gdal.SetConfigOption('USE_RRD', 'YES')
    
    wrk_ds = gdal.Open('tmp/ovr6.tif',gdal.GA_Update)
    
    if wrk_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    wrk_ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )
    
    gdal.SetConfigOption('USE_RRD', oldOption)
    
    try:
        os.stat('tmp/ovr6.aux')
    except:
        gdaltest.post_reason( 'no external overview.' )
        return 'fail'
    
    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'


###############################################################################
# Check nearest resampling on a dataset with a raster band that has a color table

def tiff_ovr_7():

    shutil.copyfile( 'data/test_average_palette.tif', 'tmp/test_average_palette.tif' )

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # In nearest resampling, we are expecting a uniform black image.
    ds = gdal.Open('tmp/test_average_palette.tif', gdal.GA_Update)
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds.BuildOverviews( 'NEAREST', overviewlist = [2] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 0

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Check average resampling on a dataset with a raster band that has a color table

def tiff_ovr_8():

    shutil.copyfile( 'data/test_average_palette.tif', 'tmp/test_average_palette.tif' )

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # So the result of averaging (0,0,0) and (255,255,255) is (127,127,127), which is
    # index 2. So the result of the averaging is a uniform grey image.
    ds = gdal.Open('tmp/test_average_palette.tif', gdal.GA_Update)
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Test --config COMPRESS_OVERVIEW JPEG --config PHOTOMETRIC_OVERVIEW YCBCR -ro
# Will also check that pixel interleaving is automatically selected (#3064)

def tiff_ovr_9():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    shutil.copyfile( 'data/rgbsmall.tif', 'tmp/ovr9.tif' )

    gdal.SetConfigOption('COMPRESS_OVERVIEW', 'JPEG')
    gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', 'YCBCR')

    ds = gdal.Open('tmp/ovr9.tif', gdal.GA_ReadOnly)

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    gdal.SetConfigOption('COMPRESS_OVERVIEW', '')
    gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', '')

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    if cs != exp_cs and cs != 5635:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    # Re-check after dataset reopening
    ds = gdal.Open('tmp/ovr9.tif', gdal.GA_ReadOnly)

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    if cs != exp_cs and cs != 5635:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Similar to tiff_ovr_9 but with internal overviews.

def tiff_ovr_10():

    src_ds = gdal.Open('data/rgbsmall.tif', gdal.GA_ReadOnly)
    
    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr10.tif', src_ds, options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR' ] )
    src_ds = None

    if ds is None:
        gdaltest.post_reason( 'Failed to apply JPEG compression.' )
        return 'fail'

    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    ds = None
    ds = gdal.Open('tmp/ovr10.tif', gdal.GA_ReadOnly)

    if ds is None:
        gdaltest.post_reason( 'Failed to open copy of test dataset.' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    if cs != exp_cs and cs != 5635:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Overview on a dataset with NODATA_VALUES

def tiff_ovr_11():

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)
    
    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr11.tif', src_ds )
    src_ds = None

    md = gdaltest.tiff_drv.GetMetadata()
    try:
        if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1 or \
           int(gdal.VersionInfo('VERSION_NUM')) < 1700:
            # The two following lines are necessary with inverted endianness
            # for the moment with older libtiff
            # See http://bugzilla.maptools.org/show_bug.cgi?id=1924 for more details
            ds = None
            ds = gdal.Open('tmp/ovr11.tif', gdal.GA_Update)
    except:
    # OG-python bindings don't have gdal.VersionInfo. Too bad, but let's hope that GDAL's version isn't too old !
        pass 

    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    ds = None
    ds = gdal.Open('tmp/ovr11.tif', gdal.GA_ReadOnly)

    if ds is None:
        gdaltest.post_reason( 'Failed to open copy of test dataset.' )
        return 'fail'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Same as tiff_ovr_11 but with compression to trigger the multiband overview
# code

def tiff_ovr_12():

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)
    
    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr12.tif', src_ds, options = [ 'COMPRESS=DEFLATE' ] )
    src_ds = None

    md = gdaltest.tiff_drv.GetMetadata()
    try:
        if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1 or \
           int(gdal.VersionInfo('VERSION_NUM')) < 1700:
            # The two following lines are necessary with inverted endianness
            # for the moment with older libtiff
            # See http://bugzilla.maptools.org/show_bug.cgi?id=1924 for more details
            ds = None
            ds = gdal.Open('tmp/ovr12.tif', gdal.GA_Update)
    except:
    # OG-python bindings don't have gdal.VersionInfo. Too bad, but let's hope that GDAL's version isn't too old !
        pass 


    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )

    ds = None
    ds = gdal.Open('tmp/ovr12.tif', gdal.GA_ReadOnly)

    if ds is None:
        gdaltest.post_reason( 'Failed to open copy of test dataset.' )
        return 'fail'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'


###############################################################################
# Test gaussian resampling

def tiff_ovr_13():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('data/mfloat32.vrt')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdaltest.tiff_drv.CreateCopy( 'tmp/mfloat32.tif', src_ds,
                                  options = ['INTERLEAVE=PIXEL'] )
    src_ds = None

    ds = gdal.Open( 'tmp/mfloat32.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    err = ds.BuildOverviews( 'GAUSS', overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    #if ds.GetRasterBand(1).GetOverview(0).Checksum() != 1225:
    #    gdaltest.post_reason( 'bad checksum' )
    #    return 'fail'

    ds = None

    return 'success'

###############################################################################
# Check gauss resampling on a dataset with a raster band that has a color table

def tiff_ovr_14():

    shutil.copyfile( 'data/test_average_palette.tif', 'tmp/test_gauss_palette.tif' )

    ds = gdal.Open('tmp/test_gauss_palette.tif', gdal.GA_Update)

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds.BuildOverviews( 'GAUSS', overviewlist = [2] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Same as tiff_ovr_11 but with gauss, and compression to trigger the multiband overview
# code

def tiff_ovr_15():

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)
    
    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr15.tif', src_ds, options = [ 'COMPRESS=DEFLATE' ] )
    src_ds = None

    md = gdaltest.tiff_drv.GetMetadata()
    try:
        if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1 or \
           int(gdal.VersionInfo('VERSION_NUM')) < 1700:
            # The two following lines are necessary with inverted endianness
            # for the moment with older libtiff
            # See http://bugzilla.maptools.org/show_bug.cgi?id=1924 for more details
            ds = None
            ds = gdal.Open('tmp/ovr15.tif', gdal.GA_Update)
    except:
    # OG-python bindings don't have gdal.VersionInfo. Too bad, but let's hope that GDAL's version isn't too old !
        pass 


    ds.BuildOverviews( 'GAUSS', overviewlist = [2] )

    ds = None
    ds = gdal.Open('tmp/ovr15.tif', gdal.GA_ReadOnly)

    if ds is None:
        gdaltest.post_reason( 'Failed to open copy of test dataset.' )
        return 'fail'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2954
    exp_cs = 2987

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'


###############################################################################
# Test mode resampling on non-byte dataset

def tiff_ovr_16():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('data/mfloat32.vrt')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdaltest.tiff_drv.CreateCopy( 'tmp/ovr16.tif', src_ds,
                                  options = ['INTERLEAVE=PIXEL'] )
    src_ds = None

    ds = gdal.Open( 'tmp/ovr16.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    err = ds.BuildOverviews( 'MODE', overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    if cs != exp_cs:
        gdaltest.post_reason( 'bad checksum' )
        print(exp_cs, cs)
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test mode resampling on a byte dataset

def tiff_ovr_17():

    shutil.copyfile( 'data/byte.tif', 'tmp/ovr17.tif' )

    ds = gdal.Open( 'tmp/ovr17.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    err = ds.BuildOverviews( 'MODE', overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    if cs != exp_cs:
        gdaltest.post_reason( 'bad checksum' )
        print(exp_cs, cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Check mode resampling on a dataset with a raster band that has a color table

def tiff_ovr_18():

    shutil.copyfile( 'data/test_average_palette.tif', 'tmp/ovr18.tif' )

    ds = gdal.Open('tmp/ovr18.tif', gdal.GA_Update)

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    ds.BuildOverviews( 'MODE', overviewlist = [2] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 100

    ds = None

    if cs != exp_cs:
        gdaltest.post_reason( 'got wrong overview checksum.' )
        print(exp_cs, cs)
        return 'fail'

    return 'success'

###############################################################################
# Check that we can create overviews on a newly create file (#2621)
# Will cause older libtiff versions (<=3.8.2 for sure) to crash, so skip it
# if BigTIFF is not supported (this is a sign of an older libtiff...)

def tiff_ovr_19():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds=gdal.GetDriverByName('GTiff').Create('tmp/ovr19.tif',100,100,1)
    ds.GetRasterBand(1).Fill(1)

    # The flush is important to simulate the behaviour that wash it by #2621
    ds.FlushCache()
    ds.BuildOverviews('NEAR', overviewlist = [2])
    ds.FlushCache()
    ds.BuildOverviews('NEAR', overviewlist = [2,4])

    if ds.GetRasterBand(1).GetOverviewCount() != 2 is None:
        print('Overview could not be generated')
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 2500:
        print(cs)
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs != 625:
        print(cs)
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test BIGTIFF_OVERVIEW=YES option

def tiff_ovr_20():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/ovr20.tif', 100, 100, 1 )
    ds = None

    ds = gdal.Open( 'tmp/ovr20.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'YES')
    err = ds.BuildOverviews( 'NEAREST', overviewlist = [2, 4] )
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'IF_NEEDED')

    ds = None

    fileobj = open( 'tmp/ovr20.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'


###############################################################################
# Test BIGTIFF_OVERVIEW=IF_NEEDED option

def tiff_ovr_21():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/ovr21.tif', 170000, 100000, 1, options = ['SPARSE_OK=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/ovr21.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    err = ds.BuildOverviews( 'NONE', overviewlist = [2] )

    ds = None

    fileobj = open( 'tmp/ovr21.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Test BIGTIFF_OVERVIEW=NO option when BigTIFF is really needed

def tiff_ovr_22():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/ovr22.tif', 170000, 100000, 1, options = ['SPARSE_OK=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/ovr22.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'NO')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    err = ds.BuildOverviews( 'NONE', overviewlist = [2] )
    gdal.PopErrorHandler()
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'IF_NEEDED')

    ds = None

    if err != 0:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Same as before, but BigTIFF might be not needed as we use a compression
# method for the overviews.

def tiff_ovr_23():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/ovr23.tif', 170000, 100000, 1, options = ['SPARSE_OK=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/ovr23.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'NO')
    gdal.SetConfigOption('COMPRESS_OVERVIEW', 'DEFLATE')
    err = ds.BuildOverviews( 'NONE', overviewlist = [2] )
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'IF_NEEDED')
    gdal.SetConfigOption('COMPRESS_OVERVIEW', '')

    ds = None

    fileobj = open( 'tmp/ovr23.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check Classical TIFF signature
    if ((binvalues[2] != 0x2A or binvalues[3] != 0) \
        and (binvalues[3] != 0x2A or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Test BIGTIFF_OVERVIEW=IF_SAFER option

def tiff_ovr_24():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/ovr24.tif', 85000, 100000, 1, options = ['SPARSE_OK=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/ovr24.tif' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # 85 k * 100 k = 8.5 GB, so BigTIFF might be needed as
    # 8.5 GB / 2 > 4.2 GB
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'IF_SAFER')
    err = ds.BuildOverviews( 'NONE', overviewlist = [16] )
    gdal.SetConfigOption('BIGTIFF_OVERVIEW', 'IF_NEEDED')

    ds = None

    fileobj = open( 'tmp/ovr24.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Test creating overviews after some blocks have been written in the main
# band and actually flushed

def tiff_ovr_25():

    ds = gdaltest.tiff_drv.Create('tmp/ovr25.tif',100,100,1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist = [2])
    ds = None

    ds = gdal.Open('tmp/ovr25.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 10000:
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() == 0:
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(0).Checksum() != 2500:
        return 'fail'

    return 'success'

###############################################################################
# Test gdal.RegenerateOverview()

def tiff_ovr_26():

    try:
        x = gdal.RegenerateOverview
    except:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/ovr26.tif',100,100,1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist = [2])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs_new != 0:
        return 'fail'
    gdal.RegenerateOverview(ds.GetRasterBand(1), ds.GetRasterBand(1).GetOverview(0), 'NEAR')
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != cs_new:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test gdal.RegenerateOverviews()

def tiff_ovr_27():

    try:
        x = gdal.RegenerateOverviews
    except:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/ovr27.tif',100,100,1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist = [2, 4])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    ds.GetRasterBand(1).GetOverview(1).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs_new != 0 or cs2_new != 0:
        return 'fail'
    gdal.RegenerateOverviews(ds.GetRasterBand(1), [ds.GetRasterBand(1).GetOverview(0), ds.GetRasterBand(1).GetOverview(1)], 'NEAR')
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs != cs_new:
        return 'fail'
    if cs2 != cs2_new:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test cleaning overviews.

def tiff_ovr_28():

    ds = gdal.Open( 'tmp/ovr25.tif', gdal.GA_Update )
    if ds.BuildOverviews( overviewlist = [] ) != 0:
        gdaltest.post_reason( 'BuildOverviews() returned error code.' )
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason( 'Overview(s) appear to still exist.' )
        return 'fail'

    # Close and reopen to confirm they are really gone.
    ds = None
    ds = gdal.Open( 'tmp/ovr25.tif' )
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason( 'Overview(s) appear to still exist after reopen.')
        return 'fail'
    
    return 'success'

###############################################################################
# Test cleaning external overviews (ovr) on a non-TIFF format.

def tiff_ovr_29():

    src_ds = gdal.Open('data/byte.tif') 
    png_ds = gdal.GetDriverByName('PNG').CreateCopy( 'tmp/ovr29.png', src_ds )
    src_ds = None

    png_ds.BuildOverviews( overviewlist = [2] )
    png_ds = None

    if open('tmp/ovr29.png.ovr') is None:
        gdaltest.post_reason( 'Did not expected .ovr file.' )
        return 'fail'

    png_ds = gdal.Open( 'tmp/ovr29.png' )

    if png_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not find overview' )
        return 'fail'
        
    png_ds.BuildOverviews( overviewlist = [] )
    if png_ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason( 'delete overview failed.' )
        return 'fail'

    png_ds = None
    png_ds = gdal.Open( 'tmp/ovr29.png' )

    if png_ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason( 'delete overview failed.' )
        return 'fail'
        
    png_ds = None

    try:
        open('tmp/ovr29.png.ovr')
        gdaltest.post_reason( '.ovr file still present' )
        return 'fail'
    except:
        pass

    gdal.GetDriverByName('PNG').Delete('tmp/ovr29.png')
    
    return 'success'

###############################################################################
# Test fix for #2988.

def tiff_ovr_30():

    ds = gdaltest.tiff_drv.Create('tmp/ovr30.tif', 20, 20, 1)
    ds.BuildOverviews( overviewlist = [2])
    ds = None

    ds = gdal.Open('tmp/ovr30.tif', gdal.GA_Update)
    dict = {}
    dict['TEST_KEY'] = 'TestValue'
    ds.SetMetadata(dict)
    ds = None

    ds = gdaltest.tiff_drv.Create('tmp/ovr30.tif', 20, 20, 1)
    ds.BuildOverviews( overviewlist = [2])
    ds = None

    ds = gdal.Open('tmp/ovr30.tif', gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/ovr30.tif')
    if ds.GetProjectionRef().find('4326') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #3033

def tiff_ovr_31():

    ds = gdaltest.tiff_drv.Create('tmp/ovr31.tif', 100, 100, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(255)
    ds.GetRasterBand(3).Fill(255)
    ds.GetRasterBand(4).Fill(255)
    ds.BuildOverviews( 'average', overviewlist = [2, 4])
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    expected_cs = 7646
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d' % (cs, expected_cs))
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test Cubic sampling.

def tiff_ovr_32():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    # 4 regular band
    shutil.copyfile( 'data/stefan_full_rgba_photometric_rgb.tif', 'tmp/ovr32.tif' )

    ds = gdal.Open( 'tmp/ovr32.tif', gdal.GA_Update )
    ds.BuildOverviews( 'cubic', overviewlist = [2,5] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr32.tif' )
    
    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open( 'data/stefan_full_rgba_photometric_rgb.tif' )

    tmp_ds = gdal.GetDriverByName('GTiff').Create( '/vsimem/ovr32_float.tif', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, gdal.GDT_Float32)
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type = gdal.GDT_Byte)
    tmp_ds.BuildOverviews( 'cubic', overviewlist = [2] )

    tmp2_ds = gdal.GetDriverByName('GTiff').Create( '/vsimem/ovr32_byte.tif', tmp_ds.RasterXSize, tmp_ds.RasterYSize, tmp_ds.RasterCount)
    tmp2_ds.BuildOverviews( 'NONE', overviewlist = [2] )
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type = gdal.GDT_Byte)
    tmp2_ovr_ds.WriteRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data)

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdaltest.tiff_drv.Delete( '/vsimem/ovr32_float.tif' )
    gdaltest.tiff_drv.Delete( '/vsimem/ovr32_byte.tif' )
    
    # Test GDALRegenerateOverviewsMultiBand
    shutil.copyfile( 'data/stefan_full_rgba_photometric_rgb.tif', 'tmp/ovr32.tif' )

    ds = gdal.Open( 'tmp/ovr32.tif' )
    gdal.SetConfigOption('COMPRESS_OVERVIEW', 'DEFLATE')
    gdal.SetConfigOption('INTERLEAVE_OVERVIEW', 'PIXEL')
    ds.BuildOverviews( 'cubic', overviewlist = [2,5] )
    gdal.SetConfigOption('COMPRESS_OVERVIEW', None)
    gdal.SetConfigOption('INTERLEAVE_OVERVIEW', None)

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr32.tif' )
    

    # 3 bands + alpha
    shutil.copyfile( 'data/stefan_full_rgba.tif', 'tmp/ovr32.tif' )

    ds = gdal.Open( 'tmp/ovr32.tif', gdal.GA_Update )
    ds.BuildOverviews( 'cubic', overviewlist = [2,5] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21656
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 2132
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr32.tif' )
    
    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open( 'data/stefan_full_rgba.tif' )

    tmp_ds = gdal.GetDriverByName('GTiff').Create( '/vsimem/ovr32_float.tif', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, gdal.GDT_Float32)
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type = gdal.GDT_Byte)
    tmp_ds.BuildOverviews( 'cubic', overviewlist = [2] )

    tmp2_ds = gdal.GetDriverByName('GTiff').Create( '/vsimem/ovr32_byte.tif', tmp_ds.RasterXSize, tmp_ds.RasterYSize, tmp_ds.RasterCount)
    tmp2_ds.BuildOverviews( 'NONE', overviewlist = [2] )
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type = gdal.GDT_Byte)
    tmp2_ovr_ds.WriteRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data)

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    #expected_cs = 21656
    expected_cs = 21168
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdaltest.tiff_drv.Delete( '/vsimem/ovr32_float.tif' )
    gdaltest.tiff_drv.Delete( '/vsimem/ovr32_byte.tif' )
    
    # Same test with a compressed dataset
    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/ovr32.tif', src_ds, options = ['COMPRESS=DEFLATE'])
    ds.BuildOverviews( 'cubic', overviewlist = [2,5] )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21656
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 2132
    if cs != expected_cs:
        gdaltest.post_reason('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr32.tif' )

    return 'success'


###############################################################################
# Test creation of overviews on a 1x1 dataset (fix for #3069)

def tiff_ovr_33():

    try:
        os.remove('tmp/ovr33.tif.ovr')
    except:
        pass

    ds = gdaltest.tiff_drv.Create('tmp/ovr33.tif', 1, 1, 1)
    ds = None
    ds = gdal.Open('tmp/ovr33.tif')
    ds.BuildOverviews('NEAREST', overviewlist = [2, 4])
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr33.tif' )

    return 'success'


###############################################################################
# Confirm that overviews are used on a Band.RasterIO().

def tiff_ovr_34():

    ds_in = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr34.tif', ds_in)
    ds.BuildOverviews('NEAREST', overviewlist = [2] )
    ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
    ds = None
    ds_in = None
    
    ds = gdal.Open('tmp/ovr34.tif')
    data = ds.GetRasterBand(1).ReadRaster( 0,0,20,20,buf_xsize=5,buf_ysize=5)
    ds = None

    if data != '                         '.encode('ascii'):
        gdaltest.post_reason( 'did not get expected cleared overview.' )
        print('[%s]' % data)
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/ovr34.tif' )

    return 'success'

###############################################################################
# Confirm that overviews are used on a Band.RasterIO().

def tiff_ovr_35():

    ds_in = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr35.tif', ds_in)
    ds.BuildOverviews('NEAREST', overviewlist = [2] )
    ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
    ds = None
    ds_in = None
    
    ds = gdal.Open('tmp/ovr35.tif')
    data = ds.ReadRaster( 0,0,20,20,buf_xsize=5,buf_ysize=5,band_list=[1])
    ds = None

    if data != '                         '.encode('ascii'):
        gdaltest.post_reason( 'did not get expected cleared overview.' )
        print('[%s]' % data)
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/ovr35.tif' )

    return 'success'

###############################################################################
# Confirm that overviews are used on a Band.RasterIO() when using BlockBasedRasterIO() (#3124)

def tiff_ovr_36():

    oldval = gdal.GetConfigOption('GDAL_FORCE_CACHING', 'NO')
    gdal.SetConfigOption('GDAL_FORCE_CACHING', 'YES')
    
    ret = tiff_ovr_35()
    
    gdal.SetConfigOption('GDAL_FORCE_CACHING', oldval)
    
    return ret

###############################################################################
# Test PREDICTOR_OVERVIEW=2 option. (#3414)

def tiff_ovr_37():

    shutil.copy( '../gdrivers/data/n43.dt0', 'tmp/ovr37.dt0')

    ds = gdal.Open( 'tmp/ovr37.dt0' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    gdal.SetConfigOption('PREDICTOR_OVERVIEW', '2')
    gdal.SetConfigOption('COMPRESS_OVERVIEW', 'LZW')
    err = ds.BuildOverviews( 'NEAR', overviewlist = [2] )
    gdal.SetConfigOption('PREDICTOR_OVERVIEW', None)
    gdal.SetConfigOption('COMPRESS_OVERVIEW', None)

    ds = None

    ds = gdal.Open( 'tmp/ovr37.dt0' )
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 45378:
        print(cs)
        gdaltest.post_reason( 'got wrong overview checksum.' )
        return 'fail'
    ds = None

    predictor2_size = os.stat('tmp/ovr37.dt0.ovr')[stat.ST_SIZE]
    # 4131 : on little-endian host
    # XXXX : on big-endian host ??? FIXME: To be updated
    if predictor2_size != 4131:
        print(predictor2_size)
        gdaltest.post_reason( 'did not get expected file size.' )
        return 'fail'

    return 'success'

###############################################################################
# Test that the predictor flag gets well propagated to internal overviews

def tiff_ovr_38():

    # Skip with old libtiff (crash with 3.8.2)
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr38.tif', src_ds, options = ['COMPRESS=LZW', 'PREDICTOR=2'])
    ds.BuildOverviews( overviewlist = [2, 4])
    ds = None

    file_size = os.stat('tmp/ovr38.tif')[stat.ST_SIZE]

    if file_size > 21000:
        print(file_size)
        gdaltest.post_reason( 'did not get expected file size.' )
        return 'fail'

    return 'success'

###############################################################################
# Test external overviews on all datatypes

def tiff_ovr_39():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    data = src_ds.GetRasterBand(1).ReadRaster(0,0,20,20)
    src_ds = None

    for datatype in [gdal.GDT_Byte,
                     gdal.GDT_Int16,
                     gdal.GDT_UInt16,
                     gdal.GDT_Int32,
                     gdal.GDT_UInt32,
                     gdal.GDT_Float32,
                     gdal.GDT_Float64,
                     gdal.GDT_CInt16,
                     gdal.GDT_CInt32,
                     gdal.GDT_CFloat32,
                     gdal.GDT_CFloat64]:

        gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' data/byte.tif tmp/ovr39.tif -ot ' + gdal.GetDataTypeName(datatype))

        try:
            os.remove('tmp/ovr39.tif.ovr')
        except:
            pass

        ds = gdal.Open('tmp/ovr39.tif')
        ds.BuildOverviews('NEAREST', overviewlist = [2])
        ds = None

        ds = gdal.Open('tmp/ovr39.tif.ovr')
        ovr_datatype = ds.GetRasterBand(1).DataType
        ds = None

        if datatype != ovr_datatype:
            gdaltest.post_reason('did not get expected datatype')
            return 'fail'

        ds = gdal.Open('tmp/ovr39.tif')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        ds = None

        if gdal.DataTypeIsComplex(datatype):
            expected_cs = 1171
        else:
            expected_cs = 1087

        if cs != expected_cs:
            gdaltest.post_reason('did not get expected checksum for datatype %s' % gdal.GetDataTypeName(datatype))
            return 'fail'

    return 'success'

###############################################################################
# Test external overviews on 1 bit datasets with AVERAGE_BIT2GRAYSCALE (similar to tiff_ovr_4)

def tiff_ovr_40():

    shutil.copyfile( 'data/oddsize_1bit2b.tif', 'tmp/ovr40.tif' )

    wrk_ds = gdal.Open('tmp/ovr40.tif')

    if wrk_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    wrk_ds.BuildOverviews( 'AVERAGE_BIT2GRAYSCALE', overviewlist = [2,4] )
    wrk_ds = None

    wrk_ds = gdal.Open('tmp/ovr40.tif')

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    if 'RESAMPLING' not in md \
       or md['RESAMPLING'] != 'AVERAGE_BIT2GRAYSCALE':
        gdaltest.post_reason( 'Did not get expected RESAMPLING metadata.' )
        return 'fail'

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0,0,ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    is_bytes = False
    try:
        if (isinstance(ovimage, bytes) and not isinstance(ovimage, str)):
            is_bytes = True
    except:
        pass

    if is_bytes is True:
        for i in range(pix_count):
            sum = sum + ovimage[i]
    else:
        for i in range(pix_count):
            sum = sum + ord(ovimage[i])
            
    average = sum / pix_count
    exp_average = 154.8144
    if abs(average - exp_average) > 0.1:
        print(average)
        gdaltest.post_reason( 'got wrong average for overview image' )
        return 'fail'

    # read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(0,0,frband.XSize,frband.YSize,
                                ovband.XSize,ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    sum = 0.0
    if is_bytes is True:
        for i in range(pix_count):
            sum = sum + ovimage[i]
    else:
        for i in range(pix_count):
            sum = sum + ord(ovimage[i])
    average = sum / pix_count
    exp_average = 0.6096
    
    if abs(average - exp_average) > 0.01:
        print(average)
        gdaltest.post_reason( 'got wrong average for downsampled image' )
        return 'fail'

    wrk_ds = None

    return 'success'

###############################################################################
# Test external overviews on 1 bit datasets with NEAREST

def tiff_ovr_41():

    shutil.copyfile( 'data/oddsize_1bit2b.tif', 'tmp/ovr41.tif' )

    ds = gdal.Open('tmp/ovr41.tif')
    #data = wrk_ds.GetRasterBand(1).ReadRaster(0,0,99,99,50,50)
    ds.BuildOverviews( 'NEAREST', overviewlist = [2] )
    ds = None

    #ds = gdaltest.tiff_drv.Create('tmp/ovr41.tif.handmade.ovr',50,50,1,options=['NBITS=1'])
    #ds.GetRasterBand(1).WriteRaster(0,0,50,50,data)
    #ds = None

    ds = gdal.Open('tmp/ovr41.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    if cs != 1496:
        print(cs)
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test external overviews on dataset with color table

def tiff_ovr_42():

    ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255)]

    ct = gdal.ColorTable()
    for i in range(len(ct_data)):
        ct.SetColorEntry( i, ct_data[i] )

    ds = gdaltest.tiff_drv.Create('tmp/ovr42.tif',1,1)
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds = None

    ds = gdal.Open('tmp/ovr42.tif')
    ds.BuildOverviews('NEAREST', overviewlist = [2] )
    ds = None

    ds = gdal.Open('tmp/ovr42.tif.ovr')
    ct2 = ds.GetRasterBand(1).GetRasterColorTable()

    if ct2.GetCount() != 256 or \
       ct2.GetColorEntry(0) != (255,0,0,255) or \
       ct2.GetColorEntry(1) != (0,255,0,255) or \
       ct2.GetColorEntry(2) != (0,0,255,255) or \
       ct2.GetColorEntry(3) != (255,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Make sure that 16bit overviews with jpeg compression are handled using 12bit jpeg-in-tiff (#3539) 

def tiff_ovr_43():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    old_accum = gdal.GetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'OFF' )
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'ON' )
    gdal.ErrorReset()
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        
    try:
        ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
        ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    except:
        ds = None

    gdal.PopErrorHandler()
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', old_accum )
    
    if gdal.GetLastErrorMsg().find(
                   'Unsupported JPEG data precision 12') != -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/ovr43.tif', 16, 16, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(4000)
    ds = None

    try:
        os.remove('tmp/ovr43.tif.ovr')
    except:
        pass

    ds = gdal.Open('tmp/ovr43.tif')
    gdal.SetConfigOption('COMPRESS_OVERVIEW','JPEG')
    ds.BuildOverviews('NEAREST', overviewlist = [2])
    ds = None
    gdal.SetConfigOption('COMPRESS_OVERVIEW',None)

    ds = gdal.Open('tmp/ovr43.tif.ovr')
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if not 'NBITS' in md or md['NBITS'] != '12':
        print(md)
        gdaltest.post_reason('did not get expected NBITS')
        return 'fail'

    if cs != 354:
        print(cs)
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/ovr43.tif' )

    return 'success'

###############################################################################
# Test that we can change overview block size through GDAL_TIFF_OVR_BLOCKSIZE configuration
# option

def tiff_ovr_44():

    shutil.copyfile( 'data/byte.tif', 'tmp/ovr44.tif' )
    gdal.SetConfigOption('GDAL_TIFF_OVR_BLOCKSIZE', '256')
    ds = gdal.Open('tmp/ovr44.tif', gdal.GA_Update)
    ds.BuildOverviews( overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GDAL_TIFF_OVR_BLOCKSIZE', None)

    ds = gdal.Open('tmp/ovr44.tif')
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        if blockx != 256 or blocky != 256:
            gdaltest.post_reason('did not get expected block size')
            return 'fail'
    cs = ovr_band.Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr44.tif' )

    if cs != 1087:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Same as tiff_ovr_44, but with external overviews

def tiff_ovr_45():

    shutil.copyfile( 'data/byte.tif', 'tmp/ovr45.tif' )
    gdal.SetConfigOption('GDAL_TIFF_OVR_BLOCKSIZE', '256')
    ds = gdal.Open('tmp/ovr45.tif', gdal.GA_ReadOnly)
    ds.BuildOverviews( overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GDAL_TIFF_OVR_BLOCKSIZE', None)

    ds = gdal.Open('tmp/ovr45.tif.ovr')
    ovr_band = ds.GetRasterBand(1)
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        if blockx != 256 or blocky != 256:
            gdaltest.post_reason('did not get expected block size')
            return 'fail'
    cs = ovr_band.Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ovr45.tif' )

    if cs != 1087:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test overview on a dataset where width * height > 2 billion

def tiff_ovr_46():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if not gdaltest.run_slow_tests():
        return 'skip'

    # Test NEAREST
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'NEAREST', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')


    # Test AVERAGE in optimized case (x2 reduction)
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test AVERAGE in un-optimized case (x3 reduction)
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'AVERAGE', overviewlist = [3] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test AVERAGE in un-optimized case (color table)
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,0,0) )
    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')


    # Test GAUSS
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'GAUSS', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test GAUSS with color table
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,0,0) )
    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ds.BuildOverviews( 'GAUSS', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test MODE
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'MODE', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')


    # Test CUBIC
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options = ['SPARSE_OK=YES'])
    ds.BuildOverviews( 'CUBIC', overviewlist = [2] )
    ds = None
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    return 'success'

###############################################################################
# Test workaround with libtiff 3.X when creating interleaved overviews

def tiff_ovr_47():
    mem_drv = gdal.GetDriverByName('MEM')
    mem_ds = mem_drv.Create('', 852, 549, 3)

    for i in range(1, 4):
        band = mem_ds.GetRasterBand(i)
        band.Fill(128)

    driver = gdal.GetDriverByName("GTIFF")
    out_ds = driver.CreateCopy("/vsimem/tiff_ovr_47.tif", mem_ds)
    mem_ds = None
    out_ds.BuildOverviews("NEAREST", [ 2, 4, 8, 16])
    out_ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_47.tif")
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    gdal.Unlink("/vsimem/tiff_ovr_47.tif")
    
    if cs != 35721:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test that we don't average 0's in alpha band

def tiff_ovr_48():
    
    shutil.copy('data/rgba_with_alpha_0_and_255.tif', 'tmp')
    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif')
    ds.BuildOverviews('AVERAGE', [2])
    ds = None
    
    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif.ovr')
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != 3:
            print(i)
            return 'fail'

    return 'success'

###############################################################################
# Test possible stride computation issue in GDALRegenerateOverviewsMultiBand (#5653)

def tiff_ovr_49():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_ovr_49.tif', 1023, 1023, 1)
    ds.GetRasterBand(1).Fill(0)
    c = '\xFF'
    # Fails on 1.11.1 with col = 255 or col = 1019
    col = 1019
    ds.GetRasterBand(1).WriteRaster(col,0,1,1023,c,1,1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_ovr_49.tif')
    gdal.SetConfigOption('COMPRESS_OVERVIEW', 'DEFLATE')
    ds.BuildOverviews( 'AVERAGE', overviewlist = [2] )
    gdal.SetConfigOption('COMPRESS_OVERVIEW', None)
    ds = None
    ds = gdal.Open('/vsimem/tiff_ovr_49.tif.ovr')
    if ds.GetRasterBand(1).Checksum() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_49.tif')

    return 'success'

###############################################################################
# Cleanup

def tiff_ovr_cleanup():
    gdaltest.tiff_drv.Delete( 'tmp/mfloat32.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr4.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr5.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr6.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/test_average_palette.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr9.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr10.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr11.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr12.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/test_gauss_palette.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr15.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr16.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr17.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr18.tif' )
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') != -1:
        gdaltest.tiff_drv.Delete( 'tmp/ovr19.tif' )
        gdaltest.tiff_drv.Delete( 'tmp/ovr20.tif' )
        gdaltest.tiff_drv.Delete( 'tmp/ovr21.tif' )
        gdaltest.tiff_drv.Delete( 'tmp/ovr22.tif' )
        gdaltest.tiff_drv.Delete( 'tmp/ovr23.tif' )
        gdaltest.tiff_drv.Delete( 'tmp/ovr24.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr25.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr26.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr27.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr30.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr31.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr37.dt0' )
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') != -1:
        gdaltest.tiff_drv.Delete( 'tmp/ovr38.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr39.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr40.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr41.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ovr42.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/rgba_with_alpha_0_and_255.tif' )
    gdaltest.tiff_drv = None

    return 'success'

gdaltest_list_internal = [
    tiff_ovr_1,
    tiff_ovr_2,
    tiff_ovr_3,
    tiff_ovr_3bis,
    tiff_ovr_4,
    tiff_ovr_5,
    tiff_ovr_6,
    tiff_ovr_7,
    tiff_ovr_8,
    tiff_ovr_9,
    tiff_ovr_10,
    tiff_ovr_11,
    tiff_ovr_12,
    tiff_ovr_13,
    tiff_ovr_14,
    tiff_ovr_15,
    tiff_ovr_16,
    tiff_ovr_17,
    tiff_ovr_18,
    tiff_ovr_19,
    tiff_ovr_20,
    tiff_ovr_21,
    tiff_ovr_22,
    tiff_ovr_23,
    tiff_ovr_24,
    tiff_ovr_25,
    tiff_ovr_26,
    tiff_ovr_27,
    tiff_ovr_28,
    tiff_ovr_29,
    tiff_ovr_30,
    tiff_ovr_31,
    tiff_ovr_32,
    tiff_ovr_33,
    tiff_ovr_34,
    tiff_ovr_35,
    tiff_ovr_36,
    tiff_ovr_37,
    tiff_ovr_38,
    tiff_ovr_39,
    tiff_ovr_40,
    tiff_ovr_41,
    tiff_ovr_42,
    tiff_ovr_43,
    tiff_ovr_44,
    tiff_ovr_45,
    tiff_ovr_46,
    tiff_ovr_47,
    tiff_ovr_48,
    tiff_ovr_49,
    tiff_ovr_cleanup ]

def tiff_ovr_invert_endianness():
    gdaltest.tiff_endianness = gdal.GetConfigOption( 'GDAL_TIFF_ENDIANNESS', "NATIVE" )
    gdal.SetConfigOption( 'GDAL_TIFF_ENDIANNESS', 'INVERTED' )
    return 'success'

def tiff_ovr_restore_endianness():
    gdal.SetConfigOption( 'GDAL_TIFF_ENDIANNESS', gdaltest.tiff_endianness )
    return 'success'

gdaltest_list = []
for item in gdaltest_list_internal:
    gdaltest_list.append(item)
gdaltest_list.append(tiff_ovr_invert_endianness)
for item in gdaltest_list_internal:
    if item.__name__ != 'tiff_ovr_46':
        gdaltest_list.append( (item, item.__name__ + '_inverted') )
gdaltest_list.append(tiff_ovr_restore_endianness)

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_ovr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

