#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GRIB driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import string
import array
import shutil
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Do a simple checksum on our test file (with a faked imagery.tif).

def grib_1():

    gdaltest.grib_drv = gdal.GetDriverByName('GRIB')
    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'ds.mint.bin', 2, 46927 )
    return tst.testOpen()


###############################################################################
# Test a small GRIB 1 sample file.

def grib_2():

    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'Sample_QuikSCAT.grb', 4, 50714 )
    return tst.testOpen()

###############################################################################
# This file has different raster sizes for some of the products, which
# we sort-of-support per ticket Test a small GRIB 1 sample file.

def grib_3():

    if gdaltest.grib_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'GRIB', 'bug3246.grb', 4, 4081 )
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    msg = gdal.GetLastErrorMsg()
    if msg.find('data access may be incomplete') == -1 \
       or gdal.GetLastErrorType() != 2:
        gdaltest.post_reason( 'did not get expected warning.' )
    
    return result

###############################################################################
# Check nodata

def grib_4():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/ds.mint.bin')
    if ds.GetRasterBand(1).GetNoDataValue() != 9999:
        return 'fail'

    return 'success'

###############################################################################
# Check grib units (#3606)

def grib_5():

    if gdaltest.grib_drv is None:
        return 'skip'

    try:
        os.unlink('tmp/ds.mint.bin.aux.xml')
    except:
        pass

    shutil.copy('data/ds.mint.bin', 'tmp/ds.mint.bin')
    ds = gdal.Open('tmp/ds.mint.bin')
    md = ds.GetRasterBand(1).GetMetadata()
    if md['GRIB_UNIT'] != '[C]' or md['GRIB_COMMENT'] != 'Minimum Temperature [C]':
        gdaltest.post_reason('fail')
        print(md)
        return 'success'
    ds.GetRasterBand(1).ComputeStatistics(False)
    if abs(ds.GetRasterBand(1).GetMinimum() - 13) > 1:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'success'
    ds = None

    os.unlink('tmp/ds.mint.bin.aux.xml')

    gdal.SetConfigOption('GRIB_NORMALIZE_UNITS', 'NO')
    ds = gdal.Open('tmp/ds.mint.bin')
    gdal.SetConfigOption('GRIB_NORMALIZE_UNITS', None)
    md = ds.GetRasterBand(1).GetMetadata()
    if md['GRIB_UNIT'] != '[K]' or md['GRIB_COMMENT'] != 'Minimum Temperature [K]':
        gdaltest.post_reason('fail')
        print(md)
        return 'success'
    ds.GetRasterBand(1).ComputeStatistics(False)
    if abs(ds.GetRasterBand(1).GetMinimum() - 286) > 1:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'success'
    ds = None

    gdal.GetDriverByName('GRIB').Delete('tmp/ds.mint.bin')

    return 'success'

###############################################################################
# Handle geotransform for 1xn or nx1 grids.  The geotransform was faulty when
# grib files had one cell in either direction for geographic projections.  See
# ticket #5532

def grib_6():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/one_one.grib2')
    egt = (245.750, 0.5, 0.0, 47.250, 0.0, -0.5)
    gt = ds.GetGeoTransform()
    ds = None
    if gt != egt:
        print(gt, '!=', egt)
        gdaltest.post_reason('Invalid geotransform')
        return 'fail'
    return 'success'

###############################################################################
# This is more a /vsizip/ file test than a GRIB one, but couldn't easily
# come up with a pure /vsizip/ test case, so here's a real world use case (#5530)

def grib_7():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Write PDS numbers to all bands

def grib_8():

    if gdaltest.grib_drv is None:
        return 'skip'
    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'Fail'
    band = ds.GetRasterBand(2)
    md = band.GetMetadataItem('GRIB_PDS_TEMPLATE_NUMBERS')
    ds = None
    if md is None:
        gdaltest.post_reason('Failed to fetch pds numbers (#5144)')
        return 'fail'

    gdal.SetConfigOption('GRIB_PDS_ALL_BANDS', 'OFF')
    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'
    band = ds.GetRasterBand(2)
    md = band.GetMetadataItem('GRIB_PDS_TEMPLATE_NUMBERS')
    ds = None

    if md is not None:
        gdaltest.post_reason('Got pds numbers, when disabled (#5144)')
        return 'fail'
    return 'success'

gdaltest_list = [
    grib_1,
    grib_2,
    grib_3,
    grib_4,
    grib_5,
    grib_6,
    grib_7,
    grib_8
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'grib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

