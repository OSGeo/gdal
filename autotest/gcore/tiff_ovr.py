#!/usr/bin/env python
###############################################################################
# $Id: tiff_ovr.py,v 1.2 2006/10/27 04:31:51 fwarmerdam Exp $
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
# 
#  $Log: tiff_ovr.py,v $
#  Revision 1.2  2006/10/27 04:31:51  fwarmerdam
#  corrected licenses
#
#  Revision 1.1  2004/11/11 05:51:57  fwarmerdam
#  New
#
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Create a 3 band floating point GeoTIFF file so we can build overviews on it
# later.  Build overviews on it. 

def tiff_ovr_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('data/mfloat32.vrt')
    gdaltest.tiff_drv.CreateCopy( 'tmp/mfloat32.tif', src_ds )
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
# Cleanup

def tiff_ovr_cleanup():
    gdaltest.tiff_drv.Delete( 'tmp/mfloat32.tif' )
    gdaltest.tiff_drv = None

    return 'success'

gdaltest_list = [
    tiff_ovr_1,
    tiff_ovr_2,
    tiff_ovr_3,
    tiff_ovr_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_ovr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

